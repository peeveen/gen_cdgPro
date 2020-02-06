#include "stdafx.h"
#include "CDGDefs.h"
#include "CDGPrefs.h"
#include "CDGBitmaps.h"

// The palettes ... logical is the one defined by the CDG data, effective is the one we're using
// that has been pochled to ensure there are no duplicate colours.
RGBQUAD g_logicalPalette[16];
RGBQUAD g_effectivePalette[16];

void ResetPalette() {
	::ZeroMemory(g_logicalPalette, sizeof(RGBQUAD) * 16);
	::ZeroMemory(g_effectivePalette, sizeof(RGBQUAD) * 16);
	g_logicalPalette[0].rgbBlue = g_effectivePalette[0].rgbBlue = g_nDefaultBackgroundColor & 0x00ff;
	g_logicalPalette[0].rgbGreen = g_effectivePalette[0].rgbGreen = (g_nDefaultBackgroundColor >> 8) & 0x00ff;
	g_logicalPalette[0].rgbRed = g_effectivePalette[0].rgbRed = (g_nDefaultBackgroundColor >> 16) & 0x00ff;
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f)
		::SetDIBColorTable(g_hScaledForegroundDCs[f], 0, 16, g_logicalPalette);
}

void BuildEffectivePalette() {
	// First, copy the original palette to the unique palette.
	for (int f = 0; f < 16; ++f)
		g_effectivePalette[f] = g_logicalPalette[f];
	// Now check each entry and unique-ify it if necessary.
	// We will increase/decrease the matching RGB values by this amount.
	// Each time we find a match, we will increment this value.
	BYTE uniqueifier = 1;
	// Remember that each colour will originally have been a 12-bit
	// colour, which we have multiplied by 17 to become 24-bit, so at the
	// start of this operation, there should not be any two colours that
	// are within 16 of each other. It should also be impossible for the
	// uniqueifier to exceed 16, so we should never create a clash.
	for (int f = 0; f < 16; ++f) {
		BYTE red = g_effectivePalette[f].rgbRed;
		BYTE green = g_effectivePalette[f].rgbGreen;
		BYTE blue = g_effectivePalette[f].rgbBlue;
		for (int g = f + 1; g < 16; ++g) {
			BYTE testRed = g_effectivePalette[g].rgbRed;
			BYTE testGreen = g_effectivePalette[g].rgbGreen;
			BYTE testBlue = g_effectivePalette[g].rgbBlue;
			if ((testRed == red) && (testGreen == green) && (testBlue == blue)) {
				testRed += ((BYTE)(testRed + uniqueifier) < testRed ? -uniqueifier : uniqueifier);
				testGreen += ((BYTE)(testGreen + uniqueifier) < testGreen ? -uniqueifier : uniqueifier);
				testBlue += ((BYTE)(testBlue + uniqueifier) < testBlue ? -uniqueifier : uniqueifier);
				g_effectivePalette[g] = { testBlue,testGreen,testRed,0 };
				++uniqueifier;
			}
		}
	}
}