#pragma once
#include "stdafx.h"

void DrawBackground();
void DrawForeground(RECT* pRedrawRect);
void PaintForeground(RECT *pInvalidRect);
void RenderForegroundBackBuffer(RECT* pInvalidCDGRect);
void PaintForegroundBackBuffer();

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;

extern HANDLE g_hPaintMutex;