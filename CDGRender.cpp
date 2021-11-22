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

/// <summary>
/// Scale2X smoothing algorithm. You can find details about this online.
/// </summary>
/// <param name="pSourceBitmapBits">The source bitmap to smooth.</param>
/// <param name="pDestinationBitmapBits">The destination bitmap. Must be 2x the size.</param>
/// <param name="pInvalidRect">The rectangle of the source bitmap that needs redone.</param>
/// <param name="nSourceBitmapWidth">The width of the source bitmap. Needed for some coordinate calculation, and
/// saves time to pass it in than calculate it from the provided bitmap.</param>
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

/// <summary>
/// Draw the background. We streeeeetch out one coloured pixel to the entire window.
/// </summary>
void DrawBackground() {
	RECT r;
	::GetClientRect(g_hBackgroundWindow, &r);
	::StretchBlt(g_hBackgroundWindowDC, 0, 0, r.right - r.left, r.bottom - r.top, g_hBackgroundDC, 0, 0, 1, 1, SRCCOPY);
}

/// <summary>
/// Paint the foreground back buffer.
/// </summary>
void PaintForegroundBackBuffer() {
	int nScaling = 1 << g_nSmoothingPasses;
	int nCanvasSourceX = (CDG_CANVAS_X + g_nCanvasXOffset) * nScaling;
	int nCanvasSourceY = (CDG_CANVAS_Y + g_nCanvasYOffset) * nScaling;
	int nCanvasWidth = CDG_CANVAS_WIDTH * nScaling;
	int nCanvasHeight = CDG_CANVAS_HEIGHT * nScaling;
	// Blit area needs to be a little larger to accomodate the extremities of any outlines.
	if (g_bDrawOutline) {
		int nScalingOutlinePosDiff = nScaling << 1;
		int nScalingOutlineSizeDiff = nScalingOutlinePosDiff << 1;
		nCanvasSourceX -= nScalingOutlinePosDiff;
		nCanvasSourceY -= nScalingOutlinePosDiff;
		nCanvasWidth += nScalingOutlineSizeDiff;
		nCanvasHeight += nScalingOutlineSizeDiff;
	}
	::WaitForSingleObject(g_hForegroundBackBufferDCAccessMutex, INFINITE);
	RECT backBufferRect = { 0, 0, g_nForegroundBackBufferWidth, g_nForegroundBackBufferHeight };
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

/// <summary>
/// Scale the given rectangle.
/// </summary>
/// <param name="pRect">Rectangle to scale.</param>
/// <param name="nScale">Amount to scale by.</param>
void ScaleRect(RECT* pRect, int nScale) {
	pRect->left *= nScale;
	pRect->right *= nScale;
	pRect->top *= nScale;
	pRect->bottom *= nScale;
}

/// <summary>
/// Renders the foreground back buffer.
/// </summary>
/// <param name="pInvalidCDGRect">Rectangle of area that needs rendered.</param>
void RenderForegroundBackBuffer(RECT* pInvalidCDGRect) {
	static RECT invalidRect;
	// Choose the right scaled foreground DC.
	HDC hSourceDC = g_hScaledForegroundDCs[g_nSmoothingPasses];
	int nScaling = 1<<g_nSmoothingPasses;
	static RECT cdgAllRect = { 0, 0, CDG_WIDTH, CDG_HEIGHT };
	// Inflate the canvas rect by 1 to cover any outline. We might not be drawing
	// an outline, but in the grand scheme of things, the time taken to blit
	// an area a tiny bit larger will be inconsequential.
	static RECT cdgCanvasRect = { CDG_CANVAS_X - 1, CDG_CANVAS_Y - 1, CDG_CANVAS_X + CDG_CANVAS_WIDTH + 1, CDG_CANVAS_Y + CDG_CANVAS_HEIGHT + 1 };
	RECT cdgRect;
	// If not performing a scrolling (offset) operation, we can limit the graphical operation to the canvas.
	// Otherwise, need to take the normally-invisible border graphics into account.
	memcpy(&cdgRect, (g_nCanvasXOffset == 0 && g_nCanvasYOffset == 0 ? &cdgCanvasRect : &cdgAllRect), sizeof(RECT));
	if (pInvalidCDGRect)
		memcpy(&invalidRect, pInvalidCDGRect, sizeof(RECT));
	else
		memcpy(&invalidRect, &cdgRect, sizeof(RECT));

	// Scale the rectangles accordingly.
	ScaleRect(&cdgRect, nScaling);
	ScaleRect(&invalidRect, nScaling);

	int invalidCDGRectWidth = invalidRect.right - invalidRect.left;
	int invalidCDGRectHeight = invalidRect.bottom - invalidRect.top;

	// Blit the foreground DC to the mask DC. The mask DC/bitmap is monochrome (palette 0=white, palette 1=black),
	// so this will convert all palette 0 entries in the source DC to white, and all others to black.
	::BitBlt(g_hMaskDC, invalidRect.left, invalidRect.top, invalidCDGRectWidth, invalidCDGRectHeight, hSourceDC, invalidRect.left, invalidRect.top, SRCCOPY);

	// Clear the border mask bitmap.
	::ZeroMemory(g_pBorderMaskBitmapBits, (CDG_MAXIMUM_BITMAP_WIDTH * CDG_MAXIMUM_BITMAP_HEIGHT) / 8);
	static RECT outlineRect;
	memcpy(&outlineRect, &invalidRect, sizeof(RECT));
	if (g_bDrawOutline) {
		// The outline is twice as thick as the scaling value (outline covers all directions)
		::InflateRect(&outlineRect, nScaling<<1, nScaling<<1);
		::IntersectRect(&outlineRect, &outlineRect, &cdgRect);
	}
	if (g_nSmoothingPasses) {
		::InflateRect(&invalidRect, nScaling, nScaling);
		::IntersectRect(&invalidRect, &invalidRect, &cdgRect);
	}
	invalidCDGRectWidth = invalidRect.right - invalidRect.left;
	invalidCDGRectHeight = invalidRect.bottom - invalidRect.top;
	// Blit the mask bitmap to the border mask bitmap. To create an outline, we have to do
	// this NxN times to make a mask that is thicker than the original foreground graphics.
	for (int f = -nScaling; f <= nScaling; ++f)
		for (int g = -nScaling; g <= nScaling; ++g)
			if (g_bDrawOutline || (!f && !g))
				::BitBlt(g_hBorderMaskDC, f + outlineRect.left, g + outlineRect.top, outlineRect.right- outlineRect.left, outlineRect.bottom - outlineRect.top, g_hMaskDC, outlineRect.left, outlineRect.top, SRCPAINT);

	::WaitForSingleObject(g_hMaskedForegroundDCAccessMutex, INFINITE);
	// Now blit the foreground bitmap to the masked foreground bitmap, using the border mask bitmap as a mask.
	// Only bits from the foreground bitmap that "get through" the mask will make it to the masked foreground bitmap,
	// leaving transparency everywhere else.
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