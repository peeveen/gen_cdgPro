#include "stdafx.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGWindows.h"
#include "CDGBitmaps.h"
#include "CDGProcessor.h"
#include <objidl.h>
#include <stdlib.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;

// Logo image
Image* g_pLogoImage = NULL;
SIZE g_logoSize;

// Canvas pixel offsets for scrolling
int g_nCanvasXOffset = 0;
int g_nCanvasYOffset = 0;
// What section of the CDG canvas needs redrawn?
RECT g_redrawRect = { 0,0,0,0 };

void Perform2xSmoothing(BYTE* pSourceBitmapBits, BYTE* pDestinationBitmapBits, RECT *pInvalidRect, int nSourceBitmapWidth) {
	// 2x smoothing
	static BYTE EAandEB, BAandBB, HAandHB;
	static BYTE EA, EB;
	static BYTE B, D, F, H;
	static BYTE E0, E1, E2, E3;
	int nW = pInvalidRect->right - pInvalidRect->left;
	int nH = pInvalidRect->bottom - pInvalidRect->top;
	int left = pInvalidRect->left;
	int top = pInvalidRect->top;
	int right = pInvalidRect->right;
	int bottom = pInvalidRect->bottom;
	int halfBitmapWidth = nSourceBitmapWidth >> 1;
	int doubleBitmapWidth = nSourceBitmapWidth << 1;
	int destFinishingOffset = nSourceBitmapWidth + (nSourceBitmapWidth - nW);
	int sourceFinishingOffset = halfBitmapWidth - (nW / 2);
	pSourceBitmapBits += (left / 2) + (halfBitmapWidth * top);
	pDestinationBitmapBits += (left) + (doubleBitmapWidth * top);
	for (int y = top; y < bottom; ++y) {
		for (int x = left; x < right; x += 2) {
			// Each byte is two pixels.
			EAandEB = *pSourceBitmapBits;
			BAandBB = y ? *(pSourceBitmapBits - halfBitmapWidth) : EAandEB;
			HAandHB = y == bottom ? EAandEB : *(pSourceBitmapBits + halfBitmapWidth);
			EA = (EAandEB >> 4) & 0x0F;
			EB = EAandEB & 0x0F;

			// First pixel.
			D = x ? *(pSourceBitmapBits - 1) & 0x0F : EA;
			F = EB;
			B = (BAandBB >> 4) & 0x0F;
			H = (HAandHB >> 4) & 0x0F;

			if (B != H && D != F) {
				E0 = D == B ? D : EA;
				E1 = B == F ? F : EA;
				E2 = D == H ? D : EA;
				E3 = H == F ? F : EA;
			}
			else
				E0 = E1 = E2 = E3 = EA;
			*pDestinationBitmapBits = (E0 << 4) | E1;
			*((pDestinationBitmapBits++) + nSourceBitmapWidth) = (E2 << 4) | E3;

			// Second pixel.
			D = EA;
			F = x == right ? EB : (*(pSourceBitmapBits++ + 1) >> 4) & 0x0F;
			B = BAandBB & 0x0F;
			H = HAandHB & 0x0F;

			if (B != H && D != F) {
				E0 = D == B ? D : EB;
				E1 = B == F ? F : EB;
				E2 = D == H ? D : EB;
				E3 = H == F ? F : EB;
			}
			else
				E0 = E1 = E2 = E3 = EB;
			*pDestinationBitmapBits = (E0 << 4) | E1;
			*((pDestinationBitmapBits++) + nSourceBitmapWidth) = (E2 << 4) | E3;
		}
		pSourceBitmapBits += sourceFinishingOffset;
		pDestinationBitmapBits += destFinishingOffset;
	}
}

void DrawBackground() {
	RECT r;
	::GetClientRect(g_hBackgroundWindow, &r);
	::StretchBlt(g_hBackgroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hBackgroundDC, 0, 0, 1, 1, SRCCOPY);
}

void ScaleRect(RECT* pRect, int factor) {
	pRect->left *= factor;
	pRect->right *= factor;
	pRect->top *= factor;
	pRect->bottom *= factor;
}

