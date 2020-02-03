#include "stdafx.h"
#include <wchar.h>
#include <objidl.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;
#include "zip.h"

// Plugin version (don't touch this!)
#define GPPHDR_VER 0x10

// Plugin title
#define PLUGIN_NAME "CDG-Pro Plugin"

#define BDM_PALETTEINDEXZERO 0
#define BDM_TOPLEFTPIXEL 1
#define BDM_TOPRIGHTPIXEL 2
#define BDM_BOTTOMLEFTPIXEL 3
#define BDM_BOTTOMRIGHTPIXEL 4

// Preferences (TODO: INI file or UI).
int g_nBackgroundOpacity = 127;
bool g_bDrawOutline = true;
bool g_bShowBorder = false;
// How to determine the transparent background color?
int g_nBackgroundDetectionMode = BDM_TOPLEFTPIXEL;

// main structure with plugin information, version, name...
typedef struct {
	int version;             // version of the plugin structure
	const char* description; // name/title of the plugin 
	int(*init)();            // function which will be executed on init event
	void(*config)();         // function which will be executed on config event
	void(*quit)();           // function which will be executed on quit event
	HWND hwndParent;         // hwnd of the Winamp client main window (stored by Winamp when dll is loaded)
	HINSTANCE hDllInstance;  // hinstance of this plugin DLL. (stored by Winamp when dll is loaded) 
} winampGeneralPurposePlugin;

// The official size of the CDG graphics area.
#define CDG_WIDTH (300)
#define CDG_HEIGHT (216)

// The CDG graphics area is divided into a 50x18 grid of 6x12 pixel cells.
#define CDG_WIDTH_CELLS (50)
#define CDG_HEIGHT_CELLS (18)
#define CDG_CELL_WIDTH (6)
#define CDG_CELL_HEIGHT (12)

// The outer edges of this area are not shown on screen, but are managed in memory (for scrolling
// images into the visible area, etc). This is the size of the visible central portion.
#define CDG_CANVAS_X (CDG_CELL_WIDTH)
#define CDG_CANVAS_Y (CDG_CELL_HEIGHT)
#define CDG_CANVAS_WIDTH (288)
#define CDG_CANVAS_HEIGHT (192)

#define TOP_LEFT_PIXEL_OFFSET (((CDG_CELL_HEIGHT*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define TOP_RIGHT_PIXEL_OFFSET (TOP_LEFT_PIXEL_OFFSET+(CDG_CANVAS_WIDTH-1))
#define BOTTOM_LEFT_PIXEL_OFFSET (((((CDG_CELL_HEIGHT+CDG_CANVAS_HEIGHT)-1)*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define BOTTOM_RIGHT_PIXEL_OFFSET (BOTTOM_LEFT_PIXEL_OFFSET+(CDG_CANVAS_WIDTH-1))

// Each CDG frame is 1/300th of a second.
#define CDG_FRAME_DURATION_MS (1000.0/300.0)

// Forward offset.
#define HYSTERESIS_MS (100)
// Rewind tolerance. We periodically query WinAmp to find out the current position (in milliseconds)
// of the audio track. For some reason, usually very near the start of a track, WinAmp will report
// the position as very slightly earlier than the previous report indicated, which would normally
// make our plugin think that the user has rewound the track, and so we would go ahead and try to
// rebuild the CDG canvas from scratch. But if the amount that the track appears to have been
// rewound by is within this tolerance value, we will assume that this minor WinAmp glitch thing
// has happened, and will just not draw anything until the position advances.
#define REWIND_TOLERANCE_MS (100)
// Update screen approximately 30 FPS
#define SCREEN_REFRESH_MS (33)

// In Windows, bitmaps must have a width that is a multiple of 4 bytes.
// We are storing two bitmaps to represent the CDG display,
// one of which is 4 bits per pixel, and one which is 1 bit per pixel.
// Because of the 1bpp bitmap, this means that the bitmap width
// must be a multiple of 32 (32 bits = 4 bytes).
#define CDG_BITMAP_WIDTH (320)
#define CDG_BITMAP_HEIGHT CDG_HEIGHT

// This is the colour we will use for transparency. It is impossible to
// represent this as a 12-bit colour, and each value is a different distance from
// a multiple of 17, so it should never accidentally happen.
#define DEFAULT_TRANSPARENT_COLOR (RGB(145,67,219))

// WinAmp messages.
#define WM_WA_IPC WM_USER
#define IPC_PLAYING_FILEW 13003
#define IPC_CB_MISC 603
#define IPC_ISPLAYING 104
#define IPC_GETOUTPUTTIME 105
#define IPC_CB_MISC_STATUS 2

// The CDG instruction set.
#define CDG_INSTR_MEMORY_PRESET (1)
#define CDG_INSTR_BORDER_PRESET (2)
#define CDG_INSTR_TILE_BLOCK (6)
#define CDG_INSTR_SCROLL_PRESET (20)
#define CDG_INSTR_SCROLL_COPY (24)
#define CDG_INSTR_TRANSPARENT_COLOR (28)
#define CDG_INSTR_LOAD_COLOR_TABLE_LOW (30)
#define CDG_INSTR_LOAD_COLOR_TABLE_HIGH (31)
#define CDG_INSTR_TILE_BLOCK_XOR (38)

