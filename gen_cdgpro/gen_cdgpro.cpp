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
#include "CDGProcessor.h"

int init(void);
void config(void);
void quit(void);

// Original WndProc that we have to swap back in at the end of proceedings.
WNDPROC g_pOriginalWndProc;
// This structure contains plugin information, version, name...
winampGeneralPurposePlugin plugin = { GPPHDR_VER,PLUGIN_NAME,init,config,quit,0,0 };
// GDI+ token
ULONG_PTR g_gdiPlusToken;

// This is an export function called by winamp which returns this plugin info.
// We wrap the code in 'extern "C"' to ensure the export isn't mangled if used in a CPP file.
extern "C" __declspec(dllexport) winampGeneralPurposePlugin * winampGetGeneralPurposePlugin() {
	return &plugin;
}

DWORD WINAPI StartSongThread(LPVOID pParams) {
	::SetEvent(g_hStopCDGProcessingEvent);
	::WaitForSingleObject(g_hStoppedCDGProcessingEvent, INFINITE);
	::ResetEvent(g_hStopCDGProcessingEvent);
	const WCHAR* fileBeingPlayed = (const WCHAR*)pParams;
	clearExistingCDGData();
	if (readCDGData(fileBeingPlayed)) {
		g_nCurrentTransparentIndex = 0;
		g_bShowLogo = false;
		::SetEvent(g_hSongLoadedEvent);
	}
	free(pParams);
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
	CreateRightClickMenu();
	CreateWindows();
	CreateBitmaps();
	StartCDGProcessor();
	ClearCDGBuffer();

	g_pOriginalWndProc = (WNDPROC)SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);
	return 0;
}

void config() {
}

void quit() {
	StopCDGProcessor();
	::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);
	clearExistingCDGData();
	DestroyRightClickMenu();
	DestroyLogo();
	::GdiplusShutdown(g_gdiPlusToken);
}