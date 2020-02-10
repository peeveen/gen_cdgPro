#include "stdafx.h"
#include <malloc.h>
#include <stdio.h>

// Current CDG data.
CDGPacket* g_pCDGData = NULL;
DWORD g_nCDGPackets = 0;

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
