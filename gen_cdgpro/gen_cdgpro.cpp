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
#include "resource.h"

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
int g_nBackgroundOpacity = 160;
bool g_bDrawOutline = true;
bool g_bShowBorder = false;
// We periodically ask WinAmp how many milliseconds it has played of a song. This works fine
// but as time goes on, it starts to get it wrong, falling behind by a tiny amount each time.
// To keep the display in sync, we will multiple whatever WinAmp tells us by this amount.
double g_nTimeScaler = 1.00466;
// How to determine the transparent background color?
int g_nBackgroundDetectionMode = BDM_TOPLEFTPIXEL;
// Default background colour when there is no song playing.
int g_nDefaultBackgroundColor = 0x0055ff;
// Scale2x/4x smoothing?
int g_nSmoothingPasses = 2;
// Logo to display when there is no song playing.
const WCHAR* g_pszLogoPath = L"C:\\Users\\steven.frew\\Desktop\\smallLogo.png";

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

// In Windows, bitmaps must have a width that is a multiple of 4 bytes.
// We are storing two bitmaps to represent the CDG display,
// one of which is 4 bits per pixel, and one which is 1 bit per pixel.
// Because of the 1bpp bitmap, this means that the bitmap width
// must be a multiple of 32 (32 bits = 4 bytes).
#define CDG_BITMAP_WIDTH (320)
#define CDG_BITMAP_HEIGHT CDG_HEIGHT

// For smoothing purposes, we will scale the graphics up and apply a smoothing algorithm.
#define SUPPORTED_SCALING_LEVELS (3) // 1x,2x,4x
#define MAXIMUM_SCALING_FACTOR (4)
#define CDG_MAXIMUM_BITMAP_WIDTH (CDG_BITMAP_WIDTH*MAXIMUM_SCALING_FACTOR)
#define CDG_MAXIMUM_BITMAP_HEIGHT (CDG_BITMAP_HEIGHT*MAXIMUM_SCALING_FACTOR)

#define TOP_LEFT_PIXEL_OFFSET (((CDG_CELL_HEIGHT*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define TOP_RIGHT_PIXEL_OFFSET (TOP_LEFT_PIXEL_OFFSET+(CDG_CANVAS_WIDTH-1))
#define BOTTOM_LEFT_PIXEL_OFFSET (((((CDG_CELL_HEIGHT+CDG_CANVAS_HEIGHT)-1)*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define BOTTOM_RIGHT_PIXEL_OFFSET (BOTTOM_LEFT_PIXEL_OFFSET+(CDG_CANVAS_WIDTH-1))

// Each CDG frame is 1/300th of a second.
#define CDG_FRAME_DURATION_MS (1000.0/300.0)

// The CDG graphics files are often slightly ahead of the music, probably due to the techniques
// used when ripping the data, or possibly because the disc manufacturers assume a certain amount of
// buffering in the disc player audio hardware and no such delay exists when we play the data
// directly in software. Anyway, this is the amount of time that we will add to account for that.
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

