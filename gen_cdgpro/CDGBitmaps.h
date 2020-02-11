#pragma once

#include <objidl.h>
#include <stdlib.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
#pragma comment (lib,"Gdiplus.lib")
using namespace Gdiplus;

bool CreateBitmaps();
void DestroyBitmaps();
void ClearForegroundBuffer();
void ResizeForegroundBackBufferBitmap();

extern HDC g_hScaledForegroundDCs[];
extern BYTE* g_pScaledForegroundBitmapBits[];

extern HDC g_hMaskDC;

extern HDC g_hBorderMaskDC;
extern HBITMAP g_hBorderMaskBitmap;
extern BYTE* g_pBorderMaskBitmapBits;

extern HDC g_hMaskedForegroundDC;

extern HDC g_hForegroundBackBufferDC;
extern HANDLE g_hForegroundBackBufferDCAccessMutex;
extern int g_nForegroundBackBufferWidth;
extern int g_nForegroundBackBufferHeight;

extern HDC g_hBackgroundDC;
extern unsigned int* g_pBackgroundBitmapBits;

extern HDC g_hScrollBufferDC;
extern BYTE* g_pScrollBufferBitmapBits;

extern HDC g_hLogoDC;
extern HBITMAP g_hLogoBitmap;

extern HDC g_hScreenDC;

extern HBRUSH g_hTransparentBrush;

extern SIZE g_logoSize;
extern Image *g_pLogoImage;