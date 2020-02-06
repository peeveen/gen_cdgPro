#pragma once
#include "stdafx.h"

void Perform2xSmoothing(BYTE* pSourceBitmapBits, BYTE* pDestinationBitmapBits, int nW, int nH, int nSourceBitmapWidth);
void DrawBackground();
void DrawForeground();
void LoadLogo();
void DestroyLogo();

extern bool g_bShowLogo;

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;