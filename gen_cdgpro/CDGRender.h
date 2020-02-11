#pragma once
#include "stdafx.h"

void DrawBackground();
void DrawForeground();
void RefreshScreen(RECT* pInvalidCDGRect);
void RedrawForeground(RECT* pRedrawRect);

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;