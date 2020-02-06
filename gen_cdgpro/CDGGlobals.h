#pragma once
#include "stdafx.h"
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;

extern HINSTANCE g_hInstance;

extern HWND g_hWinampWindow;

extern BYTE g_nLastMemoryPresetColor;

extern RGBQUAD g_logicalPalette[];
extern RGBQUAD g_effectivePalette[];

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;

extern BYTE g_nCurrentTransparentIndex;

extern bool g_bShowLogo;