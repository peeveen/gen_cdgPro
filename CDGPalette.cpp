#include "stdafx.h"
#include "CDGPrefs.h"
#include "CDGBitmaps.h"
#include "CDGBackgroundFunctions.h"

// The palettes ... the first is the one defined by the CDG data.
// However, because our transparency processing code requires
// all palette entries to be unique, we "pochle" the logical
// palette to create the one that will actually be used.
RGBQUAD g_cdgPalette[16];
RGBQUAD g_palette[16];

void BuildEffectivePalette() {
	// First, copy the original palette to the unique palette.
	memcpy(g_palette, g_cdgPalette, sizeof(RGBQUAD) * 16);
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
		BYTE red = g_palette[f].rgbRed;
		BYTE green = g_palette[f].rgbGreen;
		BYTE blue = g_palette[f].rgbBlue;
		for (int g = f + 1; g < 16; ++g) {
			BYTE testRed = g_palette[g].rgbRed;
			BYTE testGreen = g_palette[g].rgbGreen;
			BYTE testBlue = g_palette[g].rgbBlue;
			if ((testRed == red) && (testGreen == green) && (testBlue == blue)) {
				testRed += ((BYTE)(testRed + uniqueifier) < testRed ? -uniqueifier : uniqueifier);
				testGreen += ((BYTE)(testGreen + uniqueifier) < testGreen ? -uniqueifier : uniqueifier);
				testBlue += ((BYTE)(testBlue + uniqueifier) < testBlue ? -uniqueifier : uniqueifier);
				g_palette[g] = { testBlue,testGreen,testRed,0 };
				++uniqueifier;
			}
		}
	}
}

void SetPalette(RGBQUAD* pRGBQuads, int nStartIndex,int nCount) {
	memcpy(g_cdgPalette + nStartIndex, pRGBQuads, sizeof(RGBQUAD) * nCount);
	BuildEffectivePalette();
	// Set the palette in each of the window device contexts.
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f)
		::SetDIBColorTable(g_hScaledForegroundDCs[f], 0, 16, g_palette);
	::SetDIBColorTable(g_hScrollBufferDC, 0, 16, g_palette);
	SetBackgroundColorIndex(g_nCurrentTransparentIndex);
}

/// <summary>
/// Reset the entire palette, defaulting all colours to the default background colour
/// as specified in the prefs file.
/// </summary>
void ResetPalette() {
	RGBQUAD emptyPalette[16];
	::ZeroMemory(emptyPalette, sizeof(RGBQUAD) * 16);
	emptyPalette[0].rgbBlue = g_nDefaultBackgroundColor & 0x00ff;
	emptyPalette[0].rgbGreen = (g_nDefaultBackgroundColor >> 8) & 0x00ff;
	emptyPalette[0].rgbRed = (g_nDefaultBackgroundColor >> 16) & 0x00ff;
	SetPalette(emptyPalette, 0, 16);
}