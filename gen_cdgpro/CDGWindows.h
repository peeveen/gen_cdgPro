#pragma once
#include "stdafx.h"

bool CreateWindows();
void DestroyWindows();

extern HDC g_hForegroundWindowDC;
extern HWND g_hForegroundWindow;

extern HDC g_hBackgroundWindowDC;
extern HWND g_hBackgroundWindow;