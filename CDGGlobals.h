#pragma once
#include "stdafx.h"
#include <objidl.h>
#include <gdiplus.h>
using namespace Gdiplus;

/// <summary>
/// Instance of this module.
/// </summary>
extern HINSTANCE g_hInstance;

/// <summary>
/// The Winamp window handle.
/// </summary>
extern HWND g_hWinampWindow;

/// <summary>
/// Mutex that prevents multiple threads painting at the same time.
/// </summary>
extern HANDLE g_hPaintMutex;