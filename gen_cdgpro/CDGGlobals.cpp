#include "stdafx.h"
#include "CDGDefs.h"
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;

// The instance handle of this DLL.
HINSTANCE g_hInstance = NULL;

// Handle to the Winamp window.
HWND g_hWinampWindow = NULL;

// We keep track of the last "reset" color. If we receive a MemoryPreset command for this color
// again before anything else has been drawn, we can ignore it.
BYTE g_nLastMemoryPresetColor = -1;

// The palettes ... logical is the one defined by the CDG data, effective is the one we're using
// that has been pochled to ensure there are no duplicate colours.
RGBQUAD g_logicalPalette[16];
RGBQUAD g_effectivePalette[16];

// Canvas pixel offsets for scrolling
int g_nCanvasXOffset = 0;
int g_nCanvasYOffset = 0;

// We keep track of what color is the current transparent color, so that we know whether it's
// worthwhile calling various GDI functions to change it.
BYTE g_nCurrentTransparentIndex = 0;
