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

void PaintForegroundBackBuffer() {
	int nScaling = 1 << g_nSmoothingPasses;
	int nCanvasSourceX = (CDG_CANVAS_X + g_nCanvasXOffset) * nScaling;
	int nCanvasSourceY = (CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling;
	int nCanvasWidth = CDG_CANVAS_WIDTH * nScaling;
	int nCanvasHeight = CDG_CANVAS_HEIGHT * nScaling;
	::WaitForSingleObject(g_hForegroundBackBufferDCAccessMutex, INFINITE);
	RECT backBufferRect = { 0,0,g_nForegroundBackBufferWidth,g_nForegroundBackBufferHeight };
	double scaleXMultiplier = g_nForegroundBackBufferWidth / (double)CDG_CANVAS_WIDTH;
	double scaleYMultiplier = g_nForegroundBackBufferHeight / (double)CDG_CANVAS_HEIGHT;
	int nScaledXMargin = (int)(g_nMargin * scaleXMultiplier);
	int nScaledYMargin = (int)(g_nMargin * scaleYMultiplier);
	::FillRect(g_hForegroundBackBufferDC, &backBufferRect, g_hTransparentBrush);
	::InflateRect(&backBufferRect, -nScaledXMargin, -nScaledYMargin);
	::WaitForSingleObject(g_hMaskedForegroundDCAccessMutex, INFINITE);
	::StretchBlt(g_hForegroundBackBufferDC, backBufferRect.left, backBufferRect.top, backBufferRect.right - backBufferRect.left, backBufferRect.bottom - backBufferRect.top, g_hMaskedForegroundDC, nCanvasSourceX, nCanvasSourceY, nCanvasWidth, nCanvasHeight, SRCCOPY);
	::ReleaseMutex(g_hMaskedForegroundDCAccessMutex);
	::ReleaseMutex(g_hForegroundBackBufferDCAccessMutex);
}

void ScaleRect(RECT* pRect, int nScale) {
	pRect->left *= nScale;
	pRect->right *= nScale;
	pRect->top *= nScale;
	pRect->bottom *= nScale;
}

void RenderForegroundBackBuffer(RECT* pInvalidCDGRect) {
	static RECT invalidRect;
	HDC hSourceDC = g_hScaledForegroundDCs[g_nSmoothingPasses];
	int nScaling = 1<<g_nSmoothingPasses;
	RECT bitmapRect = { CDG_CANVAS_X,CDG_CANVAS_Y,CDG_CANVAS_X + CDG_CANVAS_WIDTH,CDG_CANVAS_Y + CDG_CANVAS_HEIGHT };
	if (pInvalidCDGRect)
		memcpy(&invalidRect, pInvalidCDGRect, sizeof(RECT));
	else
		memcpy(&invalidRect, &bitmapRect, sizeof(RECT));
	ScaleRect(&bitmapRect, nScaling);
	ScaleRect(&invalidRect, nScaling);
	int invalidCDGRectWidth = invalidRect.right - invalidRect.left;
	int invalidCDGRectHeight = invalidRect.bottom - invalidRect.top;
	::BitBlt(g_hMaskDC, invalidRect.left, invalidRect.top, invalidCDGRectWidth, invalidCDGRectHeight, hSourceDC, invalidRect.left, invalidRect.top, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	static RECT outlineRect;
	memcpy(&outlineRect, &invalidRect, sizeof(RECT));
	if (g_bDrawOutline) {
		// The outline is twice as thick as the scaling value (outline covers all directions)
		::InflateRect(&outlineRect, nScaling<<1, nScaling<<1);
		::IntersectRect(&outlineRect, &outlineRect, &bitmapRect);
	}
	if (g_nSmoothingPasses) {
		::InflateRect(&invalidRect, nScaling, nScaling);
		::IntersectRect(&invalidRect, &invalidRect, &bitmapRect);
	}
	invalidCDGRectWidth = invalidRect.right - invalidRect.left;
	invalidCDGRectHeight = invalidRect.bottom - invalidRect.top;
	for (int f = -nScaling; f <= nScaling; ++f)
		for (int g = -nScaling; g <= nScaling; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f + outlineRect.left, g + outlineRect.top, outlineRect.right- outlineRect.left, outlineRect.bottom - outlineRect.top, g_hMaskDC, outlineRect.left, outlineRect.top, SRCPAINT);
	::WaitForSingleObject(g_hMaskedForegroundDCAccessMutex, INFINITE);
	::MaskBlt(g_hMaskedForegroundDC, invalidRect.left, invalidRect.top, invalidCDGRectWidth, invalidCDGRectHeight, hSourceDC, invalidRect.left, invalidRect.top, g_hBorderMaskBitmap, invalidRect.left, invalidRect.top, MAKEROP4(SRCCOPY, PATCOPY));
	::ReleaseMutex(g_hMaskedForegroundDCAccessMutex);
	PaintForegroundBackBuffer();
}

void DrawForeground(RECT* pInvalidCDGRect) {
	RECT cdgDisplayRect = { 0,0,CDG_WIDTH,CDG_HEIGHT };
	static RECT redrawRect;
	memcpy(&redrawRect, pInvalidCDGRect, sizeof(RECT));
	if (g_nSmoothingPasses) {
		// The smoothing algorithms have to operate on adjacent pixels, so we will have to include
		// an extra pixel on each side of the invalid rectangle. Also, the algorithm works on two
		// horizontal pixels at a time, and assumes even numbered start/ends ... it would be a more
		// complex algorithm to cater for odd numbered boundaries for very little increase in speed.
		// Therefore, we will keep the horizontal offsets even by adding yet ANOTHER pixel.
		::InflateRect(&redrawRect, 2, 1);
		::IntersectRect(&redrawRect, &redrawRect, &cdgDisplayRect);
	}
	for (int f = 0; f < g_nSmoothingPasses && f < (SUPPORTED_SCALING_LEVELS - 1); ++f) {
		int sourceWidth = CDG_BITMAP_WIDTH * (1<<f);
		Perform2xSmoothing(g_pScaledForegroundBitmapBits[f], g_pScaledForegroundBitmapBits[f + 1], &redrawRect, sourceWidth);
		ScaleRect(&redrawRect,2);
		ScaleRect(&cdgDisplayRect,2);
	}
}

void PaintForeground(RECT *pInvalidRect) {
	::WaitForSingleObject(g_hForegroundBackBufferDCAccessMutex, INFINITE);
	::BitBlt(g_hForegroundWindowDC, pInvalidRect->left, pInvalidRect->top, pInvalidRect->right - pInvalidRect->left, pInvalidRect->bottom - pInvalidRect->top, g_hForegroundBackBufferDC, pInvalidRect->left, pInvalidRect->top, SRCCOPY);
	::ReleaseMutex(g_hForegroundBackBufferDCAccessMutex);
}