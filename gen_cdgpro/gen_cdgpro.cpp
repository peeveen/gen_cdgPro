#include "stdafx.h"
#include <wchar.h>
#include <windows.h>
#include <windowsx.h>
#include <objidl.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;
#include "resource.h"

#include "CDGDefs.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGInstructionHandlers.h"
#include "CDGBackgroundFunctions.h"
#include "CDGMenu.h"
#include "CDGReader.h"
#include "CDGRender.h"
#include "CDGWindows.h"
#include "CDGBitmaps.h"

int init(void);
void config(void);
void quit(void);

// Original WndProc that we have to swap back in at the end of proceedings.
WNDPROC g_pOriginalWndProc;
// Current CDG instruction index.
int g_nCDGPC = 0;
// This structure contains plugin information, version, name...
winampGeneralPurposePlugin plugin = { GPPHDR_VER,PLUGIN_NAME,init,config,quit,0,0 };
// Handle and ID of the processing thread, as well as an event for stopping it.
HANDLE g_hCDGProcessingThread = NULL;
DWORD g_nCDGProcessingThreadID = 0;
// Cross thread communication events
HANDLE g_hStopCDGProcessingEvent = NULL;
HANDLE g_hStoppedCDGProcessingEvent = NULL;
HANDLE g_hStopCDGThreadEvent = NULL;
HANDLE g_hSongLoadedEvent = NULL;
// GDI+ token
ULONG_PTR g_gdiPlusToken;

// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &plugin;
}

byte ProcessCDGPackets() {
	byte result = 0;
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent };
	if (g_nCDGPC < g_nCDGPackets) {
		// Get current song position in milliseconds (see comment about rewind tolerance).
		LRESULT songPosition = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_GETOUTPUTTIME);
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
			size_t nStrLen = (wcslen(pszSongTitle) + 1);
			WCHAR* pszSongTitleCopy = (WCHAR*)malloc(sizeof(WCHAR) * nStrLen);
			if (pszSongTitleCopy) {
				wcscpy_s(pszSongTitleCopy, nStrLen, pszSongTitle);
				::CreateThread(NULL, 0, StartSongThread, (LPVOID)pszSongTitleCopy, 0, &nStartSongThreadID);
			}
			break;
		}
		case IPC_CB_MISC:
			if (wParam == IPC_CB_MISC_STATUS) {
				LRESULT isPlayingResult = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING);
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

int init() {
	g_hInstance = plugin.hDllInstance;
	g_hWinampWindow = plugin.hwndParent;
	GdiplusStartupInput g_gdiPlusStartupInput;
	::GdiplusStartup(&g_gdiPlusToken, &g_gdiPlusStartupInput, NULL);
	LoadLogo();
	if(CreateRightClickMenu())
		if(CreateWindows())
			if (CreateBitmaps())
				if (StartCDGProcessingThread())
					ClearCDGBuffer();

	g_pOriginalWndProc = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);
	return 0;
}

void config() {
}

void quit() {
	::SetEvent(g_hStopCDGProcessingEvent);
	::SetEvent(g_hStopCDGThreadEvent);
	::WaitForSingleObject(g_hCDGProcessingThread, INFINITE);

	::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);

	clearExistingCDGData();

	::CloseHandle(g_hStopCDGProcessingEvent);
	::CloseHandle(g_hStopCDGThreadEvent);
	::CloseHandle(g_hSongLoadedEvent);

	DestroyRightClickMenu();
	DestroyLogo();

	::GdiplusShutdown(g_gdiPlusToken);
}