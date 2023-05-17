#pragma once
#include "stdafx.h"

// Functions relating to the creation/cleanup of windows.

/// <summary>
/// Set the window to full-screen mode.
/// </summary>
/// <param name="fullScreen">True for full-screen mode, false otherwise.</param>
void SetFullScreen(bool fullScreen);

/// <summary>
/// Build all the windows that we need.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateWindows();

/// <summary>
/// Cleanup all the windows that we created.
/// </summary>
void DestroyWindows();

/// <summary>
/// Are we currently in full-screen mode?
/// </summary>
/// <returns>True if we are in fullscreen mode.</returns>
bool IsFullScreen();

/// <summary>
/// Show the windows that should be shown.
/// If a song is playing, then show all windows EXCEPT the logo window.
/// If a song is NOT playing, then:
///	  If there is a logo, show all windows.
///	  If there is NOT a logo, hide all windows.
/// </summary>
/// <param name="bSongPlaying">True if a song is playing.</param>
/// <param name="updateLogo">True to update the logo.</param>
void ShowWindows(bool bSongPlaying, bool updateLogo = false);

/// <summary>
/// Convert the given CDG coordinates to onscreen window coordinates.
/// </summary>
/// <param name="pRect"></param>
void CDGRectToClientRect(RECT* pRect);

/// <summary>
/// The foreground CDG graphics window.
/// </summary>
extern HDC g_hForegroundWindowDC;
extern HWND g_hForegroundWindow;

/// <summary>
/// The background CDG graphics window.
/// </summary>
extern HDC g_hBackgroundWindowDC;
extern HWND g_hBackgroundWindow;

/// <summary>
/// The logo window.
/// </summary>
extern HDC g_hLogoWindowDC;
extern HWND g_hLogoWindow;
