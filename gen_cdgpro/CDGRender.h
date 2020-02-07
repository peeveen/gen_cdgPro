#pragma once
#include "stdafx.h"

void DrawBackground();
void DrawForeground(RECT *pInvalidWindowRect);
void LoadLogo();
void DestroyLogo();
void RefreshScreen(RECT* pInvalidCDGRect);
void SetRedrawRect(RECT* pRedrawRect);

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;