void DrawForeground(RECT* pInvalidWindowRect) {
	static RECT windowClientRect;
	static RECT invalidCDGRect;
	static RECT outlineRect;
	RECT cdgDisplayRect = { 0,0,CDG_WIDTH,CDG_HEIGHT };
	::GetClientRect(g_hForegroundWindow, &windowClientRect);
	HDC hSourceDC = g_hScaledForegroundDCs[0];
	// If we got here and the redraw rect is {0,0,0,0}, we just need to refresh, not redraw.
	bool bDrawRequired = !!g_redrawRect.right;
	int nScaling = 1;
	if (bDrawRequired) {
		memcpy(&invalidCDGRect, &g_redrawRect, sizeof(RECT));
		::ZeroMemory(&g_redrawRect, sizeof(RECT));
		if (g_nSmoothingPasses) {
			// The smoothing algorithms have to operate on adjacent pixels, so we will have to include
			// an extra pixel on each side of the invalid rectangle. Also, the algorithm works on two
			// horizontal pixels at a time, and assumes even numbered start/ends ... it would be a more
			// complex algorithm to cater for odd numbered boundaries for very little increase in speed.
			// Therefore, we will keep the horizontal offsets even by adding yet ANOTHER pixel.
			::InflateRect(&invalidCDGRect, 2, 1);
			::IntersectRect(&invalidCDGRect, &invalidCDGRect, &cdgDisplayRect);
		}
		for (int f = 0; f < g_nSmoothingPasses && f < (SUPPORTED_SCALING_LEVELS - 1); ++f) {
			int sourceWidth = CDG_BITMAP_WIDTH * nScaling;
			Perform2xSmoothing(g_pScaledForegroundBitmapBits[f], g_pScaledForegroundBitmapBits[f + 1], &invalidCDGRect, sourceWidth);
			nScaling <<= 1;
			ScaleRect(&invalidCDGRect, 2);
			ScaleRect(&cdgDisplayRect, 2);
			hSourceDC = g_hScaledForegroundDCs[f + 1];
		}
		::BitBlt(g_hMaskDC, invalidCDGRect.left, invalidCDGRect.top, invalidCDGRect.right - invalidCDGRect.left, invalidCDGRect.bottom - invalidCDGRect.top, hSourceDC, invalidCDGRect.left, invalidCDGRect.top, SRCCOPY);
		::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
		// If drawing outlines, we will have to, possibly yet again, inflate the invalid rect to encompass the outline.
		memcpy(&outlineRect, &invalidCDGRect, sizeof(RECT));
		if (g_bDrawOutline) {
			::InflateRect(&outlineRect, nScaling, nScaling);
			::IntersectRect(&outlineRect, &outlineRect, &cdgDisplayRect);
		}
		for (int f = -nScaling; f <= nScaling; ++f)
			for (int g = -nScaling; g <= nScaling; ++g)
				if (g_bDrawOutline || (!f && !g))
					::BitBlt(g_hBorderMaskDC, f + outlineRect.left, g + outlineRect.top, outlineRect.right - outlineRect.left, outlineRect.bottom - outlineRect.top, g_hMaskDC, outlineRect.left, outlineRect.top, SRCPAINT);
		::MaskBlt(g_hMaskedForegroundDC, invalidCDGRect.left, invalidCDGRect.top, invalidCDGRect.right - invalidCDGRect.left, invalidCDGRect.bottom - invalidCDGRect.top, hSourceDC, invalidCDGRect.left, invalidCDGRect.top, g_hBorderMaskBitmap, invalidCDGRect.left, invalidCDGRect.top, MAKEROP4(SRCCOPY, PATCOPY));
	}
	else
		nScaling <<=g_nSmoothingPasses;
	double nInvalidRectXFactor = ((double)pInvalidWindowRect->left) / ((double)windowClientRect.right - windowClientRect.left);
	double nInvalidRectYFactor = ((double)pInvalidWindowRect->top) / ((double)windowClientRect.bottom - windowClientRect.top);
	double nInvalidRectWFactor = ((double)((double)pInvalidWindowRect->right- pInvalidWindowRect->left)) / ((double)windowClientRect.right - windowClientRect.left);
	double nInvalidRectHFactor = ((double)((double)pInvalidWindowRect->bottom- pInvalidWindowRect->top)) / ((double)windowClientRect.bottom - windowClientRect.top);
	int nCanvasSourceX = (int)(CDG_CANVAS_WIDTH * nScaling * nInvalidRectXFactor) + ((CDG_CANVAS_X + g_nCanvasXOffset) * nScaling);
	int nCanvasSourceY = (int)(CDG_CANVAS_HEIGHT * nScaling * nInvalidRectYFactor) + ((CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling);
	int nCanvasWidth = (int)(CDG_CANVAS_WIDTH * nScaling * nInvalidRectWFactor);
	int nCanvasHeight = (int)(CDG_CANVAS_HEIGHT * nScaling * nInvalidRectHFactor);
	::StretchBlt(g_hForegroundWindowDC, pInvalidWindowRect->left, pInvalidWindowRect->top, pInvalidWindowRect->right - pInvalidWindowRect->left, pInvalidWindowRect->bottom - pInvalidWindowRect->top, g_hMaskedForegroundDC, nCanvasSourceX, nCanvasSourceY, nCanvasWidth, nCanvasHeight, SRCCOPY);
	if (g_pLogoImage && !g_nCDGPC) {
		RECT r;
		::GetClientRect(g_hForegroundWindow, &r);
		int windowWidth = r.right - r.left;
		int windowHeight = r.bottom - r.top;
		Graphics g(g_hForegroundWindowDC);
		g.DrawImage(g_pLogoImage, (windowWidth - g_logoSize.cx) / 2, (windowHeight - g_logoSize.cy) / 2, g_logoSize.cx, g_logoSize.cy);
	}
}

void RefreshScreen() {
	::BitBlt(g_hMaskDC, 0, 0, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, g_hScaledForegroundDCs[SUPPORTED_SCALING_LEVELS - 1], 0, 0, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	::BitBlt(g_hBorderMaskDC, 0, 0, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, g_hMaskDC, 0, 0, SRCPAINT);
	::MaskBlt(g_hMaskedForegroundDC, 0, 0, CDG_MAXIMUM_BITMAP_WIDTH, CDG_MAXIMUM_BITMAP_HEIGHT, g_hScaledForegroundDCs[SUPPORTED_SCALING_LEVELS - 1], 0, 0, g_hBorderMaskBitmap, 0, 0, MAKEROP4(SRCCOPY, PATCOPY));
}

void LoadLogo() {
	g_pLogoImage = new Image(g_pszLogoPath);
	if (g_pLogoImage->GetLastStatus() == Ok) {
		g_logoSize = { (LONG)g_pLogoImage->GetWidth(),(LONG)g_pLogoImage->GetHeight() };
	}
	else {
		delete g_pLogoImage;
		g_pLogoImage = NULL;
	}
}

void DestroyLogo() {
	if (g_pLogoImage)
		delete g_pLogoImage;
}