#pragma once
#include "stdafx.h"

// Plugin version (don't touch this!)
#define GPPHDR_VER 0x10

// Plugin title
#define PLUGIN_NAME "CDG-Pro Plugin"

// main structure with plugin information, version, name...
typedef struct {
	int version;             // version of the plugin structure
	const char* description; // name/title of the plugin 
	int(*init)();            // function which will be executed on init event
	void(*config)();         // function which will be executed on config event
	void(*quit)();           // function which will be executed on quit event
	HWND hwndParent;         // hwnd of the Winamp client main window (stored by Winamp when dll is loaded)
	HINSTANCE hDllInstance;  // hinstance of this plugin DLL. (stored by Winamp when dll is loaded) 
} winampGeneralPurposePlugin;

// The official size of the CDG graphics area.
#define CDG_WIDTH (300)
#define CDG_HEIGHT (216)

// The CDG graphics area is divided into a 50x18 grid of 6x12 pixel cells.
// The outermost cells are part of the non-visible border area.
#define CDG_WIDTH_CELLS (50)
#define CDG_HEIGHT_CELLS (18)
#define CDG_CELL_WIDTH (6)
#define CDG_CELL_HEIGHT (12)
#define CDG_CANVAS_WIDTH_CELLS (48)
#define CDG_CANVAS_HEIGHT_CELLS (16)

// The outer edges of this area are not shown on screen, but are managed in memory (for scrolling
// images into the visible area, etc). This is the size of the visible central portion.
#define CDG_CANVAS_X (CDG_CELL_WIDTH)
#define CDG_CANVAS_Y (CDG_CELL_HEIGHT)
#define CDG_CANVAS_WIDTH (288)
#define CDG_CANVAS_HEIGHT (192)

// In Windows, bitmaps must have a width that is a multiple of 4 bytes.
// We are storing two bitmaps to represent the CDG display,
// one of which is 4 bits per pixel, and one which is 1 bit per pixel.
// Because of the 1bpp bitmap, this means that the bitmap width
// must be a multiple of 32 (32 bits = 4 bytes).
#define CDG_BITMAP_WIDTH (320)
#define CDG_BITMAP_HEIGHT CDG_HEIGHT

// For smoothing purposes, we will scale the graphics up and apply a smoothing algorithm.
#define SUPPORTED_SCALING_LEVELS (4) // 1x,2x,4x
#define MAXIMUM_SCALING_FACTOR ((1<<SUPPORTED_SCALING_LEVELS)>>1)
#define CDG_MAXIMUM_BITMAP_WIDTH (CDG_BITMAP_WIDTH*MAXIMUM_SCALING_FACTOR)
#define CDG_MAXIMUM_BITMAP_HEIGHT (CDG_BITMAP_HEIGHT*MAXIMUM_SCALING_FACTOR)

#define TOP_LEFT_PIXEL_OFFSET (((CDG_CELL_HEIGHT*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define TOP_RIGHT_PIXEL_OFFSET (TOP_LEFT_PIXEL_OFFSET+((CDG_CANVAS_WIDTH/2)-1))
#define BOTTOM_LEFT_PIXEL_OFFSET (((((CDG_CELL_HEIGHT+CDG_CANVAS_HEIGHT)-1)*CDG_BITMAP_WIDTH) + CDG_CELL_WIDTH) / 2)
#define BOTTOM_RIGHT_PIXEL_OFFSET (BOTTOM_LEFT_PIXEL_OFFSET+((CDG_CANVAS_WIDTH/2)-1))

// Each CDG frame is 1/300th of a second.
#define CDG_FRAME_DURATION_MS (1000.0/300.0)

// The CDG graphics files are often slightly ahead of the music, probably due to the techniques
// used when ripping the data, or possibly because the disc manufacturers assume a certain amount of
// buffering in the disc player audio hardware and no such delay exists when we play the data
// directly in software. Anyway, this is the amount of time that we will add to account for that.
#define HYSTERESIS_MS (100)
// Rewind tolerance. We periodically query WinAmp to find out the current position (in milliseconds)
// of the audio track. For some reason, usually very near the start of a track, WinAmp will report
// the position as very slightly earlier than the previous report indicated, which would normally
// make our plugin think that the user has rewound the track, and so we would go ahead and try to
// rebuild the CDG canvas from scratch. But if the amount that the track appears to have been
// rewound by is within this tolerance value, we will assume that this minor WinAmp glitch thing
// has happened, and will just not draw anything until the position advances.
#define REWIND_TOLERANCE_MS (100)
// Update screen approximately 30 FPS
#define SCREEN_REFRESH_MS (33)

// This is the colour we will use for transparency. It is impossible to
// represent this as a 12-bit colour, and each value is a different distance from
// a multiple of 17, so it should never accidentally happen.
#define DEFAULT_TRANSPARENT_COLOR_RED (145)
#define DEFAULT_TRANSPARENT_COLOR_GREEN (67)
#define DEFAULT_TRANSPARENT_COLOR_BLUE (219)
#define DEFAULT_TRANSPARENT_COLORREF (RGB(DEFAULT_TRANSPARENT_COLOR_RED,DEFAULT_TRANSPARENT_COLOR_GREEN,DEFAULT_TRANSPARENT_COLOR_BLUE))

// WinAmp messages.
#define WM_WA_IPC WM_USER
#define IPC_PLAYING_FILEW 13003
#define IPC_CB_MISC 603
#define IPC_ISPLAYING 104
#define IPC_GETOUTPUTTIME 105
#define IPC_CB_MISC_STATUS 2

// The CDG instruction set.
#define CDG_INSTR_MEMORY_PRESET (1)
#define CDG_INSTR_BORDER_PRESET (2)
#define CDG_INSTR_TILE_BLOCK (6)
#define CDG_INSTR_SCROLL_PRESET (20)
#define CDG_INSTR_SCROLL_COPY (24)
#define CDG_INSTR_TRANSPARENT_COLOR (28)
#define CDG_INSTR_LOAD_COLOR_TABLE_LOW (30)
#define CDG_INSTR_LOAD_COLOR_TABLE_HIGH (31)
#define CDG_INSTR_TILE_BLOCK_XOR (38)

// Right-click menu item IDs
#define MENUITEM_TOPMOST_ID (1)
#define MENUITEM_FULLSCREEN_ID (2)
#define MENUITEM_ABOUT_ID (3)

// The CDG data packet structure.
struct CDGPacket {
	BYTE command;
	BYTE instruction;
	BYTE unused1[2];
	BYTE data[16];
	BYTE unused2[4];
};

