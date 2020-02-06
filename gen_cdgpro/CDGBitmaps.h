#pragma once

bool CreateBitmaps();
void DestroyBitmaps();

extern HDC g_hScaledForegroundDCs[];
extern HBITMAP g_hScaledForegroundBitmaps[];
extern BYTE* g_pScaledForegroundBitmapBits[];

extern HDC g_hMaskDC;
extern HBITMAP g_hMaskBitmap;

extern HDC g_hBorderMaskDC;
extern HBITMAP g_hBorderMaskBitmap;
extern BYTE* g_pBorderMaskBitmapBits;

extern HDC g_hMaskedForegroundDC;
extern HBITMAP g_hMaskedForegroundBitmap;

extern HDC g_hBackgroundDC;
extern HBITMAP g_hBackgroundBitmap;
extern unsigned int* g_pBackgroundBitmapBits;

extern HDC g_hScrollBufferDC;
extern HBITMAP g_hScrollBufferBitmap;
extern BYTE* g_pScrollBufferBitmapBits;