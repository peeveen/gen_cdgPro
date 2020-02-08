#pragma once
#include "stdafx.h"

void SetFullScreen(bool fullScreen);
bool CreateWindows();
void DestroyWindows();
bool IsFullScreen();
void ShowLogo(bool bShow);

extern HDC g_hForegroundWindowDC;
extern HWND g_hForegroundWindow;

extern HDC g_hBackgroundWindowDC;
extern HWND g_hBackgroundWindow;

extern HDC g_hLogoWindowDC;
extern HWND g_hLogoWindow;
