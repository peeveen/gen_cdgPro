#include "stdafx.h"
#include "CDGWindows.h"
#include "CDGPrefs.h"
#include "CDGPalette.h"
#include <objidl.h>
#include <stdlib.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
using namespace Gdiplus;

// Brush filled with the transparency color.
HBRUSH g_hTransparentBrush;

// The DC and bitmap containing the CDG graphics.
HDC g_hScaledForegroundDCs[SUPPORTED_SCALING_LEVELS];
HBITMAP g_hScaledForegroundBitmaps[SUPPORTED_SCALING_LEVELS];
BYTE* g_pScaledForegroundBitmapBits[SUPPORTED_SCALING_LEVELS];

// The DC containing the monochrome mask for the CDG graphics.
HDC g_hMaskDC = NULL;
HBITMAP g_hMaskBitmap = NULL;

// The DC containing the outline/border mask for the CDG graphics.
HDC g_hBorderMaskDC = NULL;
HBITMAP g_hBorderMaskBitmap = NULL;
BYTE* g_pBorderMaskBitmapBits = NULL;

// The DC containing the masked CDG graphics.
HDC g_hMaskedForegroundDC = NULL;
HBITMAP g_hMaskedForegroundBitmap = NULL;
HANDLE g_hMaskedForegroundDCAccessMutex = NULL;

// This is the "final" display that we copy to the window DC.
// If we attempt to write to this DC *AND* read from it simultaneously,
// we get failures, and missing bits of screen. Therefore, the reading
// and writing have to happen at different times, so we need a mutex to
// control that.
HDC g_hForegroundBackBufferDC = NULL;
HBITMAP g_hForegroundBackBufferBitmap = NULL;
HANDLE g_hForegroundBackBufferDCAccessMutex = NULL;
int g_nForegroundBackBufferWidth = 0;
int g_nForegroundBackBufferHeight = 0;

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

/// <summary>
/// Cleanup the logo data.
/// </summary>
void DestroyLogo() {
	if (g_pLogoImage)
		delete g_pLogoImage;
}

/// <summary>
/// Create our unique transparent colour brush.
/// </summary>
/// <returns>True if created okay.</returns>
bool CreateTransparentBrush() {
	g_hTransparentBrush = ::CreateSolidBrush(DEFAULT_TRANSPARENT_COLORREF);
	return !!g_hTransparentBrush;
}

/// <summary>
/// Creates a bitmap and device context containing that bitmap.
/// </summary>
/// <param name="phDC">Pointer to handle that will receive the DC.</param>
/// <param name="phBitmap">Pointer to handle that will receive the bitmap.</param>
/// <param name="ppBitmapBits">Pointer to pointer that will receive the address of the bitmap bits.</param>
/// <param name="nWidth">Width of bitmap to create.</param>
/// <param name="nHeight">Height of bitmap to create. Can be negative for inverse vertical coordinates.</param>
/// <param name="nBitCount">Bit depth of bitmap.</param>
/// <returns>True if successful.</returns>
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

/// <summary>
/// Creates the various necessary foreground device contexts (one per scaling level).
/// </summary>
/// <returns>True if successful.</returns>
bool CreateForegroundDCs() {
	int nScaling = 1;
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f) {
		if (!CreateBitmapSurface(&(g_hScaledForegroundDCs[f]), &(g_hScaledForegroundBitmaps[f]), (LPVOID*)(&(g_pScaledForegroundBitmapBits[f])), CDG_BITMAP_WIDTH * nScaling, -CDG_BITMAP_HEIGHT * nScaling, 4))
			return false;
		nScaling *= 2;
	}
	return true;
}

/// <summary>
/// Creates the temporary scroll device context and bitmap.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateScrollBufferDC() {
	return CreateBitmapSurface(&g_hScrollBufferDC, &g_hScrollBufferBitmap, (LPVOID*)&g_pScrollBufferBitmapBits, CDG_BITMAP_WIDTH, -CDG_BITMAP_HEIGHT, 4);
}

