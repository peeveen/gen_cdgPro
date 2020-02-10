#pragma once
#include "stdafx.h"

void ScaleRect(RECT* pRect, double factor);
void ScaleRect(RECT* pRect, double xFactor, double yFactor);
void RectMessageBox(RECT* pRect, const WCHAR* pszCaption);