// The CDG data packet structure.
struct CDGPacket {
	BYTE command;
	BYTE instruction;
	BYTE unused1[2];
	BYTE data[16];
	BYTE unused2[4];
};

int init(void);
void config(void);
void quit(void);

// Window class names.
const WCHAR* g_foregroundWindowClassName = L"CDGProFG";
const WCHAR* g_backgroundWindowClassName = L"CDGProBG";
// The DC and bitmap containing the CDG graphics.
HDC g_hForegroundDC = NULL;
HBITMAP g_hForegroundBitmap = NULL;
BYTE* g_pForegroundBitmapBits = NULL;
// The DC and bitmap containing the background (usually 1 pixel that we stretch out).
HDC g_hBackgroundDC = NULL;
HBITMAP g_hBackgroundBitmap = NULL;
unsigned int* g_pBackgroundBitmapBits = NULL;
// The DC containing the mask for the CDG graphics, if we want to render a transparent background.
HDC g_hMaskDC = NULL;
HBITMAP g_hMaskBitmap = NULL;
// The DC containing the border mask for the CDG graphics, if we want to render a transparent background.
HDC g_hBorderMaskDC = NULL;
HBITMAP g_hBorderMaskBitmap = NULL;
BYTE* g_pBorderMaskBitmapBits = NULL;
// The DC containing the mask for the CDG graphics, if we want to render a transparent background.
HDC g_hMaskedForegroundDC = NULL;
HBITMAP g_hMaskedForegroundBitmap = NULL;
// The scroll buffer DC and bitmap.
HDC g_hScrollBufferDC = NULL;
HBITMAP g_hScrollBufferBitmap = NULL;
BYTE* g_pScrollBufferBitmapBits = NULL;
// The window (and it's DC) containing the (optionally transparent) background.
HDC g_hBackgroundWindowDC = NULL;
HWND g_hBackgroundWindow = NULL;
// The window (and it's DC) containing the foreground.
HDC g_hForegroundWindowDC = NULL;
HWND g_hForegroundWindow = NULL;
// Original WndProc that we have to swap back in at the end of proceedings.
WNDPROC g_pOriginalWndProc;
// Brush filled with the transparency color.
HBRUSH g_hTransparentBrush;
// Current CDG data.
CDGPacket* g_pCDGData = NULL;
int g_nCDGPackets = 0;
int g_nCDGPC = 0;
// This structure contains plugin information, version, name...
winampGeneralPurposePlugin plugin = { GPPHDR_VER,PLUGIN_NAME,init,config,quit,0,0 };
// We keep track of the last "reset" color. If we receive a MemoryPreset command for this color
// again before anything else has been drawn, we can ignore it.
BYTE g_nLastMemoryPresetColor = -1;
// Handle and ID of the processing thread, as well as an event for stopping it.
HANDLE g_hCDGProcessingThread = NULL;
DWORD g_nCDGProcessingThreadID = 0;
// Cross thread communication events
HANDLE g_hStopCDGProcessingEvent = NULL;
HANDLE g_hStoppedCDGProcessingEvent = NULL;
HANDLE g_hStopCDGThreadEvent = NULL;
HANDLE g_hSongLoadedEvent = NULL;
// Canvas pixel offsets for scrolling
int g_nCanvasXOffset = 0;
int g_nCanvasYOffset = 0;
// The palettes ... logical is the one defined by the CDG data, effective is the one we're using.
RGBQUAD g_logicalPalette[16];
RGBQUAD g_effectivePalette[16];



// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &plugin;
}

void clearExistingCDGData() {
	if (g_pCDGData) {
		free(g_pCDGData);
		g_pCDGData = NULL;
	}
	g_nCDGPackets = 0;
}

void SetBackgroundColorIndex(byte index) {
	// RGB macro, for some reason, encodes as BGR. Not so handy for direct 32-bit bitmap writing.
	COLORREF backgroundColorReversed = RGB(g_effectivePalette[index].rgbBlue, g_effectivePalette[index].rgbGreen, g_effectivePalette[index].rgbRed);
	*g_pBackgroundBitmapBits = backgroundColorReversed;
	COLORREF backgroundColor = RGB(g_effectivePalette[index].rgbRed, g_effectivePalette[index].rgbRed, g_effectivePalette[index].rgbBlue);
	::SetBkColor(g_hForegroundDC, backgroundColor);
}

void SetBackgroundColorFromPixel(int offset, bool highNibble) {
	byte color = (g_pForegroundBitmapBits[offset] >> (highNibble ? 4 : 0)) & 0x0F;
	SetBackgroundColorIndex(color);
}

void SetBackgroundColorFromTopLeftPixel() {
	SetBackgroundColorFromPixel(TOP_LEFT_PIXEL_OFFSET, true);
}

void SetBackgroundColorFromTopRightPixel() {
	SetBackgroundColorFromPixel(TOP_RIGHT_PIXEL_OFFSET, false);
}

void SetBackgroundColorFromBottomLeftPixel() {
	SetBackgroundColorFromPixel(BOTTOM_LEFT_PIXEL_OFFSET, true);
}

void SetBackgroundColorFromBottomRightPixel() {
	SetBackgroundColorFromPixel(BOTTOM_RIGHT_PIXEL_OFFSET, false);
}

