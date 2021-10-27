#pragma once
#include "stdafx.h"

// Handler functions for the non-trivial CDG instructions.

/// <summary>
/// Basically a "clear screen" command.
/// </summary>
/// <param name="color">Color to fill the screen with.</param>
/// <param name="pInvalidRect">Pointer to a rectangle that will be filled with
/// client area to invalidate.</param>
/// <returns>Byte indicating what needs refreshed.</returns>
CDG_REFRESH_FLAGS MemoryPreset(BYTE color, RECT* pInvalidRect);

/// <summary>
/// Basically a "set border colour" command.
/// </summary>
/// <param name="color">Color to fill the border with.</param>
/// <param name="pInvalidRect">Pointer to a rectangle that will be filled with
/// client area to invalidate.</param>
/// <returns>Byte indicating what needs refreshed.</returns>
CDG_REFRESH_FLAGS BorderPreset(BYTE color, RECT* pInvalidRect);

/// <summary>
/// Draws a block of pixels into the CDG buffer.
/// </summary>
/// <param name="pData">The pixels to draw.</param>
/// <param name="isXor">True if the draw operation is XOR, false for standard copy.</param>
/// <param name="pInvalidRect">Pointer to a rectangle that will be filled with
/// client area to invalidate.</param>
/// <returns>Byte indicating what needs refreshed.</returns>
CDG_REFRESH_FLAGS TileBlock(BYTE* pData, bool isXor, RECT *pInvalidRect);

/// <summary>
/// Sets the palette.
/// </summary>
/// <param name="pData">New palette data.</param>
/// <param name="highTable">True if we are setting the high table, false for the low table.</param>
/// <param name="pInvalidRect">Pointer to a rectangle that will be filled with
/// client area to invalidate.</param>
/// <returns>Byte indicating what needs refreshed.</returns>
CDG_REFRESH_FLAGS LoadColorTable(BYTE* pData, bool highTable, RECT* pInvalidRect);

/// <summary>
/// Scrolls a section of the CDG buffer in a given direction.
/// </summary>
/// <param name="color">Color to replace scrolled pixels with (if copy==false).</param>
/// <param name="hScroll">Horizontal block scrolling. 2=one block to the right, 1=one block to the left, 0=no block scroll.</param>
/// <param name="hScrollOffset">Number of pixels to scroll horizontally, for fine tuning (can be negative).</param>
/// <param name="vScroll">Vertical block scrolling. 2=one block up, 1=one block down, 0=no block scroll.</param>
/// <param name="vScrollOffset">Number of pixels to scroll vertically, for fine tuning (can be negative).</param>
/// <param name="copy">True if the "lost" scrolled content should be copied into the now-empty space (wraparound).</param>
/// <param name="pInvalidRect">Pointer to a rectangle that will be filled with
/// client area to invalidate.</param>
/// <returns>Byte indicating what needs refreshed.</returns>
CDG_REFRESH_FLAGS Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy,RECT *pInvalidRect);

/// <summary>
/// Channel mask. By default, channels 0 and 4 are shown.
/// </summary>
extern unsigned short g_nChannelMask;
