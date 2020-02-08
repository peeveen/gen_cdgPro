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

// Canvas pixel offsets for scrolling
int g_nCanvasXOffset = 0;
int g_nCanvasYOffset = 0;

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

void RefreshScreen(RECT* pInvalidCDGRect) {
	static RECT outlineRect;
	HDC hSourceDC = g_hScaledForegroundDCs[g_nSmoothingPasses];
	int nScaling = 1<<g_nSmoothingPasses;
	RECT bitmapRect = { 0,0,CDG_BITMAP_WIDTH * nScaling,CDG_BITMAP_HEIGHT * nScaling };
	if (!pInvalidCDGRect)
		pInvalidCDGRect = &bitmapRect;
	int invalidCDGRectWidth = pInvalidCDGRect->right - pInvalidCDGRect->left;
	int invalidCDGRectHeight = pInvalidCDGRect->bottom - pInvalidCDGRect->top;
	::BitBlt(g_hMaskDC, pInvalidCDGRect->left, pInvalidCDGRect->top, invalidCDGRectWidth, invalidCDGRectHeight, hSourceDC, pInvalidCDGRect->left, pInvalidCDGRect->top, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	// If drawing outlines, we will have to, possibly yet again, inflate the invalid rect to encompass the outline.
	memcpy(&outlineRect, pInvalidCDGRect, sizeof(RECT));
	if (g_bDrawOutline) {
		static RECT tempRect;
		::InflateRect(&outlineRect, nScaling, nScaling);
		memcpy(&tempRect, &outlineRect, sizeof(RECT));
		::IntersectRect(&outlineRect, &tempRect, &bitmapRect);
	}
	for (int f = -nScaling; f <= nScaling; ++f)
		for (int g = -nScaling; g <= nScaling; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f + outlineRect.left, g + outlineRect.top, outlineRect.right - outlineRect.left, outlineRect.bottom - outlineRect.top, g_hMaskDC, outlineRect.left, outlineRect.top, SRCPAINT);
	::MaskBlt(g_hMaskedForegroundDC, pInvalidCDGRect->left, pInvalidCDGRect->top, invalidCDGRectWidth, invalidCDGRectHeight, hSourceDC, pInvalidCDGRect->left, pInvalidCDGRect->top, g_hBorderMaskBitmap, pInvalidCDGRect->left, pInvalidCDGRect->top, MAKEROP4(SRCCOPY, PATCOPY));
}

void RedrawForeground(RECT* pInvalidCDGRect) {
	RECT cdgDisplayRect = { 0,0,CDG_WIDTH,CDG_HEIGHT };
	if (g_nSmoothingPasses) {
		// The smoothing algorithms have to operate on adjacent pixels, so we will have to include
		// an extra pixel on each side of the invalid rectangle. Also, the algorithm works on two
		// horizontal pixels at a time, and assumes even numbered start/ends ... it would be a more
		// complex algorithm to cater for odd numbered boundaries for very little increase in speed.
		// Therefore, we will keep the horizontal offsets even by adding yet ANOTHER pixel.
		static RECT tempRect;
		::InflateRect(pInvalidCDGRect, 2, 1);
		memcpy(&tempRect, pInvalidCDGRect, sizeof(RECT));
		::IntersectRect(pInvalidCDGRect, &tempRect, &cdgDisplayRect);
	}
	int nScaling = 1;
	for (int f = 0; f < g_nSmoothingPasses && f < (SUPPORTED_SCALING_LEVELS - 1); ++f) {
		int sourceWidth = CDG_BITMAP_WIDTH * nScaling;
		Perform2xSmoothing(g_pScaledForegroundBitmapBits[f], g_pScaledForegroundBitmapBits[f + 1], pInvalidCDGRect, sourceWidth);
		nScaling <<= 1;
		ScaleRect(pInvalidCDGRect, 2);
		ScaleRect(&cdgDisplayRect, 2);
	}
	RefreshScreen(pInvalidCDGRect);
}

void DrawForeground(RECT* pInvalidWindowRect) {
	static RECT windowClientRect;
	::GetClientRect(g_hForegroundWindow, &windowClientRect);
	int nScaling = 1 <<g_nSmoothingPasses;
	double windowClientRectWidth = windowClientRect.right - windowClientRect.left;
	double windowClientRectHeight = windowClientRect.bottom - windowClientRect.top;
	double nInvalidRectXFactor = ((double)pInvalidWindowRect->left) / windowClientRectWidth;
	double nInvalidRectYFactor = ((double)pInvalidWindowRect->top) / windowClientRectHeight;
	double nInvalidRectWFactor = ((double)((double)pInvalidWindowRect->right- pInvalidWindowRect->left)) / windowClientRectWidth;
	double nInvalidRectHFactor = ((double)((double)pInvalidWindowRect->bottom- pInvalidWindowRect->top)) / windowClientRectHeight;
	int nCanvasSourceX = (int)(CDG_CANVAS_WIDTH * nScaling * nInvalidRectXFactor) + ((CDG_CANVAS_X + g_nCanvasXOffset) * nScaling);
	int nCanvasSourceY = (int)(CDG_CANVAS_HEIGHT * nScaling * nInvalidRectYFactor) + ((CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling);
	int nCanvasWidth = (int)(CDG_CANVAS_WIDTH * nScaling * nInvalidRectWFactor);
	int nCanvasHeight = (int)(CDG_CANVAS_HEIGHT * nScaling * nInvalidRectHFactor);
	int nInvalidRectWidth = pInvalidWindowRect->right - pInvalidWindowRect->left;
	int nInvalidRectHeight = pInvalidWindowRect->bottom - pInvalidWindowRect->top;
	::FillRect(g_hForegroundWindowDC, pInvalidWindowRect, g_hTransparentBrush);
	double scaleXMultiplier = windowClientRectWidth / CDG_CANVAS_WIDTH;
	double scaleYMultiplier = windowClientRectHeight / CDG_CANVAS_HEIGHT;
	int nScaledXMargin = (int)(g_nMargin * scaleXMultiplier);
	int nScaledYMargin = (int)(g_nMargin * scaleYMultiplier);
	::InflateRect(pInvalidWindowRect, -nScaledXMargin, -nScaledYMargin);
	::StretchBlt(g_hForegroundWindowDC, pInvalidWindowRect->left, pInvalidWindowRect->top, nInvalidRectWidth - (nScaledXMargin << 1), nInvalidRectHeight - (nScaledYMargin << 1), g_hMaskedForegroundDC, nCanvasSourceX, nCanvasSourceY, nCanvasWidth, nCanvasHeight, SRCCOPY);
}