byte MemoryPreset(BYTE color, BYTE repeat) {
	if (g_nLastMemoryPresetColor != color) {
		memset(g_pForegroundBitmapBits, (color << 4) | color, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
		g_nLastMemoryPresetColor = color;
		switch (g_nBackgroundDetectionMode) {
		case BDM_TOPLEFTPIXEL:
		case BDM_TOPRIGHTPIXEL:
		case BDM_BOTTOMLEFTPIXEL:
		case BDM_BOTTOMRIGHTPIXEL:
			// All pixels will be the same value at this point, so call any function.
			SetBackgroundColorFromTopLeftPixel();
			return 0x03;
		default:
			break;
		}
	}
	return 0x01;
}

void BorderPreset(BYTE color) {
	byte colorByte = (color << 4) | color;
	// Top and bottom edge.
	memset(g_pForegroundBitmapBits, colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	memset(g_pForegroundBitmapBits + (((CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) - (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT)) / 2), colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	// Left and right edge.
	for (int f = CDG_CELL_HEIGHT; f < CDG_CELL_HEIGHT + CDG_CANVAS_HEIGHT; ++f) {
		memset(g_pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
		memset(g_pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2) + ((CDG_WIDTH - CDG_CELL_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
	}
}

byte TileBlock(byte* pData, bool isXor) {
	// 3 byte buffer that we will use to set values in the CDG raster.
	static byte g_blockBuffer[3];
	byte bgColor = pData[0] & 0x0F;
	byte fgColor = pData[1] & 0x0F;
	// Python CDG parser suggests this bit means "ignore this command"?
	//if (pData[1] & 0x20)
	//	return;
	byte row = pData[2] & 0x3F;
	byte col = pData[3] & 0x3F;
	byte upperFgColor = fgColor << 4;
	byte upperBgColor = bgColor << 4;
	int xPixel = col * CDG_CELL_WIDTH;
	int yPixel = row * CDG_CELL_HEIGHT;
	int foregroundBitmapOffset = ((xPixel)+(yPixel * CDG_BITMAP_WIDTH)) / 2;
	// The remaining 12 bytes in the data field will contain the bitmask of pixels to set.
	// The lower six bits of each byte are the pixel mask.
	for (int f = 0; f < 12; ++f) {
		byte bits = pData[f + 4];
		g_blockBuffer[0] = ((bits & 0x20) ? upperFgColor : upperBgColor) | ((bits & 0x10) ? fgColor : bgColor);
		g_blockBuffer[1] = ((bits & 0x08) ? upperFgColor : upperBgColor) | ((bits & 0x04) ? fgColor : bgColor);
		g_blockBuffer[2] = ((bits & 0x02) ? upperFgColor : upperBgColor) | ((bits & 0x01) ? fgColor : bgColor);
		if (isXor) {
			g_pForegroundBitmapBits[foregroundBitmapOffset] ^= g_blockBuffer[0];
			g_pForegroundBitmapBits[foregroundBitmapOffset + 1] ^= g_blockBuffer[1];
			g_pForegroundBitmapBits[foregroundBitmapOffset + 2] ^= g_blockBuffer[2];
		}
		else {
			g_pForegroundBitmapBits[foregroundBitmapOffset] = g_blockBuffer[0];
			g_pForegroundBitmapBits[foregroundBitmapOffset + 1] = g_blockBuffer[1];
			g_pForegroundBitmapBits[foregroundBitmapOffset + 2] = g_blockBuffer[2];
		}
		foregroundBitmapOffset += (CDG_BITMAP_WIDTH / 2);
	}
	// Did we write to the non-border screen area?
	byte result = (row < (CDG_HEIGHT_CELLS - 1) && row>0 && col < (CDG_WIDTH_CELLS - 1) && col>0) ? 0x03 : 0x02;
	// Also need to know if the background needs refreshed.
	if (g_nBackgroundDetectionMode == BDM_TOPLEFTPIXEL && col == 1 && row == 1)
		SetBackgroundColorFromTopLeftPixel();
	else if (g_nBackgroundDetectionMode == BDM_TOPRIGHTPIXEL && col == CDG_WIDTH_CELLS - 2 && row == 1)
		SetBackgroundColorFromTopRightPixel();
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMLEFTPIXEL && col == 1 && row == CDG_HEIGHT_CELLS - 2)
		SetBackgroundColorFromBottomLeftPixel();
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMRIGHTPIXEL && col == CDG_WIDTH_CELLS - 2 && row == CDG_HEIGHT_CELLS - 2)
		SetBackgroundColorFromBottomRightPixel();
	else
		result &= 0x01;
	// Screen is no longer blank.
	g_nLastMemoryPresetColor = -1;
	return result;
}

byte LoadColorTable(byte* pData, bool highTable) {
	int nPaletteStartIndex = (highTable ? 8 : 0);
	for (int f = 0; f < 8; ++f) {
		byte colorByte1 = pData[f * 2] & 0x3F;
		byte colorByte2 = pData[(f * 2) + 1] & 0x3F;
		// Get 4-bit color values.
		byte red = (colorByte1 >> 2) & 0x0F;
		byte green = ((colorByte1 << 2) & 0x0C) | ((colorByte2 >> 4) & 0x03);
		byte blue = colorByte2 & 0x0F;
		// Convert to 24-bit color.
		red = (red * 17);
		green = (green * 17);
		blue = (blue * 17);
		g_logicalPalette[f + nPaletteStartIndex] = { blue,green,red,0 };
	}
	// First, copy the original palette to the unique palette.
	for (int f = 0; f < 16; ++f)
		g_effectivePalette[f] = g_logicalPalette[f];
	// Now check each entry and unique-ify it if necessary.
	// We will increase/decrease the matching RGB values by this amount.
	// Each time we find a match, we will increment this value.
	byte uniqueifier = 1;
	// Remember that each colour will originally have been a 12-bit
	// colour, which we have multiplied by 17 to become 24-bit, so at the
	// start of this operation, there should not be any two colours that
	// are within 16 of each other. It should also be impossible for the
	// uniqueifier to exceed 16, so we should never create a clash.
	for (int f = 0; f < 16; ++f) {
		byte red = g_effectivePalette[f].rgbRed;
		byte green = g_effectivePalette[f].rgbGreen;
		byte blue = g_effectivePalette[f].rgbBlue;
		for (int g = f + 1; g < 16; ++g) {
			byte testRed = g_effectivePalette[g].rgbRed;
			byte testGreen = g_effectivePalette[g].rgbGreen;
			byte testBlue = g_effectivePalette[g].rgbBlue;
			if ((testRed == red) && (testGreen == green) && (testBlue == blue)) {
				testRed += ((byte)(testRed + uniqueifier) < testRed ? -uniqueifier : uniqueifier);
				testGreen += ((byte)(testGreen + uniqueifier) < testGreen ? -uniqueifier : uniqueifier);
				testBlue += ((byte)(testBlue + uniqueifier) < testBlue ? -uniqueifier : uniqueifier);
				g_effectivePalette[g] = { testBlue,testGreen,testRed,0 };
				++uniqueifier;
			}
		}
	}
	::SetDIBColorTable(g_hForegroundDC, 0, 16, g_effectivePalette);
	::SetDIBColorTable(g_hScrollBufferDC, 0, 16, g_effectivePalette);
	byte result = 0x01;
	if ((!highTable) && g_nBackgroundDetectionMode == BDM_PALETTEINDEXZERO) {
		SetBackgroundColorIndex(0);
		result |= 0x02;
	}
	return result;
}

byte Scroll(byte color, byte hScroll, byte hScrollOffset, byte vScroll, byte vScrollOffset, bool copy) {
	int nHScrollPixels = ((hScroll == 2 ? -1 : (hScroll == 1 ? 1 : 0)) * CDG_CELL_WIDTH);
	int nVScrollPixels = ((vScroll == 2 ? -1 : (vScroll == 1 ? 1 : 0)) * CDG_CELL_HEIGHT);
	g_nCanvasXOffset = hScrollOffset;
	g_nCanvasYOffset = vScrollOffset;
	// This should be faster than BitBlt
	memcpy(g_pScrollBufferBitmapBits, g_pForegroundBitmapBits, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	::BitBlt(g_hForegroundDC, nHScrollPixels, nVScrollPixels, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, SRCCOPY);
	if (copy) {
		if (nVScrollPixels > 0)
			::BitBlt(g_hForegroundDC, nHScrollPixels, 0, CDG_BITMAP_WIDTH, nVScrollPixels, g_hScrollBufferDC, 0, CDG_HEIGHT - nVScrollPixels, SRCCOPY);
		else if (nVScrollPixels < 0)
			::BitBlt(g_hForegroundDC, nHScrollPixels, CDG_HEIGHT + nVScrollPixels, CDG_BITMAP_WIDTH, -nVScrollPixels, g_hScrollBufferDC, 0, 0, SRCCOPY);

		if (nHScrollPixels > 0)
			::BitBlt(g_hForegroundDC, 0, nVScrollPixels, nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, CDG_WIDTH - nHScrollPixels, 0, SRCCOPY);
		else if (nHScrollPixels < 0)
			::BitBlt(g_hForegroundDC, CDG_WIDTH + nHScrollPixels, nVScrollPixels, -nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, SRCCOPY);
	}
	byte result = 0x03;
	if (g_nBackgroundDetectionMode == BDM_TOPLEFTPIXEL)
		SetBackgroundColorFromTopLeftPixel();
	else if (g_nBackgroundDetectionMode == BDM_TOPRIGHTPIXEL)
		SetBackgroundColorFromTopRightPixel();
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMLEFTPIXEL)
		SetBackgroundColorFromBottomLeftPixel();
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMRIGHTPIXEL)
		SetBackgroundColorFromBottomRightPixel();
	else
		result = 0x01;
	return result;
}

byte ProcessCDGPackets() {
	byte result = 0;
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent };
	if (g_nCDGPC < g_nCDGPackets) {
		// Get current song position in milliseconds (see comment about rewind tolerance).
		int songPosition = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
		if (songPosition != -1) {
			songPosition += HYSTERESIS_MS;
			int cdgFrameIndex = (int)(songPosition / CDG_FRAME_DURATION_MS);
			// If the target frame is BEFORE the current CDGPC, the user has
			// (possibly) rewound the song.
			// Due to the XORing nature of CDG graphics, we have to start from the start
			// and reconstruct the screen up until this point!
			int difference = cdgFrameIndex - g_nCDGPC;
			if (difference < -REWIND_TOLERANCE_MS)
				g_nCDGPC = 0;
			for (; g_nCDGPC < cdgFrameIndex;) {
				int waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, 0);
				if (waitResult == WAIT_TIMEOUT) {
					CDGPacket* pCDGPacket = g_pCDGData + g_nCDGPC;
					if ((pCDGPacket->command & 0x3F) == 0x09) {
						BYTE instr = pCDGPacket->instruction & 0x3F;
						switch (instr) {
						case CDG_INSTR_MEMORY_PRESET:
							result |= MemoryPreset(pCDGPacket->data[0] & 0x0F, pCDGPacket->data[1] & 0x0F);
							break;
						case CDG_INSTR_BORDER_PRESET:
							BorderPreset(pCDGPacket->data[0] & 0x0F);
							break;
						case CDG_INSTR_TILE_BLOCK:
						case CDG_INSTR_TILE_BLOCK_XOR:
							result |= TileBlock(pCDGPacket->data, instr == CDG_INSTR_TILE_BLOCK_XOR);
							break;
						case CDG_INSTR_SCROLL_COPY:
						case CDG_INSTR_SCROLL_PRESET:
							result |= Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x0F, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x0F, instr == CDG_INSTR_SCROLL_COPY);
							break;
						case CDG_INSTR_TRANSPARENT_COLOR:
							// Not implemented.
							break;
						case CDG_INSTR_LOAD_COLOR_TABLE_LOW:
						case CDG_INSTR_LOAD_COLOR_TABLE_HIGH:
							result |= LoadColorTable(pCDGPacket->data, instr == CDG_INSTR_LOAD_COLOR_TABLE_HIGH);
							break;
						default:
							break;
						}
					}
					g_nCDGPC++;
				}
				else
					break;
			}
		}
	}
	return result;
}