/// <summary>
/// Creates the masked foreground DC.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateMaskedForegroundDC() {
	g_hMaskedForegroundDCAccessMutex = ::CreateMutex(NULL, FALSE, NULL);
	if (g_hMaskedForegroundDCAccessMutex) {
		g_hMaskedForegroundDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
		if (g_hMaskedForegroundDC) {
			g_hMaskedForegroundBitmap = ::CreateCompatibleBitmap(g_hForegroundWindowDC, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT);
			if (g_hMaskedForegroundBitmap) {
				::SelectObject(g_hMaskedForegroundDC, g_hTransparentBrush);
				::SelectObject(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
				return true;
			}
		}
	}
	return false;
}

/// <summary>
/// Creates the foreground back buffer device context.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateForegroundBackBufferDC() {
	g_hForegroundBackBufferDCAccessMutex = ::CreateMutex(NULL, FALSE, NULL);
	if (g_hForegroundBackBufferDCAccessMutex) {
		g_hForegroundBackBufferDC = ::CreateCompatibleDC(g_hForegroundWindowDC);
		if (g_hForegroundBackBufferDC) {
			// No need to create a bitmap here, it will get created every time the window is sized.
			::SetStretchBltMode(g_hForegroundBackBufferDC, COLORONCOLOR);
			return true;
		}
	}
	return false;
}

/// <summary>
/// Resizes the foreground back buffer bitmap when the window is resized.
/// </summary>
void ResizeForegroundBackBufferBitmap() {
	if (!g_hForegroundBackBufferDC)
		return;
	static RECT clientRect;
	::GetClientRect(g_hForegroundWindow, &clientRect);
	int width = clientRect.right - clientRect.left;
	int height = clientRect.bottom - clientRect.top;
	HBITMAP hForegroundBackBufferBitmap = ::CreateCompatibleBitmap(g_hForegroundWindowDC, width, height);
	if (hForegroundBackBufferBitmap) {
		::WaitForSingleObject(g_hForegroundBackBufferDCAccessMutex, INFINITE);
		HGDIOBJ oldBitmap = ::SelectObject(g_hForegroundBackBufferDC, hForegroundBackBufferBitmap);
		g_nForegroundBackBufferWidth = width;
		g_nForegroundBackBufferHeight = height;
		::FillRect(g_hForegroundBackBufferDC, &clientRect, g_hTransparentBrush);
		::ReleaseMutex(g_hForegroundBackBufferDCAccessMutex);
		g_hForegroundBackBufferBitmap = hForegroundBackBufferBitmap;
		if (oldBitmap)
			::DeleteObject(oldBitmap);
	}
}

/// <summary>
/// Creates the monochrome mask device context and bitmap.
/// </summary>
/// <returns>True if successful.</returns>
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

/// <summary>
/// Create our border mask device context and bitmap.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateBorderMaskDC() {
	bool result = CreateBitmapSurface(&g_hBorderMaskDC, &g_hBorderMaskBitmap, (LPVOID*)&g_pBorderMaskBitmapBits, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, 1);
	// It's not a black and white palette, it's white and black!
	RGBQUAD monoPalette[] = {
		{255,255,255,0},
		{0,0,0,0}
	};
	return result && ::SetDIBColorTable(g_hBorderMaskDC, 0, 2, monoPalette);
}

/// <summary>
/// Create the one-pixel background DC and bitmap.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateBackgroundDC() {
	return CreateBitmapSurface(&g_hBackgroundDC, &g_hBackgroundBitmap, (LPVOID*)&g_pBackgroundBitmapBits, 1, 1, 32);
}

/// <summary>
/// Clear the background buffer, filling it with transparency.
/// </summary>
void ClearForegroundBuffer() {
	RECT r = { 0,0,CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT };
	HBRUSH hBrush = g_bUseLayeredWindows ? g_hTransparentBrush : CreateBackgroundBrush();
	::FillRect(g_hMaskedForegroundDC, &r, hBrush);
	::GetClientRect(g_hForegroundWindow, &r);
	::WaitForSingleObject(g_hForegroundBackBufferDCAccessMutex, INFINITE);
	::FillRect(g_hForegroundBackBufferDC, &r, hBrush);
	if (!g_bUseLayeredWindows)
		::DeleteObject(hBrush);
	::ReleaseMutex(g_hForegroundBackBufferDCAccessMutex);
}

