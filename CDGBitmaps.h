#pragma once

#include <objidl.h>
#include <stdlib.h>
#include <gdiplus.h>
#include <gdipluscolor.h>
using namespace Gdiplus;

// Functions relating to the initialization/cleanup of bitmaps and their device contexts.

/// <summary>
/// Creates the bitmaps in memory that are necessary for our CDG processing.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateBitmaps();

/// <summary>
/// Cleans up all allocated bitmaps.
/// </summary>
void DestroyBitmaps();

/// <summary>
/// Clears the foreground buffer bitmap (sets all pixels to the current background
/// colour).
/// </summary>
void ClearForegroundBuffer();

/// <summary>
/// Resizes bitmaps as a result of manual window resizing.
/// </summary>
void ResizeForegroundBackBufferBitmap();

/// <summary>
/// There are multiple foreground device contexts, one per scaling level.
/// </summary>
extern HDC g_hScaledForegroundDCs[];
/// <summary>
/// There are multiple foreground bitmaps, one per scaling level.
/// </summary>
extern BYTE* g_pScaledForegroundBitmapBits[];

/// <summary>
/// To enable transparency of the background colour, we generate a monochrome mask,
/// which is the inverse of any foreground graphics. This device context will contain
/// that monochrome bitmap. See the RenderForegroundBackBuffer() function (in
/// CDGRender.cpp) for how this is done.
/// </summary>
extern HDC g_hMaskDC;

/// <summary>
/// So that we can optionally create a border around the foreground graphics (to aid visibility)
/// we need to generate a mask that is "thicker" than the standard mask. We will generate that
/// in this device context and bitmap. See the RenderForegroundBackBuffer() function (in
/// CDGRender.cpp) for how this is done.
/// </summary>
extern HDC g_hBorderMaskDC;
extern HBITMAP g_hBorderMaskBitmap;
extern BYTE* g_pBorderMaskBitmapBits;

extern HDC g_hMaskedForegroundDC;
extern HANDLE g_hMaskedForegroundDCAccessMutex;

/// <summary>
/// This device context and bitmap is where we draw the foreground graphics, ready to
/// "blit" to the main window DC.
/// </summary>
extern HDC g_hForegroundBackBufferDC;
extern HANDLE g_hForegroundBackBufferDCAccessMutex;
extern int g_nForegroundBackBufferWidth;
extern int g_nForegroundBackBufferHeight;

/// <summary>
/// This is the device context for the background graphics. Usually just a single coloured pixel
/// that will be stretched to the full window.
/// </summary>
extern HDC g_hBackgroundDC;
extern unsigned int* g_pBackgroundBitmapBits;

/// <summary>
/// Special "temporary" DC for scroll operations. We copy the current foreground DC to this
/// DC, then back again, but offset by the scroll amount.
/// </summary>
extern HDC g_hScrollBufferDC;
extern BYTE* g_pScrollBufferBitmapBits;

/// <summary>
/// Device context containing the logo bitmap (if specified by the user).
/// </summary>
extern HDC g_hLogoDC;
extern HBITMAP g_hLogoBitmap;

/// <summary>
/// The screen device context.
/// </summary>
extern HDC g_hScreenDC;

/// <summary>
/// Our standard transparent brush. A strange colour that is impossible for the CDG palette
/// to define, so guaranteed not to get mixed in with the normal CDG palette.
/// </summary>
extern HBRUSH g_hTransparentBrush;

/// <summary>
/// The size of the logo image (if specified by the user).
/// </summary>
extern SIZE g_logoSize;

/// <summary>
/// The logo image (if specified by the user).
/// </summary>
extern Image *g_pLogoImage;