void DrawBackground() {
	RECT r;
	::GetClientRect(g_hBackgroundWindow, &r);
	::StretchBlt(g_hBackgroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hBackgroundDC, 0, 0, 1, 1, SRCCOPY);
}

void DrawForeground() {
	RECT r;
	::GetClientRect(g_hForegroundWindow, &r);

	::BitBlt(g_hMaskDC, 0, 0, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hForegroundDC, 0, 0, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 8);
	for (int f = -1; f < 2; ++f)
		for (int g = -1; g < 2; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f, g, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hMaskDC, 0, 0, SRCPAINT);
	::MaskBlt(g_hMaskedForegroundDC, 0, 0, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hForegroundDC, 0, 0, g_hBorderMaskBitmap, 0, 0, MAKEROP4(SRCCOPY, PATCOPY));
	if (g_bShowBorder)
		::StretchBlt(g_hForegroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hMaskedForegroundDC, g_nCanvasXOffset, g_nCanvasYOffset, CDG_WIDTH, CDG_HEIGHT, SRCCOPY);
	else
		::StretchBlt(g_hForegroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hMaskedForegroundDC, CDG_CANVAS_X + g_nCanvasXOffset, CDG_CANVAS_Y + g_nCanvasYOffset, CDG_CANVAS_WIDTH, CDG_CANVAS_HEIGHT, SRCCOPY);
}

