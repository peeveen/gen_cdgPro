#include "stdafx.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGReader.h"
#include "CDGRender.h"
#include "CDGWindows.h"
#include "CDGBitmaps.h"
#include "CDGPalette.h"
#include "CDGInstructionHandlers.h"
#include "CDGBackgroundFunctions.h"

// Handle of the processing thread.
HANDLE g_hCDGProcessingThread = NULL;
// Cross thread communication events.
HANDLE g_hStopCDGProcessingEvent = NULL;
HANDLE g_hStoppedCDGProcessingEvent = NULL;
HANDLE g_hStopCDGThreadEvent = NULL;
HANDLE g_hSongLoadedEvent = NULL;
// Current CDG instruction index.
DWORD g_nCDGPC = 0;

BYTE ProcessCDGPackets(long songPosition,RECT *pInvalidRect) {
	BYTE result = 0;
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent };
	// Get current song position in milliseconds (see comment about rewind tolerance).
	if (songPosition>=0) {
		// Account for WinAmp timing drift bug (see comment about time scaler)
		// and general lag (see comment about hysteresis).
		songPosition = (int)(songPosition * g_nTimeScaler) + HYSTERESIS_MS;
		DWORD cdgFrameIndex = (DWORD)(songPosition / CDG_FRAME_DURATION_MS);
		if (cdgFrameIndex > g_nCDGPackets)
			cdgFrameIndex = g_nCDGPackets;
		// If the target frame is BEFORE the current CDGPC, the user has
		// (possibly) rewound the song.
		// Due to the XORing nature of CDG graphics, we have to start from the start
		// and reconstruct the screen up until this point!
		int difference = cdgFrameIndex - g_nCDGPC;
		if (difference < -REWIND_TOLERANCE_MS)
			g_nCDGPC = 0;
		for (; g_nCDGPC < cdgFrameIndex;) {
			int waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, 0);
			if (waitResult == WAIT_TIMEOUT) {
				CDGPacket* pCDGPacket = g_pCDGData + g_nCDGPC;
				if ((pCDGPacket->command & 0x3F) == 0x09) {
					BYTE instr = pCDGPacket->instruction & 0x3F;
					switch (instr) {
					case CDG_INSTR_MEMORY_PRESET:
						result |= MemoryPreset(pCDGPacket->data[0] & 0x0F);
						break;
					case CDG_INSTR_BORDER_PRESET:
						result|=BorderPreset(pCDGPacket->data[0] & 0x0F);
						break;
					case CDG_INSTR_TILE_BLOCK:
					case CDG_INSTR_TILE_BLOCK_XOR:
						result |= TileBlock(pCDGPacket->data, instr == CDG_INSTR_TILE_BLOCK_XOR,pInvalidRect);
						break;
					case CDG_INSTR_SCROLL_COPY:
					case CDG_INSTR_SCROLL_PRESET:
						result |= Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x0F, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x0F, instr == CDG_INSTR_SCROLL_COPY,pInvalidRect);
						break;
					case CDG_INSTR_TRANSPARENT_COLOR:
						// Not implemented.
						break;
					case CDG_INSTR_LOAD_COLOR_TABLE_LOW:
					case CDG_INSTR_LOAD_COLOR_TABLE_HIGH:
						result |= LoadColorTable(pCDGPacket->data, instr == CDG_INSTR_LOAD_COLOR_TABLE_HIGH);
						break;
					default:
						break;
					}
				}
				g_nCDGPC++;
			}
			else
				break;
		}
	}
	return result;
}

void ResetProcessor() {
	g_nCDGPC = 0;
	ResetPalette();
	ClearForegroundBuffer();
	SetBackgroundColorIndex(0);
}

DWORD WINAPI CDGProcessor(LPVOID pParams) {
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent,g_hSongLoadedEvent };
	static RECT invalidRect;
	for (;;) {
		ResetProcessor();
		int waitResult = ::WaitForMultipleObjects(2, waitHandles + 1, FALSE, INFINITE);
		if (waitResult == 0) {
			break;
		}
		for (;;) {
			waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, SCREEN_REFRESH_MS);
			if (waitResult == WAIT_TIMEOUT) {
				::ZeroMemory(&invalidRect, sizeof(RECT));
				if (g_nCDGPC < g_nCDGPackets) {
					byte result = ProcessCDGPackets(::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME),&invalidRect);
					// Each call to ProcessCDGPackets will return a byte, which is an accumulation of the results from the
					// CDG instructions that were processed.
					// If the 1 bit is set, then the foreground needs redrawn (and pInvalidRect will be set to an area that needs rendered).
					// If the 2 bit is set, then the entire background needs repainted.
					// If the 4 bit is set, then the entire foreground needs repainted.
					if (result & 0x01)
						RedrawForeground(&invalidRect);
					if (result & 0x04)
						RefreshScreen(NULL);
					// Note that it is absolutely impossible to only invalidate the redrawn area of the window.
					// We are using StretchBlt to copy the memory bitmap to the screen. This performs a certain
					// amount of "between pixel" logic. For example, if we stretch a 20x20 bitmap to 30x30, it
					// will render 10 extra "halfway house" pixels in each direction. We cannot tell it to START
					// rendering on one of these pixels! If we attempt to invalidate only the redrawn area,
					// the pixels start shifting around in a noticably greasy fashion.
					if (result & 0x05)
						::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
					if (result & 0x02)
						::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				}
			}
			else
				break;
		}
		::SetEvent(g_hStoppedCDGProcessingEvent);
	}
	return 0;
}

bool StartCDGProcessor() {
	ResetProcessor();
	g_hStopCDGProcessingEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hStoppedCDGProcessingEvent = ::CreateEvent(NULL, FALSE, TRUE, NULL);
	g_hStopCDGThreadEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hSongLoadedEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hCDGProcessingThread = ::CreateThread(NULL, 0, CDGProcessor, NULL, 0, NULL);
	return !!g_hCDGProcessingThread;
}

void StopCDGProcessor() {
	::SetEvent(g_hStopCDGProcessingEvent);
	::SetEvent(g_hStopCDGThreadEvent);
	::WaitForSingleObject(g_hCDGProcessingThread, INFINITE);
	::CloseHandle(g_hStopCDGProcessingEvent);
	::CloseHandle(g_hStoppedCDGProcessingEvent);
	::CloseHandle(g_hStopCDGThreadEvent);
	::CloseHandle(g_hSongLoadedEvent);
}