// This is the colour we will use for transparency. It is impossible to
// represent this as a 12-bit colour, and each value is a different distance from
// a multiple of 17, so it should never accidentally happen.
#define DEFAULT_TRANSPARENT_COLOR_RED (145)
#define DEFAULT_TRANSPARENT_COLOR_GREEN (67)
#define DEFAULT_TRANSPARENT_COLOR_BLUE (219)
#define DEFAULT_TRANSPARENT_COLORREF (RGB(DEFAULT_TRANSPARENT_COLOR_RED,DEFAULT_TRANSPARENT_COLOR_GREEN,DEFAULT_TRANSPARENT_COLOR_BLUE))

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
// The DC and bitmap containing the background (usually 1 pixel that we stretch out).
HDC g_hBackgroundDC = NULL;
HBITMAP g_hBackgroundBitmap = NULL;
unsigned int* g_pBackgroundBitmapBits = NULL;
// The DC and bitmap containing the CDG graphics.
HDC g_hScaledForegroundDCs[SUPPORTED_SCALING_LEVELS];
HBITMAP g_hScaledForegroundBitmaps[SUPPORTED_SCALING_LEVELS];
BYTE* g_pScaledForegroundBitmapBits[SUPPORTED_SCALING_LEVELS];
// The DC containing the mask for the CDG graphics.
HDC g_hMaskDC = NULL;
HBITMAP g_hMaskBitmap = NULL;
// The DC containing the border mask for the CDG graphics.
HDC g_hBorderMaskDC = NULL;
HBITMAP g_hBorderMaskBitmap = NULL;
BYTE* g_pBorderMaskBitmapBits = NULL;
// The DC containing the masked CDG graphics.
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
BYTE g_nCurrentTransparentIndex = 0;
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
// The palettes ... logical is the one defined by the CDG data, effective is the one we're using
// that has been pochled to ensure there are no duplicate colours.
RGBQUAD g_logicalPalette[16];
RGBQUAD g_effectivePalette[16];
// DLL icon
HICON g_hIcon = NULL;
// Logo image
Image* g_pLogoImage = NULL;
SIZE g_logoSize;
bool g_bShowLogo = true;
// GDI+ token
ULONG_PTR g_gdiPlusToken;

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
	g_nCurrentTransparentIndex = index;
	// RGB macro, for some reason, encodes as BGR. Not so handy for direct 32-bit bitmap writing.
	COLORREF backgroundColorReversed = RGB(g_effectivePalette[index].rgbBlue, g_effectivePalette[index].rgbGreen, g_effectivePalette[index].rgbRed);
	*g_pBackgroundBitmapBits = backgroundColorReversed;
	COLORREF backgroundColor = RGB(g_effectivePalette[index].rgbRed, g_effectivePalette[index].rgbGreen, g_effectivePalette[index].rgbBlue);
	for(int f=0;f<SUPPORTED_SCALING_LEVELS;++f)
		::SetBkColor(g_hScaledForegroundDCs[f], backgroundColor);
}

void SetBackgroundColorFromPixel(int offset, bool highNibble) {
	byte color = (g_pScaledForegroundBitmapBits[0][offset] >> (highNibble ? 4 : 0)) & 0x0F;
	SetBackgroundColorIndex(color);
}