DWORD WINAPI CDGProcessor(LPVOID pParams) {
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent,g_hSongLoadedEvent };
	for (;;) {
		int waitResult = ::WaitForMultipleObjects(2, waitHandles + 1, FALSE, INFINITE);
		if (waitResult == 0) {
			break;
		}
		for (;;) {
			waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, SCREEN_REFRESH_MS);
			if (waitResult == 0)
				break;
			if (waitResult == 1)
				return 0;
			if (waitResult == WAIT_TIMEOUT) {
				byte result = ProcessCDGPackets();
				if (result & 0x01)
					::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				if (result & 0x02)
					::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			}
		}
		::SetEvent(g_hStoppedCDGProcessingEvent);
	}
	return 0;
}

bool StartCDGProcessingThread() {
	g_hStopCDGProcessingEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hStoppedCDGProcessingEvent = ::CreateEvent(NULL, FALSE, TRUE, NULL);
	g_hStopCDGThreadEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hSongLoadedEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hCDGProcessingThread = ::CreateThread(NULL, 0, CDGProcessor, NULL, 0, &g_nCDGProcessingThreadID);
	return !!g_hCDGProcessingThread;
}

LRESULT CALLBACK ForegroundWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
		if (hit == HTCLIENT)
			hit = HTCAPTION;
		return hit;
	}
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		break;
	case WM_MOVE: {
		int x = (int)(short)LOWORD(lParam);
		int y = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, x, y, 0, 0, SWP_NOSIZE);
		break;
	}
	case WM_SIZE: {
		int w = (int)(short)LOWORD(lParam);
		int h = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, w, h, SWP_NOMOVE);
		break;
	}
	case WM_PAINT:
		DrawForeground();
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

