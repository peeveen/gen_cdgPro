#include "stdafx.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGBitmaps.h"
#include "CDGBackgroundFunctions.h"
#include "CDGRender.h"
#include "CDGPalette.h"

// We keep track of the last "reset" color. If we receive a MemoryPreset command for this color
// again before anything else has been drawn, we can ignore it.
BYTE g_nLastMemoryPresetColor = -1;

BYTE MemoryPreset(BYTE color, BYTE repeat) {
	if (g_nLastMemoryPresetColor == color)
		return 0x00;
	memset(g_pScaledForegroundBitmapBits[0], (color << 4) | color, (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	g_nLastMemoryPresetColor = color;
	switch (g_nBackgroundDetectionMode) {
	case BDM_TOPLEFTPIXEL:
	case BDM_TOPRIGHTPIXEL:
	case BDM_BOTTOMLEFTPIXEL:
	case BDM_BOTTOMRIGHTPIXEL:
		// All pixels will be the same value at this point, so use any corner.
		SetBackgroundColorFromPixel(TOP_LEFT_PIXEL_OFFSET, true);
		return 0x03;
	default:
		return 0x01;
	}
}

void BorderPreset(BYTE color) {
	BYTE colorByte = (color << 4) | color;
	// Top and bottom edge.
	BYTE* pForegroundBitmapBits = g_pScaledForegroundBitmapBits[0];
	memset(pForegroundBitmapBits, colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	memset(pForegroundBitmapBits + (((CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) - (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT)) / 2), colorByte, (CDG_BITMAP_WIDTH * CDG_CELL_HEIGHT) / 2);
	// Left and right edge.
	for (int f = CDG_CELL_HEIGHT; f < CDG_CELL_HEIGHT + CDG_CANVAS_HEIGHT; ++f) {
		memset(pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
		memset(pForegroundBitmapBits + ((f * CDG_BITMAP_WIDTH) / 2) + ((CDG_WIDTH - CDG_CELL_WIDTH) / 2), colorByte, CDG_CELL_WIDTH / 2);
	}
	// Screen is no longer "blank".
	g_nLastMemoryPresetColor = -1;
}

BYTE TileBlock(BYTE* pData, bool isXor) {
	// 3 byte buffer that we will use to set values in the CDG raster.
	static BYTE g_blockBuffer[3];
	BYTE bgColor = pData[0] & 0x0F;
	BYTE fgColor = pData[1] & 0x0F;
	// Python CDG parser code suggests this bit means "ignore this command"?
	//if (pData[1] & 0x20)
	//	return;
	BYTE row = pData[2] & 0x3F;
	BYTE col = pData[3] & 0x3F;
	// If the coordinates are offscreen, reject them as bad data.
	if (col >= CDG_WIDTH_CELLS || row >= CDG_HEIGHT_CELLS)
		return 0x00;
	BYTE upperFgColor = fgColor << 4;
	BYTE upperBgColor = bgColor << 4;
	int xPixel = col * CDG_CELL_WIDTH;
	int yPixel = row * CDG_CELL_HEIGHT;
	int foregroundBitmapOffset = ((xPixel)+(yPixel * CDG_BITMAP_WIDTH)) / 2;
	BYTE* pForegroundBitmapBits = g_pScaledForegroundBitmapBits[0];
	// The remaining 12 bytes in the data field will contain the bitmask of pixels to set.
	// The lower six bits of each byte are the pixel mask.
	for (int f = 0; f < 12; ++f) {
		BYTE bits = pData[f + 4];
		g_blockBuffer[0] = ((bits & 0x20) ? upperFgColor : upperBgColor) | ((bits & 0x10) ? fgColor : bgColor);
		g_blockBuffer[1] = ((bits & 0x08) ? upperFgColor : upperBgColor) | ((bits & 0x04) ? fgColor : bgColor);
		g_blockBuffer[2] = ((bits & 0x02) ? upperFgColor : upperBgColor) | ((bits & 0x01) ? fgColor : bgColor);
		if (isXor) {
			pForegroundBitmapBits[foregroundBitmapOffset] ^= g_blockBuffer[0];
			pForegroundBitmapBits[foregroundBitmapOffset + 1] ^= g_blockBuffer[1];
			pForegroundBitmapBits[foregroundBitmapOffset + 2] ^= g_blockBuffer[2];
		}
		else {
			pForegroundBitmapBits[foregroundBitmapOffset] = g_blockBuffer[0];
			pForegroundBitmapBits[foregroundBitmapOffset + 1] = g_blockBuffer[1];
			pForegroundBitmapBits[foregroundBitmapOffset + 2] = g_blockBuffer[2];
		}
		foregroundBitmapOffset += (CDG_BITMAP_WIDTH / 2);
	}
	// Did we write to the non-border screen area?
	BYTE result = (row < (CDG_HEIGHT_CELLS - 1) && row>0 && col < (CDG_WIDTH_CELLS - 1) && col>0) ? 0x03 : 0x02;
	// Also need to know if the background needs refreshed.
	bool topLeftPixelSet = col == 1 && row == 1;
	bool topRightPixelSet = col == CDG_WIDTH_CELLS - 2 && row == 1;
	bool bottomLeftPixelSet = col == 1 && row == CDG_HEIGHT_CELLS - 2;
	bool bottomRightPixelSet = col == CDG_WIDTH_CELLS - 2 && row == CDG_HEIGHT_CELLS - 2;
	if (topLeftPixelSet || topRightPixelSet || bottomLeftPixelSet || bottomRightPixelSet)
		if (!CheckPixelColorBackgroundChange(topLeftPixelSet, topRightPixelSet, bottomLeftPixelSet, bottomRightPixelSet))
			result &= 0x01;
	// Screen is no longer blank.
	g_nLastMemoryPresetColor = -1;
	return result;
}

BYTE LoadColorTable(BYTE* pData, bool highTable) {
	int nPaletteStartIndex = highTable ? 8 : 0;
	RGBQUAD rgbQuads[8];
	for (int f = 0; f < 8; ++f) {
		BYTE colorByte1 = pData[f * 2] & 0x3F;
		BYTE colorByte2 = pData[(f * 2) + 1] & 0x3F;
		// Get 4-bit color values.
		BYTE red = (colorByte1 >> 2) & 0x0F;
		BYTE green = ((colorByte1 << 2) & 0x0C) | ((colorByte2 >> 4) & 0x03);
		BYTE blue = colorByte2 & 0x0F;
		// Convert to 24-bit color.
		red = (red * 17);
		green = (green * 17);
		blue = (blue * 17);
		rgbQuads[f] = { blue,green,red,0 };
	}
	SetPalette(rgbQuads, nPaletteStartIndex, 8);
	return 0x01 | (g_nCurrentTransparentIndex >= nPaletteStartIndex && g_nCurrentTransparentIndex < nPaletteStartIndex + 8 ? 0x02 : 0x00);
}

BYTE Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy) {
	int nHScrollPixels = ((hScroll == 2 ? -1 : (hScroll == 1 ? 1 : 0)) * CDG_CELL_WIDTH);
	int nVScrollPixels = ((vScroll == 2 ? -1 : (vScroll == 1 ? 1 : 0)) * CDG_CELL_HEIGHT);
	g_nCanvasXOffset = hScrollOffset;
	g_nCanvasYOffset = vScrollOffset;
	// This should be faster than BitBlt
	memcpy(g_pScrollBufferBitmapBits, g_pScaledForegroundBitmapBits[0], (CDG_BITMAP_WIDTH * CDG_BITMAP_HEIGHT) / 2);
	HDC hForegroundDC = g_hScaledForegroundDCs[0];
	DWORD rop = SRCCOPY;
	::BitBlt(hForegroundDC, nHScrollPixels, nVScrollPixels, CDG_BITMAP_WIDTH, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, rop);
	HBRUSH oldBrush = NULL;
	HBRUSH solidBrush = NULL;
	if (!copy) {
		solidBrush = ::CreateSolidBrush(RGB(g_palette[color].rgbRed, g_palette[color].rgbGreen, g_palette[color].rgbBlue));
		oldBrush = (HBRUSH)::SelectObject(hForegroundDC, solidBrush);
		rop = PATCOPY;
		if (color != g_nLastMemoryPresetColor)
			g_nLastMemoryPresetColor = -1;
	}
	if (nVScrollPixels > 0) {
		::BitBlt(hForegroundDC, nHScrollPixels, 0, CDG_BITMAP_WIDTH, nVScrollPixels, g_hScrollBufferDC, 0, CDG_HEIGHT - nVScrollPixels, rop);
		if (nHScrollPixels > 0)
			::BitBlt(hForegroundDC, (-CDG_WIDTH) + nHScrollPixels, 0, CDG_BITMAP_WIDTH, nVScrollPixels, g_hScrollBufferDC, 0, CDG_HEIGHT - nVScrollPixels, rop);
		else if (nHScrollPixels < 0)
			::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, 0, CDG_BITMAP_WIDTH, nVScrollPixels, g_hScrollBufferDC, 0, CDG_HEIGHT - nVScrollPixels, rop);
	}
	else if (nVScrollPixels < 0) {
		::BitBlt(hForegroundDC, nHScrollPixels, CDG_HEIGHT + nVScrollPixels, CDG_BITMAP_WIDTH, -nVScrollPixels, g_hScrollBufferDC, 0, 0, rop);
		if (nHScrollPixels > 0)
			::BitBlt(hForegroundDC, (-CDG_WIDTH) + nHScrollPixels, CDG_HEIGHT + nVScrollPixels, CDG_BITMAP_WIDTH, -nVScrollPixels, g_hScrollBufferDC, 0, 0, rop);
		else if (nHScrollPixels < 0)
			::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, CDG_HEIGHT + nVScrollPixels, CDG_BITMAP_WIDTH, -nVScrollPixels, g_hScrollBufferDC, 0, 0, rop);
	}
	if (nHScrollPixels > 0) {
		::BitBlt(hForegroundDC, 0, nVScrollPixels, nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, CDG_WIDTH - nHScrollPixels, 0, rop);
		if (nVScrollPixels > 0)
			::BitBlt(hForegroundDC, 0, (-CDG_HEIGHT) + nVScrollPixels, nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, CDG_WIDTH - nHScrollPixels, 0, rop);
		else if (nVScrollPixels < 0)
			::BitBlt(hForegroundDC, 0, CDG_HEIGHT + nVScrollPixels, nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, CDG_WIDTH - nHScrollPixels, 0, rop);
	}
	else if (nHScrollPixels < 0) {
		::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, nVScrollPixels, -nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, rop);
		if (nVScrollPixels > 0)
			::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, (-CDG_HEIGHT) + nVScrollPixels, -nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, rop);
		else if (nVScrollPixels < 0)
			::BitBlt(hForegroundDC, CDG_WIDTH + nHScrollPixels, CDG_HEIGHT + nVScrollPixels, -nHScrollPixels, CDG_BITMAP_HEIGHT, g_hScrollBufferDC, 0, 0, rop);
	}
	if (!copy) {
		::SelectObject(hForegroundDC, oldBrush);
		::DeleteObject(solidBrush);
	}
	return CheckPixelColorBackgroundChange(true, true, true, true) ? 0x03 : 0x01;
}
