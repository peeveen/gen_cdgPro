#pragma once
#include "stdafx.h"

// Valid values for Background Detection Mode pref
#define BDM_PALETTEINDEXZERO 0
#define BDM_TOPLEFTPIXEL 1
#define BDM_TOPRIGHTPIXEL 2
#define BDM_BOTTOMLEFTPIXEL 3
#define BDM_BOTTOMRIGHTPIXEL 4

// Functions relating to the reading/parsing of the basic prefs INI file.

bool ReadPrefs();

/// <summary>
/// Path to the INI file.
/// </summary>
extern WCHAR g_szINIPath[];

/// <summary>
/// How opaque should the background be (0-255)?
/// </summary>
extern int g_nBackgroundOpacity;

/// <summary>
/// Should we draw an outline around the foreground graphics?
/// </summary>
extern bool g_bDrawOutline;

/// <summary>
/// Amount to scale time by, to cope with Winamp's sloppy timekeeping.
/// </summary>
extern double g_nTimeScaler;

/// <summary>
/// How to calculate the background/transparent colour. Should be one of the
/// BDM_* values.
/// </summary>
extern int g_nBackgroundDetectionMode;

/// <summary>
/// Default background colour for when no CDG if playing (only used if
/// showing a logo).
/// </summary>
extern int g_nDefaultBackgroundColor;

/// <summary>
/// Number of smoothing passes to perform.
/// </summary>
extern int g_nSmoothingPasses;

/// <summary>
/// Amount of space to leave around the CDG canvas.
/// </summary>
extern int g_nMargin;

/// <summary>
/// True to use double buffered device context.
/// </summary>
extern bool g_bDoubleBuffered;

/// <summary>
/// Path to logo file (if specified).
/// </summary>
extern WCHAR g_szLogoPath[];

/// <summary>
/// If true, we will use layered windows and alpha blending, etc.
/// </summary>
extern bool g_bUseLayeredWindows;