LRESULT CALLBACK BackgroundWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_PAINT:
		DrawBackground();
		break;
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
		::SetWindowPos(hwnd, g_hForegroundWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ATOM RegisterForegroundWindowClass() {
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = (WNDPROC)ForegroundWindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = plugin.hDllInstance;
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, (LPTSTR)IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = g_foregroundWindowClassName;
	wndClass.hIconSm = NULL;
	return RegisterClassEx(&wndClass);
}

ATOM RegisterBackgroundWindowClass() {
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = (WNDPROC)BackgroundWindowProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = plugin.hDllInstance;
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, (LPTSTR)IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = g_backgroundWindowClassName;
	wndClass.hIconSm = NULL;
	return RegisterClassEx(&wndClass);
}

/*ATOM RegisterWindowClass(const WCHAR *pszClassName, WNDPROC wndProc) {
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = (WNDPROC)wndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = plugin.hDllInstance;
	wndClass.hIcon = NULL;
	wndClass.hCursor = LoadCursor(NULL, (LPTSTR)IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = pszClassName;
	wndClass.hIconSm = NULL;
	return RegisterClassEx(&wndClass);
}

ATOM RegisterBackgroundWindowClass() {
	return RegisterWindowClass(g_backgroundWindowClassName, (WNDPROC)BackgroundWindowProc);
}

ATOM RegisterForegroundWindowClass() {
	return RegisterWindowClass(g_foregroundWindowClassName, (WNDPROC)ForegroundWindowProc);
}*/

bool CreateCDGWindow(HWND* phWnd, HDC* phDC, bool foreground) {
	DWORD styles = WS_VISIBLE | (foreground ? WS_THICKFRAME : 0);
	*phWnd = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_NOACTIVATE,
		foreground ? g_foregroundWindowClassName : g_backgroundWindowClassName,
		foreground ? g_foregroundWindowClassName : g_backgroundWindowClassName,
		styles,
		50, 50,
		CDG_WIDTH, CDG_HEIGHT,
		plugin.hwndParent,
		NULL,
		plugin.hDllInstance,
		NULL);
	if (*phWnd) {
		*phDC = ::GetDC(*phWnd);
		if (*phDC) {
			if (foreground)
				::SetLayeredWindowAttributes(*phWnd, DEFAULT_TRANSPARENT_COLOR, 255, LWA_COLORKEY);
			else
				::SetLayeredWindowAttributes(*phWnd, 0, g_nBackgroundOpacity, LWA_ALPHA);
			::SetWindowLong(*phWnd, GWL_STYLE, styles);
			::SetWindowPos(*phWnd, NULL, 50, 50, CDG_WIDTH, CDG_HEIGHT, SWP_NOZORDER | SWP_FRAMECHANGED);
			RECT windowRect, clientRect;
			::GetWindowRect(*phWnd, &windowRect);
			::GetClientRect(*phWnd, &clientRect);
			int windowWidth = windowRect.right - windowRect.left;
			int windowHeight = windowRect.bottom - windowRect.top;
			int clientWidth = clientRect.right - clientRect.left;
			int clientHeight = clientRect.bottom - clientRect.top;
			::SetWindowPos(*phWnd, NULL, 50, 50, CDG_WIDTH + (windowWidth - clientWidth), CDG_HEIGHT + (windowHeight - clientHeight), SWP_NOZORDER);
			return true;
		}
	}
	return false;
}

bool CreateForegroundWindow() {
	return CreateCDGWindow(&g_hForegroundWindow, &g_hForegroundWindowDC, true);
}

bool CreateBackgroundWindow() {
	return CreateCDGWindow(&g_hBackgroundWindow, &g_hBackgroundWindowDC, false);
}

bool CreateForegroundDC() {
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = CDG_BITMAP_WIDTH;
	bitmapInfo.bmiHeader.biHeight = -CDG_BITMAP_HEIGHT;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 4;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;
	g_hForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hForegroundDC) {
		g_hForegroundBitmap = ::CreateDIBSection(g_hForegroundWindowDC, &bitmapInfo, 0, (void**)&g_pForegroundBitmapBits, NULL, 0);
		if (g_hForegroundBitmap) {
			::ZeroMemory(g_pForegroundBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 2));
			::ZeroMemory(g_logicalPalette, sizeof(RGBQUAD) * 16);
			::SelectObject(g_hForegroundDC, g_hForegroundBitmap);
			::SetDIBColorTable(g_hForegroundDC, 0, 16, g_logicalPalette);
			return true;
		}
	}
	return false;
}

bool CreateScrollBufferDC() {
	bool result = false;
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = CDG_BITMAP_WIDTH;
	bitmapInfo.bmiHeader.biHeight = -CDG_BITMAP_HEIGHT;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 4;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;
	g_hScrollBufferDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hScrollBufferDC) {
		g_hScrollBufferBitmap = ::CreateDIBSection(g_hForegroundWindowDC, &bitmapInfo, 0, (void**)&g_pScrollBufferBitmapBits, NULL, 0);
		if (g_hScrollBufferBitmap) {
			::ZeroMemory(g_pForegroundBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 2));
			::SelectObject(g_hScrollBufferDC, g_hScrollBufferBitmap);
			return true;
		}
	}
	return false;
}

bool CreateMaskedForegroundDC() {
	g_hMaskedForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskedForegroundDC) {
		g_hMaskedForegroundBitmap = ::CreateCompatibleBitmap(g_hForegroundWindowDC, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT);
		if (g_hMaskedForegroundBitmap) {
			::SelectObject(g_hMaskedForegroundDC, g_hTransparentBrush);
			::SelectObject(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
			return true;
		}
	}
	return false;
}

