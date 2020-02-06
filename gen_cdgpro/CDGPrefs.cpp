#include "stdafx.h"
#include "CDGDefs.h"

// Preferences (TODO: INI file or UI).
int g_nBackgroundOpacity = 160;
bool g_bDrawOutline = true;
bool g_bShowBorder = false;
// We periodically ask WinAmp how many milliseconds it has played of a song. This works fine
// but as time goes on, it starts to get it wrong, falling behind by a tiny amount each time.
// To keep the display in sync, we will multiply whatever WinAmp tells us by this amount.
double g_nTimeScaler = 1.00466;
// How to determine the transparent background color?
int g_nBackgroundDetectionMode = BDM_TOPRIGHTPIXEL;
// Default background colour when there is no song playing.
int g_nDefaultBackgroundColor = 0x0055ff;
// Scale2x/4x smoothing?
int g_nSmoothingPasses = 2;
// Logo to display when there is no song playing.
const WCHAR* g_pszLogoPath = L"C:\\Users\\steven.frew\\Desktop\\smallLogo.png";