/// <summary>
/// Loads the logo file into memory.
/// </summary>
/// <returns>True if successful.</returns>
bool LoadLogo() {
	g_pLogoImage = new Image(g_szLogoPath);
	if (g_pLogoImage->GetLastStatus() == Ok) {
		g_logoSize = { (LONG)g_pLogoImage->GetWidth(),(LONG)g_pLogoImage->GetHeight() };
		return true;
	}
	delete g_pLogoImage;
	g_pLogoImage = NULL;
	return false;
}

/// <summary>
/// Creates the logo device context and bitmap.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateLogoDC() {
	// If there is no logo, we will use a 1x1 transparent pixel instead.
	bool logoFound = LoadLogo();
	if (!logoFound)
		g_logoSize = { 1,1 };
	g_hLogoDC = ::CreateCompatibleDC(g_hScreenDC);
	if (g_hLogoDC) {
		g_hLogoBitmap = ::CreateCompatibleBitmap(g_hScreenDC, g_logoSize.cx, g_logoSize.cy );
		if (g_hLogoBitmap) {
			::SelectObject(g_hLogoDC, g_hLogoBitmap);
			Graphics g(g_hLogoDC);
			if (logoFound)
				g.DrawImage(g_pLogoImage, 0, 0, g_logoSize.cx, g_logoSize.cy);
			else {
				SolidBrush sb(Color::MakeARGB(0, 0, 0, 0));
				g.FillRectangle(&sb, 0, 0, g_logoSize.cx, g_logoSize.cy);
			}
			return true;
		}
	}
	return false;
}

/// <summary>
/// Creates all necessary device contexts and bitmaps.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateBitmaps() {
	g_hScreenDC = ::GetDC(NULL);
	return CreateTransparentBrush() &&
		CreateBackgroundDC() &&
		CreateForegroundDCs() &&
		CreateMaskDC() &&
		CreateBorderMaskDC() &&
		CreateForegroundBackBufferDC() &&
		CreateScrollBufferDC() &&
		CreateLogoDC() &&
		CreateMaskedForegroundDC();
}

/// <summary>
/// Deletes a given device context and corresponding bitmap.
/// </summary>
/// <param name="hDC">Device context to destroy.</param>
/// <param name="hBitmap">Bitmap to destroy.</param>
void DeleteDCAndBitmap(HDC hDC, HBITMAP hBitmap) {
	if (hDC)
		::DeleteDC(hDC);
	if (hBitmap)
		::DeleteObject(hBitmap);
}

/// <summary>
/// Cleans up all bitmaps and device contexts.
/// </summary>
void DestroyBitmaps() {
	DeleteDCAndBitmap(g_hMaskDC, g_hMaskBitmap);
	DeleteDCAndBitmap(g_hBorderMaskDC, g_hBorderMaskBitmap);
	DeleteDCAndBitmap(g_hMaskedForegroundDC, g_hMaskedForegroundBitmap);
	DeleteDCAndBitmap(g_hScrollBufferDC, g_hScrollBufferBitmap);
	DeleteDCAndBitmap(g_hScrollBufferDC, g_hScrollBufferBitmap);
	DeleteDCAndBitmap(g_hLogoDC, g_hLogoBitmap);
	DeleteDCAndBitmap(g_hBackgroundDC, g_hBackgroundBitmap);
	DeleteDCAndBitmap(g_hForegroundBackBufferDC, g_hForegroundBackBufferBitmap);
	for (int f = 0; f < SUPPORTED_SCALING_LEVELS; ++f)
		DeleteDCAndBitmap(g_hScaledForegroundDCs[f], g_hScaledForegroundBitmaps[f]);
	DestroyLogo();
	if (g_hTransparentBrush)
		::DeleteObject(g_hTransparentBrush);
	if (g_hForegroundBackBufferDCAccessMutex)
		::CloseHandle(g_hForegroundBackBufferDCAccessMutex);
	if (g_hMaskedForegroundDCAccessMutex)
		::CloseHandle(g_hMaskedForegroundDCAccessMutex);
	::ReleaseDC(NULL,g_hScreenDC);
}