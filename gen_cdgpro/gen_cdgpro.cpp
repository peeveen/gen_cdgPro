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

// plugin version (don't touch this)
#define GPPHDR_VER 0x10

// plugin name/title (change this to something you like)
#define PLUGIN_NAME "CDG-Pro Plugin"

// main structure with plugin information, version, name...
typedef struct {
	int version;                   // version of the plugin structure
	const char* description;             // name/title of the plugin 
	int(*init)();                 // function which will be executed on init event
	void(*config)();              // function which will be executed on config event
	void(*quit)();                // function which will be executed on quit event
	HWND hwndParent;               // hwnd of the Winamp client main window (stored by Winamp when dll is loaded)
	HINSTANCE hDllInstance;        // hinstance of this plugin DLL. (stored by Winamp when dll is loaded) 
} winampGeneralPurposePlugin;

// The official size of the CDG graphics area.
#define CDG_WIDTH (300)
#define CDG_HEIGHT (216)

// The outer edges of this area are not shown on screen, but are managed in memory (for scrolling
// images into the visible area, etc). This is the size of the visible central portion.
#define CDG_CANVAS_WIDTH (288)
#define CDG_CANVAS_HEIGHT (192)

// The CDG graphics area is divided into a 50x18 grid of 6x12 pixel cells.
#define CDG_WIDTH_CELLS (50)
#define CDG_HEIGHT_CELLS (18)
#define CDG_CELL_WIDTH (6)
#define CDG_CELL_HEIGHT (12)

// Each CDG frame is 1/300th of a second.
#define CDG_FRAME_DURATION_MS (1000.0/300.0)

// Forward offset.
#define HYSTERESIS_MS (100)
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
// represent this as a 12-bit colour, so it should never accidentally happen.
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
const WCHAR* g_backgroundWindowClassName = L"CDGProGB";
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
unsigned char* g_pMaskBitmapBits = NULL;
// The DC containing the mask for the CDG graphics, if we want to render a transparent background.
HDC g_hMaskedForegroundDC = NULL;
HBITMAP g_hMaskedForegroundBitmap = NULL;
unsigned char* g_pMaskedForegroundBitmapBits = NULL;
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
CDGPacket *g_pCDGData = NULL;
int g_nCDGPackets = 0;
int g_nCDGPC = 0;
// This structure contains plugin information, version, name...
// GPPHDR_VER is the version of the winampGeneralPurposePlugin (GPP) structure
winampGeneralPurposePlugin plugin = {
	GPPHDR_VER,  // version of the plugin, defined in "gen_myplugin.h"
	PLUGIN_NAME, // name/title of the plugin, defined in "gen_myplugin.h"
	init,        // function name which will be executed on init event
	config,      // function name which will be executed on config event
	quit,        // function name which will be executed on quit event
	0,           // handle to Winamp main window, loaded by winamp when this dll is loaded
	0            // hinstance to this dll, loaded by winamp when this dll is loaded
};
// We keep track of the last "reset" color. If we receive a MemoryPreset command for this color
// again before anything else has been drawn, we can ignore it.
BYTE g_nLastMemoryPresetColor = -1;
// Handle and ID of the processing thread, as well as an event for stopping it.
HANDLE g_hCDGProcessingThread = NULL;
DWORD g_nCDGProcessingThreadID = 0;
HANDLE g_hStopCDGProcessingEvent = NULL;
HANDLE g_hSongLoadedEvent = NULL;


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

