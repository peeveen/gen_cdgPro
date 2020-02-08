#include "stdafx.h"
#include <windowsx.h>
#include "CDGGlobals.h"
#include "CDGBitmaps.h"
#include "CDGPrefs.h"
#include "CDGMenu.h"
#include "CDGRender.h"
#include "resource.h"

// Window class names.
const WCHAR* g_foregroundWindowClassName = L"CDGProFG";
const WCHAR* g_backgroundWindowClassName = L"CDGProBG";
const WCHAR* g_logoWindowClassName = L"CDGProLogo";
const WCHAR* g_pszWindowCaption = L"CDG Pro";

// Application icon.
HICON g_hIcon = NULL;
// The window (and it's DC) containing the foreground.
HDC g_hForegroundWindowDC = NULL;
HWND g_hForegroundWindow = NULL;
// The window (and it's DC) containing the (optionally transparent) background.
HDC g_hBackgroundWindowDC = NULL;
HWND g_hBackgroundWindow = NULL;
// The window (and it's DC) containing the (optional) logo.
HDC g_hLogoWindowDC = NULL;
HWND g_hLogoWindow = NULL;
// Window size to restore to from fullscreen. If not currently in fullscreen mode, this is zero size.
RECT g_lastSize = { 0,0,0,0 };
// Offset between the logo window and the foreground window.
SIZE g_logoWindowOffset = { 0,0 };
// Blend function params for logo.
BLENDFUNCTION g_blendFn= { AC_SRC_OVER ,0,255,AC_SRC_ALPHA };

void UpdateLogoPosition() {
	::SetWindowPos(g_hForegroundWindow, g_hLogoWindow, 0, 0,0,0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	static RECT foregroundClientRect,foregroundWindowRect;
	::GetClientRect(g_hForegroundWindow, &foregroundClientRect);
	::GetWindowRect(g_hForegroundWindow, &foregroundWindowRect);
	POINT topLeft = { 0,0 };
	::ClientToScreen(g_hForegroundWindow, &topLeft);
	int clientRectWidth = foregroundClientRect.right - foregroundClientRect.left;
	int clientRectHeight = foregroundClientRect.bottom - foregroundClientRect.top;
	POINT logoSourcePoint = { 0,0 };
	RECT clientScreenRect = { topLeft.x,topLeft.y,topLeft.x + clientRectWidth,topLeft.y + clientRectHeight };

	int halfRemainderX = (g_logoSize.cx - clientRectWidth) >> 1;
	if (clientRectWidth < g_logoSize.cx)
		logoSourcePoint.x = halfRemainderX;
	else
		topLeft.x -= halfRemainderX;
	int halfRemainderY = (g_logoSize.cy - clientRectHeight) >> 1;
	if (clientRectHeight < g_logoSize.cy)
		logoSourcePoint.y = halfRemainderY;
	else
		topLeft.y -= halfRemainderY;

	RECT logoRect = { topLeft.x,topLeft.y,topLeft.x + g_logoSize.cx,topLeft.y + g_logoSize.cy };
	::IntersectRect(&logoRect, &logoRect, &clientScreenRect);
	SIZE logoSize = { logoRect.right-logoRect.left,logoRect.bottom- logoRect.top};
	g_logoWindowOffset = { foregroundWindowRect.left - topLeft.x,foregroundWindowRect.top - topLeft.y };
	::UpdateLayeredWindow(g_hLogoWindow, g_hScreenDC, &topLeft, &logoSize, g_hLogoDC, &logoSourcePoint, 0, &g_blendFn, ULW_ALPHA);
}

void ShowLogo(bool bShow) {
	::ShowWindow(g_hLogoWindow, bShow?SW_SHOW:SW_HIDE);
	DWORD exStyle = ::GetWindowLong(g_hForegroundWindow, GWL_EXSTYLE);
	if (bShow)
		exStyle &= ~WS_EX_APPWINDOW;
	else
		exStyle |= WS_EX_APPWINDOW;
	::SetWindowLong(g_hForegroundWindow, GWL_EXSTYLE, exStyle);
}

bool IsFullScreen() {
	return g_lastSize.right - g_lastSize.left > 0;
}

void SetFullScreen(bool fullscreen)
{
	bool currentlyFullScreen = IsFullScreen();
	if (currentlyFullScreen != fullscreen) {
		if (fullscreen)
		{
			::GetWindowRect(g_hForegroundWindow, &g_lastSize);
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
			g_lastSize = { 0,0,0,0 };
		}
	}
}

LRESULT CALLBACK ForegroundWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	static PAINTSTRUCT ps;
	switch (uMsg)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
		if (hit == HTCLIENT)
			hit = HTCAPTION;
		return hit;
	}
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE) {
			SetMenuItemCheckmark(MENUITEM_FULLSCREEN_ID, false);
			SetFullScreen(false);
		}
		break;
	case WM_SHOWWINDOW:
	case WM_WINDOWPOSCHANGED:
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		::SetWindowPos(g_hLogoWindow,NULL, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE|SWP_NOACTIVATE);
		::SetWindowPos(hwnd, g_hLogoWindow, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
		UpdateLogoPosition();
		break;
	case WM_NCRBUTTONUP: {
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		ShowMenu(xPos, yPos);
		break;
	}
	case WM_MOVE: {
		int x = (int)(short)LOWORD(lParam);
		int y = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, x, y, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
		UpdateLogoPosition();
		break;
	}
	case WM_SIZE: {
		int w = (int)(short)LOWORD(lParam);
		int h = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE);
		UpdateLogoPosition();
		break;
	}
	case WM_CLOSE:
		return 1;
	case WM_PAINT:
		::BeginPaint(g_hForegroundWindow, &ps);
		DrawForeground(&ps.rcPaint);
		::EndPaint(g_hForegroundWindow, &ps);
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

