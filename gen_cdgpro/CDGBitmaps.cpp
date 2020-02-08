#include "stdafx.h"
#include "CDGWindows.h"
#include "CDGPrefs.h"
#include <objidl.h>
#include <stdlib.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;

// Brush filled with the transparency color.
HBRUSH g_hTransparentBrush;

// The DC and bitmap containing the CDG graphics.
HDC g_hScaledForegroundDCs[SUPPORTED_SCALING_LEVELS];
HBITMAP g_hScaledForegroundBitmaps[SUPPORTED_SCALING_LEVELS];
BYTE* g_pScaledForegroundBitmapBits[SUPPORTED_SCALING_LEVELS];

// The DC containing the mask for the CDG graphics.
HDC g_hMaskDC = NULL;
HBITMAP g_hMaskBitmap = NULL;

// The DC containing the border mask for the CDG graphics.
HDC g_hBorderMaskDC = NULL;
HBITMAP g_hBorderMaskBitmap = NULL;
BYTE* g_pBorderMaskBitmapBits = NULL;

// The DC containing the masked CDG graphics.
HDC g_hMaskedForegroundDC = NULL;
HBITMAP g_hMaskedForegroundBitmap = NULL;

// The DC and bitmap containing the background (usually 1 pixel that we stretch out).
HDC g_hBackgroundDC = NULL;
HBITMAP g_hBackgroundBitmap = NULL;
unsigned int* g_pBackgroundBitmapBits = NULL;

// The scroll buffer DC and bitmap.
HDC g_hScrollBufferDC = NULL;
HBITMAP g_hScrollBufferBitmap = NULL;
BYTE* g_pScrollBufferBitmapBits = NULL;

// The DC and bitmap for the logo
HDC g_hLogoDC = NULL;
HBITMAP g_hLogoBitmap = NULL;

// The DC for the screen.
HDC g_hScreenDC = NULL;

// Logo image
Image* g_pLogoImage = NULL;
SIZE g_logoSize;

void DestroyLogo() {
	if (g_pLogoImage)
		delete g_pLogoImage;
}

bool CreateTransparentBrush() {
	g_hTransparentBrush = ::CreateSolidBrush(DEFAULT_TRANSPARENT_COLORREF);
	return !!g_hTransparentBrush;
}

bool CreateBitmapSurface(HDC* phDC, HBITMAP* phBitmap, LPVOID* ppBitmapBits, int nWidth, int nHeight, int nBitCount) {
	BITMAPINFO bitmapInfo;
	bitmapInfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
	bitmapInfo.bmiHeader.biWidth = nWidth;
	bitmapInfo.bmiHeader.biHeight = nHeight;
	bitmapInfo.bmiHeader.biPlanes = 1;
	bitmapInfo.bmiHeader.biBitCount = nBitCount;
	bitmapInfo.bmiHeader.biCompression = BI_RGB;
	bitmapInfo.bmiHeader.biSizeImage = 0;
	bitmapInfo.bmiHeader.biXPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biYPelsPerMeter = 0;
	bitmapInfo.bmiHeader.biClrUsed = 0;
	bitmapInfo.bmiHeader.biClrImportant = 0;

	*phDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (*phDC) {
		*phBitmap = ::CreateDIBSection(*phDC, &bitmapInfo, 0, ppBitmapBits, NULL, 0);
		if (*phBitmap) {
			::SelectObject(*phDC, *phBitmap);
			return true;
		}
	}
	return false;
}

bool CreateForegroundDCs() {
	int nScaling = 1;
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f) {
		if (!CreateBitmapSurface(&(g_hScaledForegroundDCs[f]), &(g_hScaledForegroundBitmaps[f]), (LPVOID*)(&(g_pScaledForegroundBitmapBits[f])), CDG_BITMAP_WIDTH * nScaling, -CDG_BITMAP_HEIGHT * nScaling, 4))
			return false;
		nScaling *= 2;
	}
	return true;
}

bool CreateScrollBufferDC() {
	return CreateBitmapSurface(&g_hScrollBufferDC, &g_hScrollBufferBitmap, (LPVOID*)&g_pScrollBufferBitmapBits, CDG_BITMAP_WIDTH, -CDG_BITMAP_HEIGHT, 4);
}

