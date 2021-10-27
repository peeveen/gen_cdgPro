#pragma once
#include "stdafx.h"

// Functions relating to palette management.

void ResetPalette();
void SetPalette(RGBQUAD* pRGBQuads, int nStartIndex, int nCount);

extern RGBQUAD g_palette[16];