#include "stdafx.h"
#include "CDGDefs.h"
#include "CDGGlobals.h"
#include "CDGPrefs.h"
#include "CDGReader.h"
#include "CDGWindows.h"
#include "CDGInstructionHandlers.h"

// Handle and ID of the processing thread.
HANDLE g_hCDGProcessingThread = NULL;
DWORD g_nCDGProcessingThreadID = 0;
// Cross thread communication events.
HANDLE g_hStopCDGProcessingEvent = NULL;
HANDLE g_hStoppedCDGProcessingEvent = NULL;
HANDLE g_hStopCDGThreadEvent = NULL;
HANDLE g_hSongLoadedEvent = NULL;

// Current CDG instruction index.
int g_nCDGPC = 0;

BYTE ProcessCDGPackets(DWORD songPosition) {
	BYTE result = 0;
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent };
	// Get current song position in milliseconds (see comment about rewind tolerance).
	if (songPosition != -1) {
		// Account for WinAmp timing drift bug (see comment about time scaler)
		// and general lag (see comment about hysteresis).
		songPosition = (int)(songPosition * g_nTimeScaler) + HYSTERESIS_MS;
		int cdgFrameIndex = (int)(songPosition / CDG_FRAME_DURATION_MS);
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
						result |= MemoryPreset(pCDGPacket->data[0] & 0x0F, pCDGPacket->data[1] & 0x0F);
						break;
					case CDG_INSTR_BORDER_PRESET:
						BorderPreset(pCDGPacket->data[0] & 0x0F);
						break;
					case CDG_INSTR_TILE_BLOCK:
					case CDG_INSTR_TILE_BLOCK_XOR:
						result |= TileBlock(pCDGPacket->data, instr == CDG_INSTR_TILE_BLOCK_XOR);
						break;
					case CDG_INSTR_SCROLL_COPY:
					case CDG_INSTR_SCROLL_PRESET:
						result |= Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x0F, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x0F, instr == CDG_INSTR_SCROLL_COPY);
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

DWORD WINAPI CDGProcessor(LPVOID pParams) {
	HANDLE waitHandles[] = { g_hStopCDGProcessingEvent, g_hStopCDGThreadEvent,g_hSongLoadedEvent };
	for (;;) {
		int waitResult = ::WaitForMultipleObjects(2, waitHandles + 1, FALSE, INFINITE);
		if (waitResult == 0) {
			break;
		}
		g_nCDGPC = 0;
		for (;;) {
			waitResult = ::WaitForMultipleObjects(2, waitHandles, FALSE, SCREEN_REFRESH_MS);
			if (waitResult == 0)
				break;
			if (waitResult == 1)
				return 0;
			if (waitResult == WAIT_TIMEOUT) {
				if (g_nCDGPC < g_nCDGPackets) {
					byte result = ProcessCDGPackets(::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME));
					if (result & 0x01)
						::RedrawWindow(g_hForegroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
					if (result & 0x02)
						::RedrawWindow(g_hBackgroundWindow, NULL, NULL, RDW_INVALIDATE | RDW_UPDATENOW);
				}
			}
		}
		::SetEvent(g_hStoppedCDGProcessingEvent);
	}
	return 0;
}

bool StartCDGProcessor() {
	g_hStopCDGProcessingEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hStoppedCDGProcessingEvent = ::CreateEvent(NULL, FALSE, TRUE, NULL);
	g_hStopCDGThreadEvent = ::CreateEvent(NULL, TRUE, FALSE, NULL);
	g_hSongLoadedEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
	g_hCDGProcessingThread = ::CreateThread(NULL, 0, CDGProcessor, NULL, 0, &g_nCDGProcessingThreadID);
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