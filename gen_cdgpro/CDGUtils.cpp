#include "stdafx.h"

void ScaleRect(RECT* pRect, double xFactor, double yFactor) {
	pRect->left = (int)(pRect->left * xFactor);
	pRect->right = (int)(pRect->right * xFactor);
	pRect->top = (int)(pRect->top * yFactor);
	pRect->bottom = (int)(pRect->bottom * yFactor);
}

void ScaleRect(RECT* pRect, double factor) {
	ScaleRect(pRect, factor, factor);
}

void RectMessageBox(RECT* pRect, const WCHAR* pszCaption) {
	WCHAR szBuffer[255];
	wsprintf(szBuffer, L"%d,%d -> %d,%d", pRect->left, pRect->top, pRect->right, pRect->bottom);
	::MessageBox(NULL, szBuffer, pszCaption, MB_OK);
}