#include "stdafx.h"
#include <windowsx.h>
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGMenu.h"
#include "CDGRender.h"
#include "resource.h"

// Window class names.
const WCHAR* g_foregroundWindowClassName = L"CDGProFG";
const WCHAR* g_backgroundWindowClassName = L"CDGProBG";
// Application icon.
HICON g_hIcon = NULL;
// The window (and it's DC) containing the foreground.
HDC g_hForegroundWindowDC = NULL;
HWND g_hForegroundWindow = NULL;
// The window (and it's DC) containing the (optionally transparent) background.
HDC g_hBackgroundWindowDC = NULL;
HWND g_hBackgroundWindow = NULL;
// Window size to restore to from fullscreen. If not currently in fullscreen mode, this is zero size.
RECT g_lastSize = { 0,0,0,0 };

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
	switch (uMsg)
	{
	case WM_NCHITTEST: {
		LRESULT hit = DefWindowProc(hwnd, uMsg, wParam, lParam);
		if (hit == HTCLIENT)
			hit = HTCAPTION;
		return hit;
	}
	case WM_KEYDOWN:
		if (wParam == VK_ESCAPE)
			SetFullScreen(false);
		break;
	case WM_WINDOWPOSCHANGED:
	case WM_SHOWWINDOW:
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
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
		break;
	}
	case WM_SIZE: {
		int w = (int)(short)LOWORD(lParam);
		int h = (int)(short)HIWORD(lParam);
		::SetWindowPos(g_hBackgroundWindow, hwnd, 0, 0, w, h, SWP_NOMOVE | SWP_NOACTIVATE);
		break;
	}
	case WM_CLOSE:
		return 1;
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

bool CreateCDGWindow(HWND* phWnd, HDC* phDC, const WCHAR* pszClassName, DWORD additionalStyles = 0, DWORD additionalExStyles = 0) {
	DWORD styles = WS_VISIBLE | additionalStyles;
	*phWnd = CreateWindowEx(
		WS_EX_LAYERED | additionalExStyles,
		pszClassName,
		pszClassName,
		styles,
		50, 50,
		CDG_WIDTH, CDG_HEIGHT,
		g_hWinampWindow,
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
	return CreateCDGWindow(&g_hForegroundWindow, &g_hForegroundWindowDC, g_foregroundWindowClassName, WS_THICKFRAME, WS_EX_APPWINDOW);
}

bool CreateBackgroundWindow() {
	return CreateCDGWindow(&g_hBackgroundWindow, &g_hBackgroundWindowDC, g_backgroundWindowClassName, 0, WS_EX_NOACTIVATE);
}

void UnregisterWindowClasses() {
	::UnregisterClass(g_foregroundWindowClassName, g_hInstance);
	::UnregisterClass(g_backgroundWindowClassName, g_hInstance);
	if (g_hIcon)
		::DeleteObject(g_hIcon);
}

bool CreateWindows() {
	if (RegisterBackgroundWindowClass() &&
		RegisterForegroundWindowClass() &&
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
	UnregisterWindowClasses();
}