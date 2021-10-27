#pragma once
#include "stdafx.h"

// Functions related to maintaining state of the background/transparent value.
// While CDG *does* have the concept of a background colour, some CDG files will, for
// some reason, insist upon using a non-background colour to fill the background with.
// This screws with our attempt to use the background colour as the transparent colour.
// To get around this, we have a fudge of using the colour from the pixel at any given
// corner of the buffer as the background/transparent colour.

/// <summary>
/// Sets the current background (transparent) palette index.
/// </summary>
/// <param name="index">New value.</param>
void SetBackgroundColorIndex(BYTE index);

/// <summary>
/// Sets the current background (transparent) palette index from the value of the pixel at the given offset.
/// </summary>
/// <param name="offset">Offset into the CDG buffer that the pixel value should be read from.</param>
/// <param name="highNibble">True to use the high nibble of that byte, false for low.</param>
void SetBackgroundColorFromPixel(int offset, bool highNibble);

/// <summary>
/// Checks whether the most recent buffer write might cause a change of the background/transparent value.
/// </summary>
/// <param name="topLeftPixelSet">True if the top left pixel was set during the last set processed of CDG instructions.</param>
/// <param name="topRightPixelSet">True if the top right pixel was set during the last set processed of CDG instructions.</param>
/// <param name="bottomLeftPixelSet">True if the bottom left pixel was set during the last set processed of CDG instructions.</param>
/// <param name="bottomRightPixelSet">True if the bottom right pixel was set during the last set processed of CDG instructions.</param>
/// <returns>True if the background/transparent value should change.</returns>
bool CheckPixelColorBackgroundChange(bool topLeftPixelSet, bool topRightPixelSet, bool bottomLeftPixelSet, bool bottomRightPixelSet);

extern BYTE g_nCurrentTransparentIndex;