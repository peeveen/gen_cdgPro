#include "stdafx.h"
#include <windowsx.h>
#include "CDGGlobals.h"
#include "CDGInstructionHandlers.h"
#include "CDGWindows.h"
#include "resource.h"

// The context menu
HMENU g_hMenu = NULL;
// The channels submenu
HMENU g_hChannelsMenu = NULL;
// When displaying the about dialog, we need to cater for the possibility that the main window will be topmost.
// The dialog proc will need to remove/reset the topmostness.
bool g_bDialogSetsTopmost = false;

void SetMenuItemCheckmark(UINT nMenuItemID, bool set) {
	MENUITEMINFO menuItemInfo;
	::ZeroMemory(&menuItemInfo, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_STATE;
	menuItemInfo.fState = set ? MFS_CHECKED : MFS_UNCHECKED;
	::SetMenuItemInfo(g_hMenu, nMenuItemID, FALSE, &menuItemInfo);
}

void ToggleTopmost() {
	DWORD currentExStyle = ::GetWindowLong(g_hLogoWindow, GWL_EXSTYLE);
	bool currentlyTopmost = !!(currentExStyle & WS_EX_TOPMOST);
	::SetWindowLong(g_hLogoWindow, GWL_EXSTYLE, currentExStyle ^ WS_EX_TOPMOST);
	::SetWindowPos(g_hLogoWindow, currentlyTopmost ? HWND_NOTOPMOST : HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetMenuItemCheckmark(MENUITEM_TOPMOST_ID, !currentlyTopmost);
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
		if (g_bDialogSetsTopmost)
			ToggleTopmost();
		return TRUE;
	}
	case WM_COMMAND:
		switch (LOWORD(wParam))
		{
		case IDOK:
			if (g_bDialogSetsTopmost)
				ToggleTopmost();
			EndDialog(hwnd, wParam);
			return TRUE;
		}
	}
	return FALSE;
}

void ShowAboutDialog() {
	// Remove topmost attributes, otherwise the user will never see the about dialog.
	DWORD currentExStyle = ::GetWindowLong(g_hLogoWindow, GWL_EXSTYLE);
	g_bDialogSetsTopmost = !!(currentExStyle & WS_EX_TOPMOST);
	::DialogBox(g_hInstance, MAKEINTRESOURCE(IDD_ABOUTDIALOG), g_hLogoWindow, (DLGPROC)AboutDialogProc);
}

void ToggleFullScreen() {
	bool currentFullScreenState = IsFullScreen();
	SetFullScreen(!currentFullScreenState);
	SetMenuItemCheckmark(MENUITEM_FULLSCREEN_ID, !currentFullScreenState);
}

void ShowMenu(int xPos,int yPos) {
	int command = ::TrackPopupMenu(g_hMenu, TPM_RETURNCMD, xPos, yPos, 0, g_hForegroundWindow, NULL);
	if (command >= MENUITEM_CHANNEL && command <= MENUITEM_CHANNEL + 16) {
		int channel = command - MENUITEM_CHANNEL;
		unsigned short nChannelBit = 1 << channel;
		g_nChannelMask ^= nChannelBit;
		SetMenuItemCheckmark(command, g_nChannelMask & nChannelBit);
	}
	else
		switch (command) {
			case MENUITEM_TOPMOST_ID:
				ToggleTopmost();
				break;
			case MENUITEM_FULLSCREEN_ID:
				ToggleFullScreen();
				break;
			case MENUITEM_ABOUT_ID:
				ShowAboutDialog();
			default:
				break;
		}
}

bool CreateRightClickMenu() {
	g_hChannelsMenu = ::CreatePopupMenu();
	if (g_hChannelsMenu) {
		WCHAR szChannel[24];
		for (int f = 0; f < 16; ++f) {
			wsprintf(szChannel, L"Channel %d", f);
			::AppendMenu(g_hChannelsMenu, ((g_nChannelMask & (1 << f)) ? MF_CHECKED : MF_UNCHECKED) | MF_ENABLED | MF_STRING, MENUITEM_CHANNEL + f, szChannel);
		}
		g_hMenu = ::CreatePopupMenu();
		if (g_hMenu) {
			::AppendMenu(g_hMenu, MF_UNCHECKED | MF_ENABLED | MF_STRING, MENUITEM_TOPMOST_ID, L"Always On Top");
			::AppendMenu(g_hMenu, MF_UNCHECKED | MF_ENABLED | MF_STRING, MENUITEM_FULLSCREEN_ID, L"Full Screen");
			::AppendMenu(g_hMenu, MF_SEPARATOR, 0, NULL);
			::AppendMenu(g_hMenu, MF_ENABLED | MF_STRING|MF_POPUP, (UINT_PTR)g_hChannelsMenu, L"Channels");
			::AppendMenu(g_hMenu, MF_SEPARATOR, 0, NULL);
			::AppendMenu(g_hMenu, MF_ENABLED | MF_STRING, MENUITEM_ABOUT_ID, L"About");
			return true;
		}
	}
	return false;
}

void DestroyRightClickMenu() {
	if (g_hMenu)
		::DestroyMenu(g_hMenu);
	if (g_hChannelsMenu)
		::DestroyMenu(g_hChannelsMenu);
}