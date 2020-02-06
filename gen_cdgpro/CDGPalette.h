#pragma once
#include "stdafx.h"

void ResetPalette();
void BuildEffectivePalette();

extern RGBQUAD g_logicalPalette[16];
extern RGBQUAD g_effectivePalette[16];