void MemoryPreset(BYTE color, BYTE repeat) {
	if (g_nLastMemoryPresetColor != color) {
		memset(g_pForegroundBitmapBits, (color << 4) | color, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
		g_nLastMemoryPresetColor = color;
	}
}

void BorderPreset(BYTE color) {
	byte colorByte = (color << 4) | color;
	// Top and bottom edge.
	memset(g_pForegroundBitmapBits, colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	memset(g_pForegroundBitmapBits+(((CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) - (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT)) / 2), colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	// Left and right edge.
	for (int f = CDG_CELL_HEIGHT; f < CDG_CELL_HEIGHT +CDG_CANVAS_HEIGHT; ++f) {
		memset(g_pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
		memset(g_pForegroundBitmapBits + ((((f + 1) * CDG_BITMAP_WIDTH) / 2) - (CDG_CELL_WIDTH / 2)), colorByte, CDG_CELL_WIDTH / 2);
	}
}

bool TileBlock(byte* pData,bool isXor) {
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
	int foregroundBitmapOffset = ((xPixel)+(yPixel*CDG_BITMAP_WIDTH))/2;
	// The remaining 12 bytes in the data field will contain the bitmask of pixels to set.
	// The lower six bits of each byte are the pixel mask.
	for (int f = 4; f < 16; ++f) {
		byte bits = pData[f];
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
		foregroundBitmapOffset += CDG_BITMAP_WIDTH / 2;
	}
	// Screen is no longer blank.
	g_nLastMemoryPresetColor = -1;
	// Return true ONLY if we wrote to non-border screen area.
	return row < (CDG_HEIGHT_CELLS-1) && row>0 && col < (CDG_WIDTH_CELLS-1) && col>0;
}

bool LoadColorTable(byte* pData, int nPaletteStartIndex) {
	RGBQUAD palette[8];
	for (int f = 0; f < 8; ++f) {
		byte colorByte1 = pData[f*2] & 0x3F;
		byte colorByte2 = pData[(f*2)+1] & 0x3F;
		// Get 4-bit color values.
		byte red = (colorByte1 >> 2) & 0x0F;
		byte green = ((colorByte1 << 2) & 0x0C)| ((colorByte2 >> 4) & 0x03);
		byte blue = colorByte2 & 0x0F;
		// Convert to 24-bit color.
		red = (red * 16) + red;
		green = (green * 16) + green;
		blue = (blue * 16) + blue;
		palette[f].rgbRed = red;
		palette[f].rgbGreen = green;
		palette[f].rgbBlue = blue;
		palette[f].rgbReserved = 0;
	}
	::SetDIBColorTable(g_hForegroundDC, nPaletteStartIndex, 8, palette);
	if (!nPaletteStartIndex) {
		// RGB macro, for some reason, encodes as BGR
		*g_pBackgroundBitmapBits = RGB(palette[0].rgbBlue, palette[0].rgbGreen, palette[0].rgbRed);
	}
	// TODO: Return true ONLY if the background color has changed.
	return true;
}

bool Scroll(byte color,byte hScroll,byte hScrollOffset,byte vScroll,byte vScrollOffset,bool copy) {
	// TODO
	// Return true ONLY if we wrote to non-border screen area.
	return true;
}

byte ProcessCDGPackets() {
	byte result = 0;
	if (g_nCDGPC < g_nCDGPackets) {
		// Get current song position in milliseconds.
		int songPosition = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
		if (songPosition != -1) {
			songPosition += HYSTERESIS_MS;
			int cdgFrameIndex = (int)(songPosition / CDG_FRAME_DURATION_MS);
			// If the target frame is BEFORE the current CDGPC, the user has rewound the song.
			// Due to the XORing nature of CDG graphics, we have to start from the start!
			if (cdgFrameIndex < g_nCDGPC) {
//				WCHAR g[33];
//				wsprintf(g, L"%ld %ld", cdgFrameIndex,g_nCDGPC);
//				::MessageBox(NULL, g, L"", MB_OK);
				g_nCDGPC = 0;
			}
			while (g_nCDGPC < cdgFrameIndex && ::WaitForSingleObject(g_hStopCDGProcessingEvent, 0) == WAIT_TIMEOUT) {
				CDGPacket* pCDGPacket = g_pCDGData + g_nCDGPC;
				if ((pCDGPacket->command & 0x3F) == 0x09) {
					BYTE instr = pCDGPacket->instruction & 0x3F;
					switch (instr) {
					case CDG_INSTR_MEMORY_PRESET:
						MemoryPreset(pCDGPacket->data[0] & 0x0F, pCDGPacket->data[1] & 0x0F);
						result |= 0x01;
						break;
					case CDG_INSTR_BORDER_PRESET:
						BorderPreset(pCDGPacket->data[0] & 0x0F);
						break;
					case CDG_INSTR_TILE_BLOCK:
						result |= (TileBlock(pCDGPacket->data, false) ? 0x01 : 0x00);
						break;
					case CDG_INSTR_TILE_BLOCK_XOR:
						result |= (TileBlock(pCDGPacket->data, true) ? 0x01 : 0x00);
						break;
					case CDG_INSTR_SCROLL_PRESET:
						result |= (Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x07, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x07, false) ? 0x01 : 0x00);
						break;
					case CDG_INSTR_SCROLL_COPY:
						result |= (Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x07, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x07, true) ? 0x01 : 0x00);
						break;
					case CDG_INSTR_TRANSPARENT_COLOR:
						// Not implemented.
						break;
					case CDG_INSTR_LOAD_COLOR_TABLE_LOW:
						result |= (LoadColorTable(pCDGPacket->data, 0) ? 0x03 : 0x01);
						break;
					case CDG_INSTR_LOAD_COLOR_TABLE_HIGH:
						result |= (LoadColorTable(pCDGPacket->data, 8) ? 0x03 : 0x01);
						break;
					default:
						break;
					}
				}
				g_nCDGPC++;
			}
			// After the loop, the PC will be 1 frame too far on. Bring it back.
			//--g_nCDGPC;
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

	::ZeroMemory(g_pMaskBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 8));
	for (int f = -1; f < 2; ++f)
		for (int g = -1; g < 2; ++g)
			::BitBlt(g_hMaskDC, f, g, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hForegroundDC, 0, 0, MERGEPAINT);
	::MaskBlt(g_hMaskedForegroundDC, 0, 0, CDG_BITMAP_WIDTH,CDG_BITMAP_HEIGHT, g_hForegroundDC, 0, 0, g_hMaskBitmap, 0, 0, MAKEROP4(SRCCOPY,PATCOPY));
	::StretchBlt(g_hForegroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hMaskedForegroundDC, 0, 0, CDG_WIDTH, CDG_HEIGHT, SRCCOPY);
}

DWORD WINAPI CDGProcessor(LPVOID pParams) {
	HANDLE waitHandles[] = { g_hSongLoadedEvent, g_hStopCDGProcessingEvent };
	for(;;) {
		int waitResult=::WaitForMultipleObjects(2, waitHandles, FALSE, INFINITE);
		if (waitResult == 1)
			break;
		while (::WaitForSingleObject(g_hStopCDGProcessingEvent, SCREEN_REFRESH_MS) == WAIT_TIMEOUT) {
			byte result = ProcessCDGPackets();
			if (result & 0x01)
				::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
			if (result & 0x02)
				::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
		}
	}
	return 0;
}

bool StartCDGProcessingThread() {
	g_hStopCDGProcessingEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hSongLoadedEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hCDGProcessingThread=::CreateThread(NULL,0,CDGProcessor,NULL,0,&g_nCDGProcessingThreadID);
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
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0,0,w, h, SWP_NOMOVE);
		break;
	}
	case WM_PAINT:
		DrawForeground();
		break;
	case WM_NCMOUSEMOVE:
		//SetLayeredWindowAttributes(g_hWnd, 0, 0, 0);
		break;
	case WM_NCMOUSELEAVE:
		//SetLayeredWindowAttributes(g_hWnd, RGB(45, 167, 209), 60, LWA_COLORKEY);
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
	wndClass.hbrBackground = (HBRUSH)::GetStockObject(NULL_BRUSH);
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
	wndClass.hbrBackground = (HBRUSH)::GetStockObject(NULL_BRUSH);
	wndClass.lpszMenuName = NULL;
	wndClass.lpszClassName = g_backgroundWindowClassName;
	wndClass.hIconSm = NULL;

	return RegisterClassEx(&wndClass);
}

bool CreateForegroundWindow() {
	DWORD styles = WS_VISIBLE | WS_THICKFRAME;
	g_hForegroundWindow = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_NOACTIVATE | WS_EX_STATICEDGE,
		g_foregroundWindowClassName,
		g_foregroundWindowClassName,
		styles,
		50, 50,
		400, 300,
		plugin.hwndParent,
		NULL,
		plugin.hDllInstance,
		NULL);
	if (g_hForegroundWindow) {
		g_hForegroundWindowDC = ::GetDC(g_hForegroundWindow);
		if (g_hForegroundWindowDC) {
			::SetLayeredWindowAttributes(g_hForegroundWindow, DEFAULT_TRANSPARENT_COLOR, 255, LWA_COLORKEY);
			::SetWindowLong(g_hForegroundWindow, GWL_STYLE, styles);
			::SetWindowPos(g_hForegroundWindow, NULL, 50, 50, 400, 300, SWP_FRAMECHANGED);
			return true;
		}
	}
	return false;
}