bool CreateMaskDC() {
	g_hMaskDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskDC) {
		g_hMaskBitmap = ::CreateBitmap(CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, 1, 1, NULL);
		if (g_hMaskBitmap) {
			::SelectObject(g_hMaskDC, g_hMaskBitmap);
			return true;
		}
	}
	return false;
}

bool CreateBorderMaskDC() {
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = CDG_BITMAP_WIDTH;
	bitmapInfo.bmiHeader.biHeight = CDG_BITMAP_HEIGHT;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 1;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;

	bool result = false;
	g_hBorderMaskDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hBorderMaskDC) {
		g_hBorderMaskBitmap = ::CreateDIBSection(g_hForegroundWindowDC, &bitmapInfo, 0, (void**)&g_pBorderMaskBitmapBits, NULL, 0);
		if (g_hBorderMaskBitmap) {
			::SelectObject(g_hBorderMaskDC, g_hBorderMaskBitmap);
			result = true;
		}
	}
	return result;
}

bool CreateBackgroundDC() {
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = 1;
	bitmapInfo.bmiHeader.biHeight = 1;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = 32;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;

	g_hBackgroundDC = ::CreateCompatibleDC(g_hBackgroundWindowDC);
	if (g_hBackgroundDC) {
		g_hBackgroundBitmap = ::CreateDIBSection(g_hBackgroundWindowDC, &bitmapInfo, 0, (void**)&g_pBackgroundBitmapBits, NULL, 0);
		if (g_hBackgroundBitmap) {
			::SelectObject(g_hBackgroundDC, g_hBackgroundBitmap);
			return true;
		}
	}
	return false;
}

void readCDGData(const WCHAR* pFileBeingPlayed) {
	WCHAR pathBuffer[MAX_PATH + 1];
	char zipEntryName[MAX_PATH + 1];
	zip_stat_t fileStat;

	wcscpy_s(pathBuffer, pFileBeingPlayed);
	pathBuffer[MAX_PATH] = '\0';
	int pathLength = wcslen(pathBuffer);
	_wcslwr_s(pathBuffer);
	const WCHAR* zipPrefixLocation = wcsstr(pathBuffer, L"zip://");
	bool isZipFile = !wcscmp(pathBuffer + (pathLength - 4), L".zip");
	if (zipPrefixLocation || isZipFile) {
		// Format of string might be zip://somepathtoazipfile,n
		// n will be the indexed entry in the zip file that is being played.
		// First of all, get rid of the zip:// bit.
		if (zipPrefixLocation) {
			wcscpy_s(pathBuffer, pathBuffer + 6);
			// Now we need to get rid of the ,n bit
			WCHAR* pComma = wcsrchr(pathBuffer, ',');
			if (pComma - pathBuffer)
				*pComma = '\0';
		}
		// OK, we now have the path to the zip file. We can go ahead and read it, looking for the
		// CDG file.
		FILE* pFile;
		errno_t fileError = _wfopen_s(&pFile, pathBuffer, L"rb");
		if (!fileError && pFile) {
			zip_source_t* pZipSource = zip_source_filep_create(pFile, 0, 0, NULL);
			if (pZipSource) {
				zip_t* pZip = zip_open_from_source(pZipSource, ZIP_RDONLY, NULL);
				if (pZip) {
					zip_int64_t nZipEntries = zip_get_num_entries(pZip, ZIP_FL_UNCHANGED);
					for (--nZipEntries; ~nZipEntries; --nZipEntries) {
						const char* pEntryName = zip_get_name(pZip, nZipEntries, ZIP_FL_ENC_GUESS);
						strcpy_s(zipEntryName, pEntryName);
						_strlwr_s(zipEntryName);
						int nameLength = strlen(zipEntryName);
						if (strstr(zipEntryName, ".cdg") == zipEntryName + (nameLength - 4)) {
							if (!zip_stat_index(pZip, nZipEntries, ZIP_FL_UNCHANGED, &fileStat)) {
								zip_file_t* pCDGFile = zip_fopen_index(pZip, nZipEntries, ZIP_FL_UNCHANGED);
								if (pCDGFile) {
									g_nCDGPackets = (int)(fileStat.size / sizeof(CDGPacket));
									g_pCDGData = (CDGPacket*)malloc((size_t)g_nCDGPackets * sizeof(CDGPacket));
									zip_fread(pCDGFile, g_pCDGData, fileStat.size);
									zip_fclose(pCDGFile);
									break;
								}
							}
						}
					}
					zip_close(pZip);
				}
				zip_source_close(pZipSource);
			}
			fclose(pFile);
		}
	}
	else {
		// This is a plain filesystem path. We want to replace the extension with cdg, or
		// add cdg if there is no extension.
		WCHAR* pDot = wcsrchr(pathBuffer, '.');
		WCHAR* pSlash = wcsrchr(pathBuffer, '\\');
		if (pDot > pSlash)
			*pDot = '\0';
		wcscat_s(pathBuffer, L".cdg");
		FILE* pFile = NULL;
		errno_t error = _wfopen_s(&pFile, pathBuffer, L"rb");
		if (!error && pFile) {
			fseek(pFile, 0, SEEK_END);
			int size = ftell(pFile);
			g_nCDGPackets = size / sizeof(CDGPacket);
			fseek(pFile, 0, SEEK_SET);
			g_pCDGData = (CDGPacket*)malloc(g_nCDGPackets * sizeof(CDGPacket));
			if (g_pCDGData)
				fread(g_pCDGData, sizeof(CDGPacket), g_nCDGPackets, pFile);
			fclose(pFile);
		}
	}
}