bool CreateMaskedForegroundDC() {
	g_hMaskedForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskedForegroundDC) {
		g_hMaskedForegroundBitmap = ::CreateCompatibleBitmap(g_hForegroundWindowDC, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT);
		if (g_hMaskedForegroundBitmap) {
			::SelectObject(g_hMaskedForegroundDC, g_hTransparentBrush);
			::SelectObject(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
			return true;
		}
	}
	return false;
}

bool CreateMaskDC() {
	g_hMaskDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
	if (g_hMaskDC) {
		g_hMaskBitmap = ::CreateBitmap(CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, 1, 1, NULL);
		if (g_hMaskBitmap) {
			::SelectObject(g_hMaskDC, g_hMaskBitmap);
			return true;
		}
	}
	return false;
}

bool CreateBorderMaskDC() {
	bool result = CreateBitmapSurface(&g_hBorderMaskDC, &g_hBorderMaskBitmap, (LPVOID*)&g_pBorderMaskBitmapBits, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, 1);
	RGBQUAD monoPalette[] = {
		{255,255,255,0},
		{0,0,0,0}
	};
	return result && ::SetDIBColorTable(g_hBorderMaskDC, 0, 2, monoPalette);
}

bool CreateBackgroundDC() {
	return CreateBitmapSurface(&g_hBackgroundDC, &g_hBackgroundBitmap, (LPVOID*)&g_pBackgroundBitmapBits, 1, 1, 32);
}

void ClearForegroundBuffer() {
	RECT r = { 0,0,CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT };
	::FillRect(g_hMaskedForegroundDC, &r, g_hTransparentBrush);
}

SIZE LoadLogo() {
	g_pLogoImage = new Image(g_pszLogoPath);
	if (g_pLogoImage->GetLastStatus() == Ok) {
		return g_logoSize = { (LONG)g_pLogoImage->GetWidth(),(LONG)g_pLogoImage->GetHeight() };
	}
	delete g_pLogoImage;
	g_pLogoImage = NULL;
	SIZE noSize = { 0,0 };
	return noSize;
}

bool CreateLogoDC() {
	SIZE logoSize = LoadLogo();
	if (logoSize.cx == 0)
		return true;
	bool result = false;
	g_hLogoDC = ::CreateCompatibleDC(g_hScreenDC);
	if (g_hLogoDC) {
		g_hLogoBitmap = ::CreateCompatibleBitmap(g_hScreenDC, logoSize.cx, logoSize.cy);
		if (g_hLogoBitmap) {
			::SelectObject(g_hLogoDC, g_hLogoBitmap);
			RECT r;
			::GetClientRect(g_hLogoWindow, &r);
			int windowWidth = r.right - r.left;
			int windowHeight = r.bottom - r.top;
			Graphics g(g_hLogoDC);
			g.DrawImage(g_pLogoImage, 0, 0, g_logoSize.cx, g_logoSize.cy);
			result = true;
		}
	}
	return result;
}

bool CreateBitmaps() {
	g_hScreenDC = ::GetDC(NULL);
	return CreateTransparentBrush() &&
		CreateBackgroundDC() &&
		CreateForegroundDCs() &&
		CreateMaskDC() &&
		CreateBorderMaskDC() &&
		CreateScrollBufferDC() &&
		CreateLogoDC() &&
		CreateMaskedForegroundDC();
}

void DeleteDCAndBitmap(HDC hDC, HBITMAP hBitmap) {
	if (hDC)
		::DeleteDC(hDC);
	if (hBitmap)
		::DeleteObject(hBitmap);
}

void DestroyBitmaps() {
	DeleteDCAndBitmap(g_hMaskDC, g_hMaskBitmap);
	DeleteDCAndBitmap(g_hBorderMaskDC, g_hBorderMaskBitmap);
	DeleteDCAndBitmap(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
	DeleteDCAndBitmap(g_hScrollBufferDC, g_hScrollBufferBitmap);
	DeleteDCAndBitmap(g_hScrollBufferDC, g_hScrollBufferBitmap);
	DeleteDCAndBitmap(g_hLogoDC, g_hLogoBitmap);
	DeleteDCAndBitmap(g_hBackgroundDC, g_hBackgroundBitmap);
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f)
		DeleteDCAndBitmap(g_hScaledForegroundDCs[f], g_hScaledForegroundBitmaps[f]);
	DestroyLogo();
	if (g_hTransparentBrush)
		::DeleteObject(g_hTransparentBrush);
	::ReleaseDC(NULL,g_hScreenDC);
}