bool CreateBackgroundWindow() {
	DWORD styles = WS_VISIBLE;
	g_hBackgroundWindow = CreateWindowEx(
		WS_EX_LAYERED | WS_EX_NOACTIVATE,
		g_backgroundWindowClassName,
		g_backgroundWindowClassName,
		styles,
		50, 50,
		400, 300,
		plugin.hwndParent,
		NULL,
		plugin.hDllInstance,
		NULL);
	if (g_hBackgroundWindow) {
		g_hBackgroundWindowDC = ::GetDC(g_hBackgroundWindow);
		if (g_hBackgroundWindowDC) {
			::SetLayeredWindowAttributes(g_hBackgroundWindow, 0, 127, LWA_ALPHA);
			::SetWindowLong(g_hBackgroundWindow, GWL_STYLE, styles);
			::SetWindowPos(g_hBackgroundWindow, NULL, 50, 50, 400, 300, SWP_FRAMECHANGED);
			return true;
		}
	}
	return false;
}

bool CreateForegroundDC() {
	bool result = false;
	g_hForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hForegroundDC) {
		BITMAPINFO* pBitmapInfo = (BITMAPINFO*)malloc(sizeof(BITMAPINFO) + (sizeof(RGBQUAD) * 16));
		if (pBitmapInfo) {
			pBitmapInfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
			pBitmapInfo->bmiHeader.biWidth = CDG_BITMAP_WIDTH;
			pBitmapInfo->bmiHeader.biHeight = -CDG_BITMAP_HEIGHT;
			pBitmapInfo->bmiHeader.biPlanes = 1;
			pBitmapInfo->bmiHeader.biBitCount = 4;
			pBitmapInfo->bmiHeader.biCompression = BI_RGB;
			pBitmapInfo->bmiHeader.biSizeImage = 0;
			pBitmapInfo->bmiHeader.biXPelsPerMeter = 0;
			pBitmapInfo->bmiHeader.biYPelsPerMeter = 0;
			pBitmapInfo->bmiHeader.biClrUsed = 16;
			pBitmapInfo->bmiHeader.biClrImportant = 0;
			for (int f = 0; f < 16; ++f) {
				pBitmapInfo->bmiColors[f].rgbReserved = 0;
				pBitmapInfo->bmiColors[f].rgbRed = 
				pBitmapInfo->bmiColors[f].rgbGreen = 
				pBitmapInfo->bmiColors[f].rgbBlue = (f * 15)+f;
			}

			g_hForegroundBitmap = ::CreateDIBSection(g_hForegroundWindowDC, pBitmapInfo, 0, (void**)&g_pForegroundBitmapBits, NULL, 0);
			if (g_hForegroundBitmap) {
				::ZeroMemory(g_pForegroundBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 2));
				::SelectObject(g_hForegroundDC, g_hForegroundBitmap);
				result = true;
			}
			free(pBitmapInfo);
		}
	}
	return result;
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
	bool result = false;
	g_hMaskDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskDC) {
		BITMAPINFO bitmapInfo;
		bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bitmapInfo.bmiHeader.biWidth = CDG_BITMAP_WIDTH;
		bitmapInfo.bmiHeader.biHeight = -CDG_BITMAP_HEIGHT;
		bitmapInfo.bmiHeader.biPlanes = 1;
		bitmapInfo.bmiHeader.biBitCount = 1;
		bitmapInfo.bmiHeader.biCompression = BI_RGB;
		bitmapInfo.bmiHeader.biSizeImage = 0;
		bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
		bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
		bitmapInfo.bmiHeader.biClrUsed = 2;
		bitmapInfo.bmiHeader.biClrImportant = 0;

		g_hMaskBitmap = ::CreateDIBSection(g_hForegroundWindowDC, &bitmapInfo, 0, (void**)&g_pMaskBitmapBits, NULL, 0);
		if (g_hMaskBitmap) {
			::ZeroMemory(g_pMaskBitmapBits, (((CDG_BITMAP_WIDTH) * (CDG_BITMAP_HEIGHT)) / 8));
			::SelectObject(g_hMaskDC, g_hMaskBitmap);
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

void readCDGData(const WCHAR *pFileBeingPlayed) {
	WCHAR pathBuffer[MAX_PATH + 1];
	char zipEntryName[MAX_PATH + 1];
	zip_stat_t fileStat;

	wcscpy_s(pathBuffer, pFileBeingPlayed);
	pathBuffer[MAX_PATH] = '\0';
	_wcslwr_s(pathBuffer);
	if (wcsstr(pathBuffer, L"zip://")) {
		// Format of string will be zip://somepathtoazipfile,n
		// n will be the indexed entry in the zip file that is being played.
		// First of all, get rid of the zip:// bit.
		wcscpy_s(pathBuffer, pathBuffer + 6);
		// Now we need to get rid of the ,n bit
		WCHAR *pComma = wcsrchr(pathBuffer, ',');
		int zipIndex = -1;
		if (pComma) {
			zipIndex = _wtoi(pComma + 1);
			*pComma = '\0';
		}
		// OK, we now have the path to the zip file. We can go ahead and read it, looking for the
		// CDG file.
		FILE *pFile;
		errno_t fileError = _wfopen_s(&pFile, pathBuffer, L"rb");
		if (!fileError && pFile) {
			zip_source_t *pZipSource = zip_source_filep_create(pFile, 0, 0, NULL);
			if (pZipSource) {
				zip_t *pZip = zip_open_from_source(pZipSource, ZIP_RDONLY, NULL);
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
		WCHAR *pDot = wcsrchr(pathBuffer, '.');
		WCHAR *pSlash = wcsrchr(pathBuffer, '\\');
		if (pDot > pSlash)
			*pDot = '\0';
		wcscat_s(pathBuffer, L".cdg");
		FILE *pFile=NULL;
		errno_t error = _wfopen_s(&pFile, pathBuffer, L"rb");
		if (!error && pFile) {
			fseek(pFile, 0, SEEK_END);
			int size = ftell(pFile);
			g_nCDGPackets = size / sizeof(CDGPacket);
			fseek(pFile, 0, SEEK_SET);
			g_pCDGData = (CDGPacket *)malloc(g_nCDGPackets*sizeof(CDGPacket));
			if (g_pCDGData)
				fread(g_pCDGData, sizeof(CDGPacket), g_nCDGPackets, pFile);
			fclose(pFile);
		}
	}
}

LRESULT CALLBACK CdgProWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
		case WM_WA_IPC:
			switch (lParam) {
				case IPC_PLAYING_FILEW: {
					const WCHAR *fileBeingPlayed = (const WCHAR *)wParam;
					clearExistingCDGData();
					readCDGData(fileBeingPlayed);
					if (g_pCDGData) {
						g_nCDGPC = 0;
						::SetEvent(g_hSongLoadedEvent);
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
		if(RegisterBackgroundWindowClass())
			if(RegisterForegroundWindowClass())
				if(CreateBackgroundWindow())
					if(CreateForegroundWindow())
						if (CreateBackgroundDC())
							if (CreateForegroundDC())
								if (CreateMaskDC())
									if (CreateMaskedForegroundDC())
										if(StartCDGProcessingThread()){
										}

	g_pOriginalWndProc = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);
	return 0;
}

void config() {
}

void quit() {
	::SetEvent(g_hStopCDGProcessingEvent);
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

	if (g_hMaskedForegroundDC)
		::DeleteDC(g_hMaskedForegroundDC);
	if (g_hMaskedForegroundBitmap)
		::DeleteObject(g_hMaskedForegroundBitmap);

	if (g_hTransparentBrush)
		::DeleteObject(g_hTransparentBrush);

	::CloseHandle(g_hStopCDGProcessingEvent);
	::CloseHandle(g_hSongLoadedEvent);

	UnregisterClass(g_foregroundWindowClassName, plugin.hDllInstance);
	UnregisterClass(g_backgroundWindowClassName, plugin.hDllInstance);
}