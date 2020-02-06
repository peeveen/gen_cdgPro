#include "stdafx.h"
#include <windowsx.h>
#include "CDGDefs.h"
#include "CDGGlobals.h"
#include "CDGWindows.h"
#include "resource.h"

// Window state
bool g_bFullScreen = false;
RECT g_lastSize;
// The context menu
HMENU g_hMenu = NULL;

void SetFullScreen(bool fullscreen)
{
	if (g_bFullScreen != fullscreen) {
		if (!g_bFullScreen)
			::GetWindowRect(g_hForegroundWindow, &g_lastSize);
		g_bFullScreen = fullscreen;
		if (g_bFullScreen)
		{
			// Remove frame from window
			DWORD currentStyle = ::GetWindowLong(g_hForegroundWindow, GWL_STYLE);
			::SetWindowLong(g_hForegroundWindow, GWL_STYLE, currentStyle & ~WS_THICKFRAME);
			// Figure out what screen we're on, and what size it is.
			MONITORINFO monitorInfo;
			monitorInfo.cbSize = sizeof(monitorInfo);
			::GetMonitorInfo(MonitorFromWindow(g_hForegroundWindow, MONITOR_DEFAULTTONEAREST), &monitorInfo);
			RECT window_rect(monitorInfo.rcMonitor);
			::SetWindowPos(g_hForegroundWindow, NULL, window_rect.left, window_rect.top, window_rect.right - window_rect.left, window_rect.bottom - window_rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
		else
		{
			// Reset original window style and size.
			DWORD currentStyle = ::GetWindowLong(g_hForegroundWindow, GWL_STYLE);
			::SetWindowLong(g_hForegroundWindow, GWL_STYLE, currentStyle | WS_THICKFRAME);
			RECT new_rect(g_lastSize);
			::SetWindowPos(g_hForegroundWindow, NULL, new_rect.left, new_rect.top, new_rect.right - new_rect.left, new_rect.bottom - new_rect.top, SWP_NOZORDER | SWP_NOACTIVATE | SWP_FRAMECHANGED);
		}
	}
}

INT_PTR AboutDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	switch (msg)
	{
	case WM_INITDIALOG: {
		RECT rParent, rThis, rCentered;
		::GetWindowRect(g_hForegroundWindow, &rParent);
		::GetWindowRect(hwnd, &rThis);
		::CopyRect(&rCentered, &rParent);
		::OffsetRect(&rThis, -rThis.left, -rThis.top);
		::OffsetRect(&rCentered, -rCentered.left, -rCentered.top);
		::OffsetRect(&rCentered, -rThis.right, -rThis.bottom);
		::SetWindowPos(hwnd, HWND_TOP, rParent.left + (rCentered.right / 2), rParent.top + (rCentered.bottom / 2), 0, 0, SWP_NOSIZE);
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			EndDialog(hwnd, wParam);
			return TRUE;
		}
	}
	return FALSE;
}

void ShowAboutDialog() {
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTDIALOG), g_hForegroundWindow, (DLGPROC)AboutDialogProc);
}

void ShowMenu(int xPos,int yPos) {
	MENUITEMINFO menuItemInfo;
	::ZeroMemory(&menuItemInfo, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_STATE;
	int command = ::TrackPopupMenu(g_hMenu, TPM_RETURNCMD, xPos, yPos, 0, g_hForegroundWindow, NULL);
	switch (command) {
	case MENUITEM_TOPMOST_ID: {
		DWORD currentExStyle = ::GetWindowLong(g_hForegroundWindow, GWL_EXSTYLE);
		if (currentExStyle & WS_EX_TOPMOST) {
			menuItemInfo.fState = MFS_UNCHECKED;
			::SetWindowLong(g_hForegroundWindow, GWL_EXSTYLE, currentExStyle &= ~WS_EX_TOPMOST);
			::SetMenuItemInfo(g_hMenu, MENUITEM_TOPMOST_ID, FALSE, &menuItemInfo);
			::SetWindowPos(g_hForegroundWindow, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		else {
			menuItemInfo.fState = MFS_CHECKED;
			::SetWindowLong(g_hForegroundWindow, GWL_EXSTYLE, currentExStyle | WS_EX_TOPMOST);
			::SetMenuItemInfo(g_hMenu, MENUITEM_TOPMOST_ID, FALSE, &menuItemInfo);
			::SetWindowPos(g_hForegroundWindow, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		}
		break;
	}
	case MENUITEM_FULLSCREEN_ID:
		if (g_bFullScreen) {
			menuItemInfo.fState = MFS_UNCHECKED;
			::SetMenuItemInfo(g_hMenu, MENUITEM_FULLSCREEN_ID, FALSE, &menuItemInfo);
			SetFullScreen(false);
		}
		else {
			menuItemInfo.fState = MFS_CHECKED;
			::SetMenuItemInfo(g_hMenu, MENUITEM_FULLSCREEN_ID, FALSE, &menuItemInfo);
			SetFullScreen(true);
		}
		break;
	case MENUITEM_ABOUT_ID:
		ShowAboutDialog();
	default:
		break;
	}
}

bool CreateRightClickMenu() {
	g_hMenu = ::CreatePopupMenu();
	if (g_hMenu) {
		HBITMAP checked = (HBITMAP)::LoadImage(g_hInstance, MAKEINTRESOURCE(IDB_CHECKED), IMAGE_BITMAP, 16, 16, LR_MONOCHROME | LR_LOADTRANSPARENT);
		HBITMAP unchecked = (HBITMAP)::LoadImage(g_hInstance, MAKEINTRESOURCE(IDB_UNCHECKED), IMAGE_BITMAP, 16, 16, LR_MONOCHROME | LR_LOADTRANSPARENT);
		::SetMenuItemBitmaps(g_hMenu, MENUITEM_TOPMOST_ID, MF_BYCOMMAND, unchecked, checked);
		::AppendMenu(g_hMenu, MF_UNCHECKED | MF_ENABLED | MF_STRING, MENUITEM_TOPMOST_ID, L"Always On Top");
		::AppendMenu(g_hMenu, MF_UNCHECKED | MF_ENABLED | MF_STRING, MENUITEM_FULLSCREEN_ID, L"Full Screen");
		::AppendMenu(g_hMenu, MF_SEPARATOR, 0, NULL);
		::AppendMenu(g_hMenu, MF_ENABLED | MF_STRING, MENUITEM_ABOUT_ID, L"About");
		return true;
	}
	return false;
}

void DestroyRightClickMenu() {
	if (g_hMenu)
		::DestroyMenu(g_hMenu);
}