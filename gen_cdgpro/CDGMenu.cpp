#include "stdafx.h"
#include <windowsx.h>
#include "CDGGlobals.h"
#include "CDGWindows.h"
#include "resource.h"

// The context menu
HMENU g_hMenu = NULL;

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

void ToggleFullScreen(MENUITEMINFO* pMenuItemInfo, bool currentFullScreenState) {
	pMenuItemInfo->fState = currentFullScreenState ? MFS_UNCHECKED : MFS_CHECKED;
	::SetMenuItemInfo(g_hMenu, MENUITEM_FULLSCREEN_ID, FALSE, pMenuItemInfo);
	SetFullScreen(!currentFullScreenState);
}

void ToggleTopmost(MENUITEMINFO* pMenuItemInfo, DWORD currentExStyle) {
	bool currentlyTopmost = !!(currentExStyle & WS_EX_TOPMOST);
	pMenuItemInfo->fState = currentlyTopmost ? MFS_UNCHECKED : MFS_CHECKED;
	::SetWindowLong(g_hForegroundWindow, GWL_EXSTYLE, currentExStyle ^ WS_EX_TOPMOST);
	::SetMenuItemInfo(g_hMenu, MENUITEM_TOPMOST_ID, FALSE, pMenuItemInfo);
	::SetWindowPos(g_hForegroundWindow, currentlyTopmost?HWND_NOTOPMOST:HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
}

void ShowMenu(int xPos,int yPos) {
	MENUITEMINFO menuItemInfo;
	::ZeroMemory(&menuItemInfo, sizeof(MENUITEMINFO));
	menuItemInfo.cbSize = sizeof(MENUITEMINFO);
	menuItemInfo.fMask = MIIM_STATE;
	int command = ::TrackPopupMenu(g_hMenu, TPM_RETURNCMD, xPos, yPos, 0, g_hForegroundWindow, NULL);
	switch (command) {
		case MENUITEM_TOPMOST_ID:
			ToggleTopmost(&menuItemInfo, ::GetWindowLong(g_hForegroundWindow, GWL_EXSTYLE));
			break;
		case MENUITEM_FULLSCREEN_ID:
			ToggleFullScreen(&menuItemInfo,IsFullScreen());
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