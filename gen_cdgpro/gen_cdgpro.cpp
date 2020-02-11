#include "stdafx.h"
#include <shellapi.h>
#include <malloc.h>
#include <objidl.h>
#include <gdiplus.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;
#include "resource.h"

#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGBackgroundFunctions.h"
#include "CDGPalette.h"
#include "CDGMenu.h"
#include "CDGRender.h"
#include "CDGWindows.h"
#include "CDGBitmaps.h"
#include "CDGProcessor.h"

int init(void);
void config(void);
void quit(void);

// The instance handle of this DLL.
HINSTANCE g_hInstance = NULL;
// Handle to the Winamp window.
HWND g_hWinampWindow = NULL;

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

void Stop() {
	ResetProcessor();
	ShowWindows(false);
	::ZeroMemory(g_pScaledForegroundBitmapBits[0], (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
	::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
}

DWORD WINAPI StartSongThread(LPVOID pParams) {
	// By asking for the filename of the currently playing playlist entry, we get
	// the path to the extracted media content that in_zip created. This saves
	// us having to unzip it ourselves.
	int listPos = ::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETLISTPOS);
	const WCHAR* fileBeingPlayed = (const WCHAR*)::SendMessage(g_hWinampWindow, WM_WA_IPC, listPos, IPC_GETPLAYLISTFILEW);
	if (fileBeingPlayed) {
		bool isCDGProcessorRunning = ::WaitForSingleObject(g_hStoppedCDGProcessingEvent, 0) == WAIT_TIMEOUT;
		if (isCDGProcessorRunning) {
			::SetEvent(g_hStopCDGProcessingEvent);
			::WaitForSingleObject(g_hStoppedCDGProcessingEvent, INFINITE);
			::ResetEvent(g_hStopCDGProcessingEvent);
		}
		else
			::SetEvent(g_hStoppedCDGProcessingEvent);
		if (ReadCDGData(fileBeingPlayed)) {
			ShowWindows(true);
			::SetEvent(g_hSongLoadedEvent);
		}
		else
			Stop();
	}
	return 0;
}

LRESULT CALLBACK CdgProWndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg) {
	case WM_WA_IPC:
		switch (lParam) {
		case IPC_PLAYING_FILEW: {
			// This provides a path to the file being playing, but if it's a zip, that's
			// what you get: the path to the ZIP. We want the path to the extracted
			// media file, which is in TEMP somewhere. That's what the various
			// SendMessage calls in the StartSongThread method do.
			::CreateThread(NULL, 0, StartSongThread, NULL, 0, NULL);
			break;
		}
		case IPC_CB_MISC:
			if (wParam == IPC_CB_MISC_STATUS) {
				LRESULT isPlayingResult = ::SendMessage(plugin.hwndParent, WM_WA_IPC, 0, IPC_ISPLAYING);
				if (!isPlayingResult)
					Stop();
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
	g_pOriginalWndProc = (WNDPROC)::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)CdgProWndProc);

	ReadPrefs();

	GdiplusStartupInput g_gdiPlusStartupInput;
	::GdiplusStartup(&g_gdiPlusToken, &g_gdiPlusStartupInput, NULL);

	CreateRightClickMenu();
	CreateWindows();
	CreateBitmaps();
	StartCDGProcessor();

	ShowWindows(false);

	return 0;
}

void config() {
	::ShellExecute(g_hWinampWindow, L"edit", g_szINIPath, NULL, NULL, SW_SHOWDEFAULT);
}

void quit() {
	DestroyWindows();
	DestroyRightClickMenu();
	StopCDGProcessor();
	DestroyBitmaps();
	::GdiplusShutdown(g_gdiPlusToken);
	::SetWindowLong(plugin.hwndParent, GWL_WNDPROC, (LONG)g_pOriginalWndProc);
}