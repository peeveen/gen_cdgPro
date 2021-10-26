#pragma once
#include "stdafx.h"

// Functions relating to the rendering of the CDG buffer onto the screen.

void DrawBackground();
void DrawForeground(RECT* pRedrawRect);
void PaintForeground(RECT *pInvalidRect);
void RenderForegroundBackBuffer(RECT* pInvalidCDGRect);
void PaintForegroundBackBuffer();

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;