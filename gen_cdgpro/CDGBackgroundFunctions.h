#pragma once
#include "stdafx.h"

void SetBackgroundColorIndex(BYTE index);
void SetBackgroundColorFromPixel(int offset, bool highNibble);
bool CheckPixelColorBackgroundChange(bool topLeftPixelSet, bool topRightPixelSet, bool bottomLeftPixelSet, bool bottomRightPixelSet);

extern BYTE g_nCurrentTransparentIndex;