#include "stdafx.h"
#include "CDGDefs.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGBitmaps.h"
#include "CDGInstructionHandlers.h"

// We keep track of what color is the current transparent color, so that we know whether it's
// worthwhile calling various GDI functions to change it.
BYTE g_nCurrentTransparentIndex = 0;

void SetBackgroundColorIndex(BYTE index) {
	g_nCurrentTransparentIndex = index;
	// RGB macro, for some reason, encodes as BGR. Not so handy for direct 32-bit bitmap writing.
	COLORREF backgroundColorReversed = RGB(g_effectivePalette[index].rgbBlue, g_effectivePalette[index].rgbGreen, g_effectivePalette[index].rgbRed);
	*g_pBackgroundBitmapBits = backgroundColorReversed;
	COLORREF backgroundColor = RGB(g_effectivePalette[index].rgbRed, g_effectivePalette[index].rgbGreen, g_effectivePalette[index].rgbBlue);
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f)
		::SetBkColor(g_hScaledForegroundDCs[f], backgroundColor);
}

void SetBackgroundColorFromPixel(int offset, bool highNibble) {
	BYTE color = (g_pScaledForegroundBitmapBits[0][offset] >> (highNibble ? 4 : 0)) & 0x0F;
	SetBackgroundColorIndex(color);
}

bool CheckPixelColorBackgroundChange(bool topLeftPixelSet, bool topRightPixelSet, bool bottomLeftPixelSet, bool bottomRightPixelSet) {
	if (g_nBackgroundDetectionMode == BDM_TOPLEFTPIXEL && topLeftPixelSet)
		SetBackgroundColorFromPixel(TOP_LEFT_PIXEL_OFFSET, true);
	else if (g_nBackgroundDetectionMode == BDM_TOPRIGHTPIXEL && topRightPixelSet)
		SetBackgroundColorFromPixel(TOP_RIGHT_PIXEL_OFFSET, false);
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMLEFTPIXEL && bottomLeftPixelSet)
		SetBackgroundColorFromPixel(BOTTOM_LEFT_PIXEL_OFFSET, true);
	else if (g_nBackgroundDetectionMode == BDM_BOTTOMRIGHTPIXEL && bottomRightPixelSet)
		SetBackgroundColorFromPixel(BOTTOM_RIGHT_PIXEL_OFFSET, false);
	else
		return false;
	return true;
}