DWORD WINAPI StartSongThread(LPVOID pParams) {
	::SetEvent(g_hStopCDGProcessingEvent);
	::WaitForSingleObject(g_hStoppedCDGProcessingEvent, INFINITE);
	::ResetEvent(g_hStopCDGProcessingEvent);
	const WCHAR* fileBeingPlayed = (const WCHAR*)pParams;
	clearExistingCDGData();
	readCDGData(fileBeingPlayed);
	free(pParams);
	if (g_pCDGData) {
		g_nCDGPC = 0;
		::SetEvent(g_hSongLoadedEvent);
	}
	return 0;
}

LRESULT CALLBACK CdgProWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	DWORD nStartSongThreadID;
	switch (uMsg) {
	case WM_WA_IPC:
		switch (lParam) {
		case IPC_PLAYING_FILEW: {
			const WCHAR* pszSongTitle = (const WCHAR*)wParam;
			int nStrLen = (wcslen(pszSongTitle) + 1);
			WCHAR* pszSongTitleCopy = (WCHAR*)malloc(sizeof(WCHAR) * nStrLen);
			if (pszSongTitleCopy) {
				wcscpy_s(pszSongTitleCopy, nStrLen, pszSongTitle);
				::CreateThread(NULL, 0, StartSongThread, (LPVOID)pszSongTitleCopy, 0, &nStartSongThreadID);
			}
			break;
		}
		case IPC_CB_MISC:
			if (wParam == IPC_CB_MISC_STATUS) {
				//ShowCDGDisplay(g_pCDGData && ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING) != 0);
			}
			break;
		}
		break;
	}
	// Call Winamp Window Proc
	return CallWindowProc(g_pOriginalWndProc, hwnd, uMsg, wParam, lParam);
}

bool CreateTransparentBrush() {
	g_hTransparentBrush = ::CreateSolidBrush(DEFAULT_TRANSPARENT_COLOR);
	return !!g_hTransparentBrush;
}

int init() {
	if (CreateTransparentBrush())
		if (RegisterBackgroundWindowClass())
			if (RegisterForegroundWindowClass())
				if (CreateBackgroundWindow())
					if (CreateForegroundWindow())
						if (CreateBackgroundDC())
							if (CreateForegroundDC())
								if (CreateMaskDC())
									if (CreateBorderMaskDC())
										if (CreateScrollBufferDC())
											if (CreateMaskedForegroundDC())
												if (StartCDGProcessingThread()) {
												}

	g_pOriginalWndProc = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);
	return 0;
}

void config() {
}

void quit() {
	::SetEvent(g_hStopCDGProcessingEvent);
	::SetEvent(g_hStopCDGThreadEvent);
	::WaitForSingleObject(g_hCDGProcessingThread, INFINITE);

	SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);

	clearExistingCDGData();

	if (g_hForegroundWindowDC)
		::ReleaseDC(g_hForegroundWindow, g_hForegroundWindowDC);
	if (g_hForegroundWindow) {
		::CloseWindow(g_hForegroundWindow);
		::DestroyWindow(g_hForegroundWindow);
	}
	if (g_hBackgroundWindowDC)
		::ReleaseDC(g_hBackgroundWindow, g_hBackgroundWindowDC);
	if (g_hBackgroundWindow) {
		::CloseWindow(g_hBackgroundWindow);
		::DestroyWindow(g_hBackgroundWindow);
	}

	if (g_hBackgroundDC)
		::DeleteDC(g_hBackgroundDC);
	if (g_hBackgroundBitmap)
		::DeleteObject(g_hBackgroundBitmap);

	if (g_hForegroundDC)
		::DeleteDC(g_hForegroundDC);
	if (g_hForegroundBitmap)
		::DeleteObject(g_hForegroundBitmap);

	if (g_hMaskDC)
		::DeleteDC(g_hMaskDC);
	if (g_hMaskBitmap)
		::DeleteObject(g_hMaskBitmap);

	if (g_hBorderMaskDC)
		::DeleteDC(g_hBorderMaskDC);
	if (g_hBorderMaskBitmap)
		::DeleteObject(g_hBorderMaskBitmap);

	if (g_hMaskedForegroundDC)
		::DeleteDC(g_hMaskedForegroundDC);
	if (g_hMaskedForegroundBitmap)
		::DeleteObject(g_hMaskedForegroundBitmap);

	if (g_hScrollBufferDC)
		::DeleteDC(g_hScrollBufferDC);
	if (g_hScrollBufferBitmap)
		::DeleteObject(g_hScrollBufferBitmap);

	if (g_hTransparentBrush)
		::DeleteObject(g_hTransparentBrush);

	::CloseHandle(g_hStopCDGProcessingEvent);
	::CloseHandle(g_hStopCDGThreadEvent);
	::CloseHandle(g_hSongLoadedEvent);

	UnregisterClass(g_foregroundWindowClassName, plugin.hDllInstance);
	UnregisterClass(g_backgroundWindowClassName, plugin.hDllInstance);
}