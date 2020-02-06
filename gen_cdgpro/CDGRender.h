#pragma once
#include "stdafx.h"

void DrawBackground();
void DrawForeground();
void LoadLogo();
void DestroyLogo();

extern int g_nCanvasXOffset;
extern int g_nCanvasYOffset;
extern RECT g_redrawRect;