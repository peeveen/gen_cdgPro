#include "stdafx.h"
#include <stdio.h>
#include "CDGPrefs.h"
#include "CDGGlobals.h"

// Preferences (TODO: INI file or UI).
int g_nBackgroundOpacity = 192;
bool g_bDrawOutline = true;
bool g_bShowBorder = false;
// We periodically ask WinAmp how many milliseconds it has played of a song. This works fine
// but as time goes on, it starts to get it wrong, falling behind by a tiny amount each time.
// To keep the display in sync, we will multiply whatever WinAmp tells us by this amount.
double g_nTimeScaler = 1.00466;
// How to determine the transparent background color?
int g_nBackgroundDetectionMode = BDM_TOPRIGHTPIXEL;
// Default background colour when there is no song playing.
int g_nDefaultBackgroundColor = 0x0055ff;
// Scale2x/4x smoothing?
int g_nSmoothingPasses = 2;
// Size of margin.
int g_nMargin = 15;
// Logo to display when there is no song playing.
WCHAR g_szLogoPath[MAX_PATH + 1]={'\0'};

void TrimLeading(WCHAR* pszString, int nBufferSize) {
	WCHAR* pszPointer = pszString;
	while (*pszPointer == ' ' || *pszPointer == '\t')
		++pszPointer;
	wcscpy_s(pszString, nBufferSize, pszPointer);
}

void ParseInt(WCHAR* pszBuffer, int nBufferSize,int* pnDest, int nValidMinimum,int nValidMaximum,bool hex = false) {
	TrimLeading(pszBuffer, nBufferSize);
	if (pszBuffer[0] == '=') {
		++pszBuffer;
		--nBufferSize;
		TrimLeading(pszBuffer, nBufferSize);
		int parsedValue = 0;
		if (hex)
			parsedValue = wcstol(pszBuffer, NULL, 16);
		else
			parsedValue= _wtol(pszBuffer);
		if (parsedValue < nValidMinimum)
			parsedValue = nValidMinimum;
		else if (parsedValue > nValidMaximum)
			parsedValue = nValidMaximum;
		*pnDest = parsedValue;
	}
}

void ParseBool(WCHAR* pszBuffer, int nBufferSize, bool* pbDest) {
	TrimLeading(pszBuffer, nBufferSize);
	if (pszBuffer[0] == '=') {
		++pszBuffer;
		--nBufferSize;
		TrimLeading(pszBuffer, nBufferSize);
		bool parsedValue = pszBuffer[0] == 'T' || pszBuffer[0] == 't' || pszBuffer[0] == 'Y' || pszBuffer[0] == 'y';
		*pbDest = parsedValue;
	}
}

void ParseString(WCHAR* pszBuffer, int nBufferSize, WCHAR *pszDest,int nDestBufferSize) {
	TrimLeading(pszBuffer, nBufferSize);
	if (pszBuffer[0] == '=') {
		++pszBuffer;
		--nBufferSize;
		wcscpy_s(pszDest, nDestBufferSize,pszBuffer);
	}
}

void ReadPrefs() {
	WCHAR szBuffer[MAX_PATH*2];
	if (GetModuleFileName(g_hInstance, szBuffer, MAX_PATH)) {
		WCHAR* pszLastSlash = wcsrchr(szBuffer, '\\');
		if (pszLastSlash) {
			*(pszLastSlash + 1) = '\0';
			wcscat_s(szBuffer, L"gen_cdgPro.ini");
			FILE* pFile = NULL;
			errno_t error=_wfopen_s(&pFile,szBuffer, L"rt");
			if (pFile && !error) {
				while (fgetws(szBuffer, MAX_PATH * 2, pFile)) {
					TrimLeading(szBuffer, MAX_PATH * 2);
					if (!wcsncmp(szBuffer, L"Opacity", 7))
						ParseInt(szBuffer + 7, (MAX_PATH * 2) - 7, &g_nBackgroundOpacity, 0, 255);
					else if (!wcsncmp(szBuffer, L"BackgroundColor", 15))
						ParseInt(szBuffer + 15, (MAX_PATH * 2) - 15, &g_nDefaultBackgroundColor,0,0xFFFFFF,true);
					else if (!wcsncmp(szBuffer, L"BackgroundDetectionMode", 23))
						ParseInt(szBuffer + 23, (MAX_PATH * 2) - 23, &g_nBackgroundDetectionMode, BDM_PALETTEINDEXZERO, BDM_BOTTOMRIGHTPIXEL );
					else if (!wcsncmp(szBuffer, L"SmoothingPasses", 15))
						ParseInt(szBuffer + 15, (MAX_PATH * 2) - 15, &g_nSmoothingPasses, 0, SUPPORTED_SCALING_LEVELS - 1);
					else if (!wcsncmp(szBuffer, L"Margin", 6))
						ParseInt(szBuffer + 6, (MAX_PATH * 2) - 6, &g_nMargin,0,50);
					else if (!wcsncmp(szBuffer, L"Outline", 7))
						ParseBool(szBuffer + 7, (MAX_PATH * 2) - 7, &g_bDrawOutline);
					else if (!wcsncmp(szBuffer, L"LogoPath", 8))
						ParseString(szBuffer + 8, (MAX_PATH * 2) - 8, g_szLogoPath,MAX_PATH);
				}
				fclose(pFile);
			}
		}
	}
}