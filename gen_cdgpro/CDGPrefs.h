#pragma once
#include "stdafx.h"

// Valid values for Background Detection Mode pref
#define BDM_PALETTEINDEXZERO 0
#define BDM_TOPLEFTPIXEL 1
#define BDM_TOPRIGHTPIXEL 2
#define BDM_BOTTOMLEFTPIXEL 3
#define BDM_BOTTOMRIGHTPIXEL 4

// Preferences (TODO: INI file or UI).
extern int g_nBackgroundOpacity;
extern bool g_bDrawOutline;
extern bool g_bShowBorder;
// We periodically ask WinAmp how many milliseconds it has played of a song. This works fine
// but as time goes on, it starts to get it wrong, falling behind by a tiny amount each time.
// To keep the display in sync, we will multiply whatever WinAmp tells us by this amount.
extern double g_nTimeScaler;
// How to determine the transparent background color?
extern int g_nBackgroundDetectionMode;
// Default background colour when there is no song playing.
extern int g_nDefaultBackgroundColor;
// Scale2x/4x smoothing?
extern int g_nSmoothingPasses;
// Logo to display when there is no song playing.
extern const WCHAR* g_pszLogoPath;