byte MemoryPreset(BYTE color, BYTE repeat) {
	if (g_nLastMemoryPresetColor == color)
		return 0x00;
	memset(g_pScaledForegroundBitmapBits[0], (color << 4) | color, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	g_nLastMemoryPresetColor = color;
	switch (g_nBackgroundDetectionMode) {
	case BDM_TOPLEFTPIXEL:
	case BDM_TOPRIGHTPIXEL:
	case BDM_BOTTOMLEFTPIXEL:
	case BDM_BOTTOMRIGHTPIXEL:
		// All pixels will be the same value at this point, so use any corner.
		SetBackgroundColorFromPixel(TOP_LEFT_PIXEL_OFFSET, true);
		return 0x03;
	default:
		return 0x01;
	}
}

void BorderPreset(BYTE color) {
	byte colorByte = (color << 4) | color;
	// Top and bottom edge.
	BYTE* pForegroundBitmapBits=g_pScaledForegroundBitmapBits[0];
	memset(pForegroundBitmapBits, colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	memset(pForegroundBitmapBits + (((CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) - (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT)) / 2), colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	// Left and right edge.
	for (int f = CDG_CELL_HEIGHT; f < CDG_CELL_HEIGHT + CDG_CANVAS_HEIGHT; ++f) {
		memset(pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
		memset(pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2) + ((CDG_WIDTH - CDG_CELL_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
	}
	// Screen is no longer "blank".
	g_nLastMemoryPresetColor = -1;
}

bool CheckPixelColorBackgroundChange(bool topLeftPixelSet, bool topRightPixelSet, bool bottomLeftPixelSet, bool bottomRightPixelSet) {
	if (g_nBackgroundDetectionMode == BDM_TOPLEFTPIXEL && topLeftPixelSet)
		SetBackgroundColorFromPixel(TOP_LEFT_PIXEL_OFFSET, true);
	else if (g_nBackgroundDetectionMode == BDM_TOPRIGHTPIXEL && topRightPixelSet)
		SetBackgroundColorFromPixel(TOP_RIGHT_PIXEL_OFFSET, false);
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMLEFTPIXEL && bottomLeftPixelSet)
		SetBackgroundColorFromPixel(BOTTOM_LEFT_PIXEL_OFFSET, true);
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMRIGHTPIXEL && bottomRightPixelSet)
		SetBackgroundColorFromPixel(BOTTOM_RIGHT_PIXEL_OFFSET, false);
	else
		return false;
	return true;
}

byte TileBlock(byte* pData, bool isXor) {
	// 3 byte buffer that we will use to set values in the CDG raster.
	static byte g_blockBuffer[3];
	byte bgColor = pData[0] & 0x0F;
	byte fgColor = pData[1] & 0x0F;
	// Python CDG parser code suggests this bit means "ignore this command"?
	//if (pData[1] & 0x20)
	//	return;
	byte row = pData[2] & 0x3F;
	byte col = pData[3] & 0x3F;
	// If the coordinates are offscreen, reject them as bad data.
	if (col >= CDG_WIDTH_CELLS || row >= CDG_HEIGHT_CELLS)
		return 0x00;
	byte upperFgColor = fgColor << 4;
	byte upperBgColor = bgColor << 4;
	int xPixel = col * CDG_CELL_WIDTH;
	int yPixel = row * CDG_CELL_HEIGHT;
	int foregroundBitmapOffset = ((xPixel)+(yPixel * CDG_BITMAP_WIDTH)) / 2;
	BYTE* pForegroundBitmapBits = g_pScaledForegroundBitmapBits[0];
	// The remaining 12 bytes in the data field will contain the bitmask of pixels to set.
	// The lower six bits of each byte are the pixel mask.
	for (int f = 0; f < 12; ++f) {
		byte bits = pData[f + 4];
		g_blockBuffer[0] = ((bits & 0x20) ? upperFgColor : upperBgColor) | ((bits & 0x10) ? fgColor : bgColor);
		g_blockBuffer[1] = ((bits & 0x08) ? upperFgColor : upperBgColor) | ((bits & 0x04) ? fgColor : bgColor);
		g_blockBuffer[2] = ((bits & 0x02) ? upperFgColor : upperBgColor) | ((bits & 0x01) ? fgColor : bgColor);
		if (isXor) {
			pForegroundBitmapBits[foregroundBitmapOffset] ^= g_blockBuffer[0];
			pForegroundBitmapBits[foregroundBitmapOffset + 1] ^= g_blockBuffer[1];
			pForegroundBitmapBits[foregroundBitmapOffset + 2] ^= g_blockBuffer[2];
		}
		else {
			pForegroundBitmapBits[foregroundBitmapOffset] = g_blockBuffer[0];
			pForegroundBitmapBits[foregroundBitmapOffset + 1] = g_blockBuffer[1];
			pForegroundBitmapBits[foregroundBitmapOffset + 2] = g_blockBuffer[2];
		}
		foregroundBitmapOffset += (CDG_BITMAP_WIDTH / 2);
	}
	// Did we write to the non-border screen area?
	byte result = (row < (CDG_HEIGHT_CELLS - 1) && row>0 && col < (CDG_WIDTH_CELLS - 1) && col>0) ? 0x03 : 0x02;
	// Also need to know if the background needs refreshed.
	bool topLeftPixelSet = col == 1 && row == 1;
	bool topRightPixelSet = col == CDG_WIDTH_CELLS - 2 && row == 1;
	bool bottomLeftPixelSet = col == 1 && row == CDG_HEIGHT_CELLS - 2;
	bool bottomRightPixelSet = col == CDG_WIDTH_CELLS - 2 && row == CDG_HEIGHT_CELLS - 2;
	if(topLeftPixelSet || topRightPixelSet || bottomLeftPixelSet || bottomRightPixelSet)
		if (!CheckPixelColorBackgroundChange(topLeftPixelSet, topRightPixelSet, bottomLeftPixelSet, bottomRightPixelSet))
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
	for(int f=0;f<SUPPORTED_SCALING_LEVELS;++f)
		::SetDIBColorTable(g_hScaledForegroundDCs[f], 0, 16, g_effectivePalette);
	::SetDIBColorTable(g_hScrollBufferDC, 0, 16, g_effectivePalette);
	SetBackgroundColorIndex(g_nCurrentTransparentIndex);
	return 0x01|(g_nCurrentTransparentIndex>= nPaletteStartIndex && g_nCurrentTransparentIndex<nPaletteStartIndex+8?0x02:0x00);
}

byte Scroll(byte color, byte hScroll, byte hScrollOffset, byte vScroll, byte vScrollOffset, bool copy) {
	int nHScrollPixels = ((hScroll == 2 ? -1 : (hScroll == 1 ? 1 : 0)) * CDG_CELL_WIDTH);
	int nVScrollPixels = ((vScroll == 2 ? -1 : (vScroll == 1 ? 1 : 0)) * CDG_CELL_HEIGHT);
	g_nCanvasXOffset = hScrollOffset;
	g_nCanvasYOffset = vScrollOffset;
	// This should be faster than BitBlt
	memcpy(g_pScrollBufferBitmapBits, g_pScaledForegroundBitmapBits[0], (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	HDC hForegroundDC = g_hScaledForegroundDCs[0];
	::BitBlt(hForegroundDC, nHScrollPixels, nVScrollPixels, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, SRCCOPY);
	if (copy) {
		if (nVScrollPixels > 0)
			::BitBlt(hForegroundDC, nHScrollPixels, 0, CDG_BITMAP_WIDTH, nVScrollPixels, g_hScrollBufferDC, 0, CDG_HEIGHT - nVScrollPixels, SRCCOPY);
		else if (nVScrollPixels < 0)
			::BitBlt(hForegroundDC, nHScrollPixels, CDG_HEIGHT + nVScrollPixels, CDG_BITMAP_WIDTH, -nVScrollPixels, g_hScrollBufferDC, 0, 0, SRCCOPY);

		if (nHScrollPixels > 0)
			::BitBlt(hForegroundDC, 0, nVScrollPixels, nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, CDG_WIDTH - nHScrollPixels, 0, SRCCOPY);
		else if (nHScrollPixels < 0)
			::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, nVScrollPixels, -nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, SRCCOPY);
	}
	else if (color != g_nLastMemoryPresetColor)
		g_nLastMemoryPresetColor = -1;
	return CheckPixelColorBackgroundChange(true, true, true, true) ? 0x03 : 0x01;
}

byte ProcessCDGPackets() {
	byte result = 0;
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent };
	if (g_nCDGPC < g_nCDGPackets) {
		// Get current song position in milliseconds (see comment about rewind tolerance).
		int songPosition = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
		if (songPosition != -1) {
			// Account for WinAmp timing drift bug (see comment about time scaler)
			// and general lag (see comment about hysteresis).
			songPosition = (int)(songPosition * g_nTimeScaler) + HYSTERESIS_MS;
			int cdgFrameIndex = (int)(songPosition / CDG_FRAME_DURATION_MS);
			if (cdgFrameIndex > g_nCDGPackets)
				cdgFrameIndex = g_nCDGPackets;
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

void Perform2xSmoothing(BYTE *pSourceBitmapBits,BYTE *pDestinationBitmapBits,int nW,int nH,int nSourceBitmapWidth) {
	// 2x smoothing
	static byte EAandEB, BAandBB, HAandHB;
	static byte EA, EB;
	static byte B,D,F,H;
	static byte E0, E1, E2, E3;
	int right = nW-1;
	int bottom = nH-1;
	int halfBitmapWidth = nSourceBitmapWidth >>1;
	int doubleBitmapWidth = nSourceBitmapWidth <<1;
	int destFinishingOffset = nSourceBitmapWidth + (nSourceBitmapWidth - nW);
	int sourceFinishingOffset = halfBitmapWidth - (nW / 2);
	for (int y = 0; y <= bottom; ++y) {
		for (int x = 0; x <= right; x += 2) {
			// Each byte is two pixels.
			EAandEB = *pSourceBitmapBits;
			BAandBB = y ? *(pSourceBitmapBits - halfBitmapWidth) : EAandEB;
			HAandHB = y == bottom ? EAandEB : *(pSourceBitmapBits + halfBitmapWidth);
			EA = (EAandEB >> 4) & 0x0F;
			EB = EAandEB & 0x0F;

			// First pixel.
			D = x ? *(pSourceBitmapBits - 1) & 0x0F : EA;
			F = EB;
			B = (BAandBB >> 4) & 0x0F;
			H = (HAandHB >> 4) & 0x0F;

			if (B != H && D != F) {
				E0 = D == B ? D : EA;
				E1 = B == F ? F : EA;
				E2 = D == H ? D : EA;
				E3 = H == F ? F : EA;
			}
			else
				E0 = E1 = E2 = E3 = EA;
			*pDestinationBitmapBits = (E0 << 4) | E1;
			*((pDestinationBitmapBits++) + nSourceBitmapWidth) = (E2 << 4) | E3;

			// Second pixel.
			D = EA;
			F = x == right?EB:(*(pSourceBitmapBits++ +1) >> 4) & 0x0F;
			B = BAandBB & 0x0F;
			H = HAandHB & 0x0F;

			if (B != H && D != F) {
				E0 = D == B ? D : EB;
				E1 = B == F ? F : EB;
				E2 = D == H ? D : EB;
				E3 = H == F ? F : EB;
			}
			else
				E0 = E1 = E2 = E3 = EB;
			*pDestinationBitmapBits = (E0 << 4) | E1;
			*((pDestinationBitmapBits++) + nSourceBitmapWidth) = (E2 << 4) | E3;
		}
		pSourceBitmapBits += sourceFinishingOffset;
		pDestinationBitmapBits += destFinishingOffset;
	}
}

void DrawBackground() {
	RECT r;
	::GetClientRect(g_hBackgroundWindow, &r);
	::StretchBlt(g_hBackgroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hBackgroundDC, 0, 0, 1, 1, SRCCOPY);
}

void DrawForeground() {
	RECT r;
	HDC hSourceDC = g_hScaledForegroundDCs[0];
	::GetClientRect(g_hForegroundWindow, &r);
	int nScaling = 1;
	for (int f = 0; f < g_nSmoothingPasses && f<(SUPPORTED_SCALING_LEVELS-1); ++f) {
		int startX = CDG_CANVAS_X * nScaling;
		int startY = CDG_CANVAS_Y * nScaling;
		int width = CDG_WIDTH * nScaling;
		int height = CDG_HEIGHT * nScaling;
		int sourceWidth = CDG_BITMAP_WIDTH * nScaling;
		Perform2xSmoothing(g_pScaledForegroundBitmapBits[f], g_pScaledForegroundBitmapBits[f+1], width,height, sourceWidth);
		nScaling *= 2;
		hSourceDC = g_hScaledForegroundDCs[f+1];
	}
	::BitBlt(g_hMaskDC, 0, 0, CDG_BITMAP_WIDTH*nScaling, CDG_BITMAP_HEIGHT*nScaling, hSourceDC, 0, 0, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	for (int f = -nScaling; f <= nScaling; ++f)
		for (int g = -nScaling; g <= nScaling; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f, g, CDG_BITMAP_WIDTH * nScaling, CDG_BITMAP_HEIGHT * nScaling, g_hMaskDC, 0,0, SRCPAINT);
	::MaskBlt(g_hMaskedForegroundDC, 0, 0, CDG_BITMAP_WIDTH * nScaling, CDG_BITMAP_HEIGHT * nScaling, hSourceDC, 0, 0, g_hBorderMaskBitmap, 0, 0, MAKEROP4(SRCCOPY, PATCOPY));
	::StretchBlt(g_hForegroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hMaskedForegroundDC, (CDG_CANVAS_X + g_nCanvasXOffset)*nScaling, (CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling, CDG_CANVAS_WIDTH*nScaling, CDG_CANVAS_HEIGHT*nScaling, SRCCOPY);
	if (g_pLogoImage && g_bShowLogo) {
		RECT r;
		::GetClientRect(g_hForegroundWindow, &r);
		int windowWidth = r.right - r.left;
		int windowHeight = r.bottom - r.top;
		Graphics g(g_hForegroundWindowDC);
		g.DrawImage(g_pLogoImage, (windowWidth - g_logoSize.cx) / 2, (windowHeight - g_logoSize.cy) / 2, g_logoSize.cx, g_logoSize.cy);
	}
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
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		break;
	case WM_MOVE: {
		int x = (int)(short)LOWORD(lParam);
		int y = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
		break;
	}
	case WM_SIZE: {
		int w = (int)(short)LOWORD(lParam);
		int h = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE);
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
		::SetWindowPos(hwnd, g_hForegroundWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ATOM RegisterWindowClass(const WCHAR* pszClassName, WNDPROC wndProc) {
	WNDCLASSEX wndClass;
	wndClass.cbSize = sizeof(WNDCLASSEX);
	wndClass.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
	wndClass.lpfnWndProc = (WNDPROC)wndProc;
	wndClass.cbClsExtra = 0;
	wndClass.cbWndExtra = 0;
	wndClass.hInstance = plugin.hDllInstance;
	wndClass.hIcon = g_hIcon;
	wndClass.hCursor = LoadCursor(NULL, (LPTSTR)IDC_ARROW);
	wndClass.hbrBackground = NULL;
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = pszClassName;
	wndClass.hIconSm = g_hIcon;
	return ::RegisterClassEx(&wndClass);
}

ATOM RegisterForegroundWindowClass() {
	return RegisterWindowClass(g_foregroundWindowClassName, (WNDPROC)ForegroundWindowProc);
}

ATOM RegisterBackgroundWindowClass() {
	return RegisterWindowClass(g_backgroundWindowClassName, (WNDPROC)BackgroundWindowProc);
}

bool CreateCDGWindow(HWND* phWnd, HDC* phDC, const WCHAR* pszClassName, DWORD additionalStyles = 0, DWORD additionalExStyles = 0) {
	DWORD styles = WS_VISIBLE | additionalStyles;
	*phWnd = CreateWindowEx(
		WS_EX_LAYERED | additionalExStyles,
		pszClassName,
		pszClassName,
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
	return CreateCDGWindow(&g_hForegroundWindow, &g_hForegroundWindowDC, g_foregroundWindowClassName, WS_THICKFRAME, WS_EX_APPWINDOW);
}

bool CreateBackgroundWindow() {
	return CreateCDGWindow(&g_hBackgroundWindow, &g_hBackgroundWindowDC, g_backgroundWindowClassName, 0, WS_EX_NOACTIVATE);
}

bool CreateBitmapSurface(HDC* phDC, HBITMAP* phBitmap, LPVOID* ppBitmapBits, int nWidth, int nHeight, int nBitCount) {
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = nWidth;
	bitmapInfo.bmiHeader.biHeight = nHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = nBitCount;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;

	*phDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (*phDC) {
		*phBitmap = ::CreateDIBSection(*phDC, &bitmapInfo, 0, ppBitmapBits, NULL, 0);
		if (*phBitmap) {
			::SelectObject(*phDC, *phBitmap);
			return true;
		}
	}
	return false;
}

bool CreateForegroundDCs() {
	int nScaling = 1;
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f) {
		if (!CreateBitmapSurface(&(g_hScaledForegroundDCs[f]), &(g_hScaledForegroundBitmaps[f]), (LPVOID *)(&(g_pScaledForegroundBitmapBits[f])), CDG_BITMAP_WIDTH * nScaling, -CDG_BITMAP_HEIGHT * nScaling, 4))
			return false;
		nScaling *= 2;
	}
	return true;
}

bool CreateScrollBufferDC() {
	return CreateBitmapSurface(&g_hScrollBufferDC, &g_hScrollBufferBitmap, (LPVOID*)&g_pScrollBufferBitmapBits, CDG_BITMAP_WIDTH, -CDG_BITMAP_HEIGHT, 4);
}

bool CreateMaskedForegroundDC() {
	g_hMaskedForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskedForegroundDC) {
		g_hMaskedForegroundBitmap = ::CreateCompatibleBitmap(g_hForegroundWindowDC, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT);
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
		g_hMaskBitmap = ::CreateBitmap(CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, 1, 1, NULL);
		if (g_hMaskBitmap) {
			::SelectObject(g_hMaskDC, g_hMaskBitmap);
			return true;
		}
	}
	return false;
}

bool CreateBorderMaskDC() {
	bool result = CreateBitmapSurface(&g_hBorderMaskDC, &g_hBorderMaskBitmap, (LPVOID*)&g_pBorderMaskBitmapBits, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, 1);
	RGBQUAD monoPalette[] = {
		{255,255,255,0},
		{0,0,0,0}
	};
	return result && ::SetDIBColorTable(g_hBorderMaskDC, 0, 2, monoPalette);
}

bool CreateBackgroundDC() {
	return CreateBitmapSurface(&g_hBackgroundDC, &g_hBackgroundBitmap, (LPVOID*)&g_pBackgroundBitmapBits, 1, 1, 32);
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
		g_nCurrentTransparentIndex = 0;
		g_bShowLogo = false;
		::SetEvent(g_hSongLoadedEvent);
	}
	return 0;
}

void ClearCDGBuffer() {
	::ZeroMemory(g_logicalPalette, sizeof(RGBQUAD) * 16);
	::ZeroMemory(g_effectivePalette, sizeof(RGBQUAD) * 16);
	g_logicalPalette[0].rgbBlue = g_effectivePalette[0].rgbBlue = g_nDefaultBackgroundColor & 0x00ff;
	g_logicalPalette[0].rgbGreen = g_effectivePalette[0].rgbGreen = (g_nDefaultBackgroundColor >> 8) & 0x00ff;
	g_logicalPalette[0].rgbRed = g_effectivePalette[0].rgbRed = (g_nDefaultBackgroundColor >> 16) & 0x00ff;
	::ZeroMemory(g_pScaledForegroundBitmapBits[0], (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 2));
	::ZeroMemory(g_pScrollBufferBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 2));
	for(int f=0;f<SUPPORTED_SCALING_LEVELS;++f)
		::SetDIBColorTable(g_hScaledForegroundDCs[f], 0, 16, g_logicalPalette);
	SetBackgroundColorIndex(0);
	g_bShowLogo = true;
	::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
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
				int isPlayingResult = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING);
				if (!isPlayingResult)
					::ClearCDGBuffer();
			}
			break;
		}
		break;
	}
	// Call Winamp Window Proc
	return ::CallWindowProc(g_pOriginalWndProc, hwnd, uMsg, wParam, lParam);
}

bool CreateTransparentBrush() {
	g_hTransparentBrush = ::CreateSolidBrush(DEFAULT_TRANSPARENT_COLORREF);
	return !!g_hTransparentBrush;
}

void LoadLogo() {
	g_pLogoImage = new Image(g_pszLogoPath);
	if (g_pLogoImage->GetLastStatus() == Ok) {
		g_logoSize = { (LONG)g_pLogoImage->GetWidth(),(LONG)g_pLogoImage->GetHeight() };
	}
	else {
		delete g_pLogoImage;
		g_pLogoImage = NULL;
	}
}

int init() {
	GdiplusStartupInput g_gdiPlusStartupInput;
	GdiplusStartup(&g_gdiPlusToken, &g_gdiPlusStartupInput, NULL);
	LoadLogo();
	if (g_hIcon = ::LoadIcon(plugin.hDllInstance, MAKEINTRESOURCE(IDI_ICON1)))
		if (CreateTransparentBrush())
			if (RegisterBackgroundWindowClass())
				if (RegisterForegroundWindowClass())
					if (CreateBackgroundWindow())
						if (CreateForegroundWindow()) {
							::SetStretchBltMode(g_hForegroundWindowDC, COLORONCOLOR);
							if (CreateBackgroundDC())
								if (CreateForegroundDCs())
									if (CreateMaskDC())
										if (CreateBorderMaskDC())
											if (CreateScrollBufferDC())
												if (CreateMaskedForegroundDC()) {
													if (::SetLayeredWindowAttributes(g_hForegroundWindow, DEFAULT_TRANSPARENT_COLORREF, 255, LWA_COLORKEY))
														if (::SetLayeredWindowAttributes(g_hBackgroundWindow, 0, g_nBackgroundOpacity, LWA_ALPHA))
															if (StartCDGProcessingThread()) {
																ClearCDGBuffer();
															}
												}
						}

	g_pOriginalWndProc = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);
	return 0;
}

void config() {
}

void CloseWindow(HWND hWnd, HDC hDC) {
	if (hDC)
		::ReleaseDC(hWnd, hDC);
	if (hWnd) {
		::CloseWindow(hWnd);
		::DestroyWindow(hWnd);
	}
}

void DeleteDC(HDC hDC, HBITMAP hBitmap) {
	if (hDC)
		::DeleteDC(hDC);
	if (hBitmap)
		::DeleteObject(hBitmap);
}

void quit() {
	::SetEvent(g_hStopCDGProcessingEvent);
	::SetEvent(g_hStopCDGThreadEvent);
	::WaitForSingleObject(g_hCDGProcessingThread, INFINITE);

	::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);

	clearExistingCDGData();

	CloseWindow(g_hForegroundWindow, g_hForegroundWindowDC);
	CloseWindow(g_hBackgroundWindow, g_hBackgroundWindowDC);
	DeleteDC(g_hBackgroundDC, g_hBackgroundBitmap);
	DeleteDC(g_hMaskDC, g_hMaskBitmap);
	DeleteDC(g_hBorderMaskDC, g_hBorderMaskBitmap);
	DeleteDC(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
	DeleteDC(g_hScrollBufferDC, g_hScrollBufferBitmap);
	DeleteDC(g_hScrollBufferDC, g_hScrollBufferBitmap);
	for(int f=0;f<SUPPORTED_SCALING_LEVELS;++f)
		DeleteDC(g_hScaledForegroundDCs[f], g_hScaledForegroundBitmaps[f]);

	if (g_hTransparentBrush)
		::DeleteObject(g_hTransparentBrush);

	::CloseHandle(g_hStopCDGProcessingEvent);
	::CloseHandle(g_hStopCDGThreadEvent);
	::CloseHandle(g_hSongLoadedEvent);

	::UnregisterClass(g_foregroundWindowClassName, plugin.hDllInstance);
	::UnregisterClass(g_backgroundWindowClassName, plugin.hDllInstance);

	if (g_hIcon)
		::DeleteObject(g_hIcon);
	if (g_pLogoImage)
		delete g_pLogoImage;

	::GdiplusShutdown(g_gdiPlusToken);
}