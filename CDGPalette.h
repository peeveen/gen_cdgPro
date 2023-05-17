#pragma once
#include "stdafx.h"

// Functions relating to palette management.

/// <summary>
/// Clears the palette.
/// </summary>
void ResetPalette();

/// <summary>
/// Sets some or all of the palette to the given values.
/// </summary>
/// <param name="pRGBQuads">Source array of colours.</param>
/// <param name="nStartIndex">Target palette start index.</param>
/// <param name="nCount">Number of palette entries to set.</param>
void SetPalette(RGBQUAD* pRGBQuads, int nStartIndex, int nCount);

/// <summary>
/// The current CDG palette.
/// </summary>
extern RGBQUAD g_palette[16];

/// <summary>
/// Creates a brush of the background colour.
/// </summary>
/// <returns>Brush</returns>
extern HBRUSH CreateBackgroundBrush();