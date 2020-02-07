#pragma once
#include "stdafx.h"

void DrawBackground();
void DrawForeground(RECT *pInvalidWindowRect);
void LoadLogo();
void DestroyLogo();
void RefreshScreen();

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;
extern RECT g_redrawRect;