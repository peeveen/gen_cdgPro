#pragma once
#include "stdafx.h"

// Valid values for Background Detection Mode pref
#define BDM_PALETTEINDEXZERO 0
#define BDM_TOPLEFTPIXEL 1
#define BDM_TOPRIGHTPIXEL 2
#define BDM_BOTTOMLEFTPIXEL 3
#define BDM_BOTTOMRIGHTPIXEL 4

void ReadPrefs();

extern int g_nBackgroundOpacity;
extern bool g_bDrawOutline;
extern double g_nTimeScaler;
extern int g_nBackgroundDetectionMode;
extern int g_nDefaultBackgroundColor;
extern int g_nSmoothingPasses;
extern int g_nMargin;
extern bool g_bDoubleBuffered;
extern WCHAR g_szLogoPath[];