#include "stdafx.h"
#include <stdio.h>
#include "CDGGlobals.h"
#include "CDGPrefs.h"
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
// Current CDG data.
CDGPacket* g_pCDGData = NULL;
DWORD g_nCDGPackets = 0;
// Current CDG instruction index.
DWORD g_nCDGPC = 0;

void ClearExistingCDGData() {
	if (g_pCDGData) {
		free(g_pCDGData);
		g_pCDGData = NULL;
	}
	g_nCDGPackets = 0;
}

bool ReadCDGData(const WCHAR* pFileBeingPlayed) {
	ClearExistingCDGData();
	bool result = false;
	WCHAR pathBuffer[MAX_PATH + 1];

	wcscpy_s(pathBuffer, pFileBeingPlayed);
	pathBuffer[MAX_PATH] = '\0';
	size_t pathLength = wcslen(pathBuffer);
	_wcslwr_s(pathBuffer);
	// This is a plain filesystem path. We want to replace the extension with cdg, or
	// add cdg if there is no extension.
	WCHAR* pDot = wcsrchr(pathBuffer, '.');
	WCHAR* pSlash = wcsrchr(pathBuffer, '\\');
	if (pDot > pSlash)
		*pDot = '\0';
	wcscat_s(pathBuffer, L".cdg");
	FILE* pFile = NULL;
	errno_t error = _wfopen_s(&pFile, pathBuffer, L"rb");
	if (!error && pFile) {
		fseek(pFile, 0, SEEK_END);
		int size = ftell(pFile);
		g_nCDGPackets = size / sizeof(CDGPacket);
		fseek(pFile, 0, SEEK_SET);
		g_pCDGData = (CDGPacket*)malloc(g_nCDGPackets * sizeof(CDGPacket));
		if (g_pCDGData) {
			fread(g_pCDGData, sizeof(CDGPacket), g_nCDGPackets, pFile);
			result = true;
		}
		fclose(pFile);
	}
	return result;
}

BYTE ProcessCDGPackets(long songPosition,RECT *pRedrawRect,RECT *pInvalidRect) {
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
						result |= MemoryPreset(pCDGPacket->data[0] & 0x0F, pInvalidRect);
						break;
					case CDG_INSTR_BORDER_PRESET:
						result|=BorderPreset(pCDGPacket->data[0] & 0x0F, pInvalidRect);
						break;
					case CDG_INSTR_TILE_BLOCK:
					case CDG_INSTR_TILE_BLOCK_XOR:
						result |= TileBlock(pCDGPacket->data, instr == CDG_INSTR_TILE_BLOCK_XOR,pRedrawRect);
						break;
					case CDG_INSTR_SCROLL_COPY:
					case CDG_INSTR_SCROLL_PRESET:
						result |= Scroll(pCDGPacket->data[0] & 0x0F, (pCDGPacket->data[1] >> 4) & 0x03, pCDGPacket->data[1] & 0x0F, (pCDGPacket->data[2] >> 4) & 0x03, pCDGPacket->data[2] & 0x0F, instr == CDG_INSTR_SCROLL_COPY,pRedrawRect);
						break;
					case CDG_INSTR_TRANSPARENT_COLOR:
						// Not implemented.
						break;
					case CDG_INSTR_LOAD_COLOR_TABLE_LOW:
					case CDG_INSTR_LOAD_COLOR_TABLE_HIGH:
						result |= LoadColorTable(pCDGPacket->data, instr == CDG_INSTR_LOAD_COLOR_TABLE_HIGH, pInvalidRect);
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
	static RECT invalidRect,redrawRect;
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
				::ZeroMemory(&redrawRect, sizeof(RECT));
				if (g_nCDGPC < g_nCDGPackets) {
					byte result = ProcessCDGPackets(::SendMessage(g_hWinampWindow, WM_WA_IPC, 0, IPC_GETOUTPUTTIME),&redrawRect,&invalidRect);
					// Each call to ProcessCDGPackets will return a byte, which is an accumulation of the results from the
					// CDG instructions that were processed.
					// If the 1 bit is set, then the area defined by redrawRect needs redrawn and repainted.
					// If the 2 bit is set, then the entire background needs repainted.
					// If the 4 bit is set, then the area defined by invalidRect needs repainted.
					if (result & 0x01) {
						DrawForeground(&redrawRect);
						if (invalidRect.right > 0) {
							::UnionRect(&invalidRect, &invalidRect, &redrawRect);
						}
						else
							memcpy(&invalidRect, &redrawRect, sizeof(RECT));
					}
					if (result & 0x05) {
						RenderForegroundBackBuffer(&invalidRect);
						CDGRectToClientRect(&invalidRect);
						::WaitForSingleObject(g_hPaintMutex,INFINITE);
						::InvalidateRect(g_hForegroundWindow, &invalidRect,FALSE);
						::ReleaseMutex(g_hPaintMutex);
					}
					if (result & 0x02) {
						::WaitForSingleObject(g_hPaintMutex, INFINITE);
						::InvalidateRect(g_hBackgroundWindow, NULL, FALSE);
						::ReleaseMutex(g_hPaintMutex);
					}
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
	ClearExistingCDGData();
}