LRESULT CALLBACK LogoWindowProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	switch (uMsg)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
		if (hit == HTCLIENT)
			hit = HTCAPTION;
		return hit;
	}
	case WM_MOVE: {
		int x = (int)(short)LOWORD(lParam);
		int y = (int)(short)HIWORD(lParam);
		int newForegroundWindowX = x + g_logoWindowOffset.cx;
		int newForegroundWindowY = y + g_logoWindowOffset.cy;
		::SetWindowPos(g_hForegroundWindow, g_hLogoWindow, newForegroundWindowX, newForegroundWindowY, 0, 0, SWP_NOSIZE | SWP_NOACTIVATE);
		break;
	}
	case WM_NCRBUTTONUP: {
		int xPos = GET_X_LPARAM(lParam);
		int yPos = GET_Y_LPARAM(lParam);
		ShowMenu(xPos, yPos);
		break;
	}
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
		::SetWindowPos(g_hForegroundWindow,hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		break;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

ATOM RegisterWindowClass(const WCHAR* pszClassName, WNDPROC wndProc) {
	if (!g_hIcon)
		g_hIcon = ::LoadIcon(g_hInstance, MAKEINTRESOURCE(IDI_APPICON));
	if (g_hIcon) {
		WNDCLASSEX wndClass;
		wndClass.cbSize = sizeof(WNDCLASSEX);
		wndClass.style = CS_PARENTDC | CS_HREDRAW | CS_VREDRAW;
		wndClass.lpfnWndProc = (WNDPROC)wndProc;
		wndClass.cbClsExtra = 0;
		wndClass.cbWndExtra = 0;
		wndClass.hInstance = g_hInstance;
		wndClass.hIcon = g_hIcon;
		wndClass.hCursor = LoadCursor(NULL, (LPTSTR)IDC_ARROW);
		wndClass.hbrBackground = NULL;
		wndClass.lpszMenuName = NULL;
		wndClass.lpszClassName = pszClassName;
		wndClass.hIconSm = g_hIcon;
		return ::RegisterClassEx(&wndClass);
	}
	return 0;
}

ATOM RegisterForegroundWindowClass() {
	return RegisterWindowClass(g_foregroundWindowClassName, (WNDPROC)ForegroundWindowProc);
}

ATOM RegisterBackgroundWindowClass() {
	return RegisterWindowClass(g_backgroundWindowClassName, (WNDPROC)BackgroundWindowProc);
}

ATOM RegisterLogoWindowClass() {
	return RegisterWindowClass(g_logoWindowClassName, (WNDPROC)LogoWindowProc);
}

bool CreateCDGWindow(HWND* phWnd, HDC* phDC, const WCHAR* pszClassName, DWORD additionalStyles, DWORD additionalExStyles) {
	DWORD styles = WS_VISIBLE | additionalStyles;
	*phWnd = CreateWindowEx(
		WS_EX_LAYERED | additionalExStyles,
		pszClassName,
		g_pszWindowCaption,
		styles,
		50, 50,
		CDG_WIDTH, CDG_HEIGHT,
		// We don't want this window to minimize/restore along with WinAmp, so we don't
		// tell Windows that this is a child of WinAmp.
		NULL,//g_hWinampWindow,
		NULL,
		g_hInstance,
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
	return CreateCDGWindow(&g_hForegroundWindow, &g_hForegroundWindowDC, g_foregroundWindowClassName, WS_THICKFRAME, WS_EX_NOACTIVATE);
}

bool CreateBackgroundWindow() {
	return CreateCDGWindow(&g_hBackgroundWindow, &g_hBackgroundWindowDC, g_backgroundWindowClassName, 0, WS_EX_NOACTIVATE);
}

bool CreateLogoWindow() {
	return CreateCDGWindow(&g_hLogoWindow, &g_hLogoWindowDC, g_logoWindowClassName, 0, WS_EX_APPWINDOW);
}

void UnregisterWindowClasses() {
	::UnregisterClass(g_foregroundWindowClassName, g_hInstance);
	::UnregisterClass(g_backgroundWindowClassName, g_hInstance);
	::UnregisterClass(g_logoWindowClassName, g_hInstance);
	if (g_hIcon)
		::DeleteObject(g_hIcon);
}

bool CreateWindows() {
	if (RegisterBackgroundWindowClass() &&
		RegisterForegroundWindowClass() &&
		RegisterLogoWindowClass() &&
		CreateLogoWindow() &&
		CreateBackgroundWindow() &&
		CreateForegroundWindow()) {
		::SetStretchBltMode(g_hForegroundWindowDC, COLORONCOLOR);
		::SetLayeredWindowAttributes(g_hForegroundWindow, DEFAULT_TRANSPARENT_COLORREF, 255, LWA_COLORKEY);
		::SetLayeredWindowAttributes(g_hBackgroundWindow, 0, g_nBackgroundOpacity, LWA_ALPHA);
		return true;
	}
	return false;
}

void CloseWindow(HWND hWnd, HDC hDC) {
	if (hDC)
		::ReleaseDC(hWnd, hDC);
	if (hWnd) {
		::CloseWindow(hWnd);
		::DestroyWindow(hWnd);
	}
}

void DestroyWindows() {
	CloseWindow(g_hForegroundWindow, g_hForegroundWindowDC);
	CloseWindow(g_hBackgroundWindow, g_hBackgroundWindowDC);
	CloseWindow(g_hLogoWindow, g_hLogoWindowDC);
	UnregisterWindowClasses();
}