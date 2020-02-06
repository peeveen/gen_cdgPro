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

void DrawForeground() {
	RECT r;
	static RECT cdgDisplayRect = { 0,0,CDG_WIDTH,CDG_HEIGHT };
	::GetClientRect(g_hForegroundWindow, &r);
	HDC hSourceDC = g_hScaledForegroundDCs[0];
	RECT invalidRect;
	memcpy(&invalidRect, &g_redrawRect, sizeof(RECT));
	int nScaling = 1;
	if (g_nSmoothingPasses) {
		// The smoothing algorithms have to operate on adjacent pixels, so we will have to include
		// an extra pixel on each side of the invalid rectangle. Also, since the algorithm jumps
		// two pixel each time on the horizontal axis, we will have to adjust for another extra
		// pixel horizontally otherwise the algorithm will miss a pixel if the boundary is on an
		// odd-numbered column. It's a tiny bit suboptimal, but we'll not kill ourselves over it.
		::InflateRect(&invalidRect, 2, 1);
		::IntersectRect(&invalidRect, &invalidRect, &cdgDisplayRect);
	}
	for (int f = 0; f < g_nSmoothingPasses && f < (SUPPORTED_SCALING_LEVELS - 1); ++f) {
		int sourceWidth = CDG_BITMAP_WIDTH * nScaling;
		Perform2xSmoothing(g_pScaledForegroundBitmapBits[f], g_pScaledForegroundBitmapBits[f + 1], &invalidRect, sourceWidth);
		nScaling *= 2;
		invalidRect.left *= 2;
		invalidRect.right *= 2;
		invalidRect.top *= 2;
		invalidRect.bottom *= 2;
		hSourceDC = g_hScaledForegroundDCs[f + 1];
	}
	::ZeroMemory(&g_redrawRect, sizeof(RECT));
	::BitBlt(g_hMaskDC, 0, 0, CDG_BITMAP_WIDTH * nScaling, CDG_BITMAP_HEIGHT * nScaling, hSourceDC, 0, 0, SRCCOPY);
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	for (int f = -nScaling; f <= nScaling; ++f)
		for (int g = -nScaling; g <= nScaling; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f, g, CDG_BITMAP_WIDTH * nScaling, CDG_BITMAP_HEIGHT * nScaling, g_hMaskDC, 0, 0, SRCPAINT);
	::MaskBlt(g_hMaskedForegroundDC, 0, 0, CDG_BITMAP_WIDTH * nScaling, CDG_BITMAP_HEIGHT * nScaling, hSourceDC, 0, 0, g_hBorderMaskBitmap, 0, 0, MAKEROP4(SRCCOPY, PATCOPY));
	::StretchBlt(g_hForegroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hMaskedForegroundDC, (CDG_CANVAS_X + g_nCanvasXOffset) * nScaling, (CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling, CDG_CANVAS_WIDTH * nScaling, CDG_CANVAS_HEIGHT * nScaling, SRCCOPY);
	if (g_pLogoImage && !g_nCDGPC) {
		RECT r;
		::GetClientRect(g_hForegroundWindow, &r);
		int windowWidth = r.right - r.left;
		int windowHeight = r.bottom - r.top;
		Graphics g(g_hForegroundWindowDC);
		g.DrawImage(g_pLogoImage, (windowWidth - g_logoSize.cx) / 2, (windowHeight - g_logoSize.cy) / 2, g_logoSize.cx, g_logoSize.cy);
	}
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