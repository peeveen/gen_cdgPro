#include "stdafx.h"
#include <stdio.h>
#include "CDGPrefs.h"
#include "CDGGlobals.h"

#define PREF_BUFFER_SIZE (MAX_PATH*2)

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
WCHAR g_szLogoPath[PREF_BUFFER_SIZE]={'\0'};

void TrimLeading(WCHAR* pszString) {
	WCHAR* pszPointer = pszString;
	while (*pszPointer == ' ' || *pszPointer == '\t')
		++pszPointer;
	wcscpy_s(pszString, PREF_BUFFER_SIZE, pszPointer);
}

void SetPref(WCHAR* pszPrefLine, const WCHAR* pszPrefName, void* pDestVal, void (*pFunc)(const WCHAR *,void *)) {
	int len = wcslen(pszPrefName);
	if (!wcsncmp(pszPrefLine, pszPrefName, len)) {
		wcscpy_s(pszPrefLine, PREF_BUFFER_SIZE,pszPrefLine + len);
		TrimLeading(pszPrefLine);
		if (pszPrefLine[0] == '=') {
			++pszPrefLine;
			TrimLeading(pszPrefLine);
			pFunc(pszPrefLine, pDestVal);
		}
	}
}

void SetBoolValue(const WCHAR* pszPrefValue, void* pbDestVal) {
	*(bool*)pbDestVal = wcslen(pszPrefValue) && (pszPrefValue[0] == 't' || pszPrefValue[0] == 'y');
}

void SetStringValue(const WCHAR* pszPrefValue, void* pszDestVal) {
	wcscpy_s((WCHAR *)pszDestVal, PREF_BUFFER_SIZE,pszPrefValue);
}

void SetIntValueRadix(const WCHAR *pszPrefValue, int* pnDestVal, int radix) {
	*(int*)pnDestVal = wcstol(pszPrefValue, NULL, radix);
}

void SetIntValue(const WCHAR* pszPrefValue, void* pnDestVal) {
	SetIntValueRadix(pszPrefValue, (int*)pnDestVal,10);
}

void SetHexIntValue(const WCHAR* pszPrefValue, void* pnDestVal) {
	SetIntValueRadix(pszPrefValue, (int*)pnDestVal,16);
}

void SetInt(WCHAR* pszPrefLine, const WCHAR* pszPrefName, int* pnDestVal, int nMin, int nMax, bool hex = false) {
	SetPref(pszPrefLine, pszPrefName, pnDestVal, hex ? SetHexIntValue : SetIntValue);
	if (*pnDestVal < nMin)
		*pnDestVal = nMin;
	else if (*pnDestVal > nMax)
		*pnDestVal = nMax;
}

void SetBool(WCHAR* pszPrefLine, const WCHAR* pszPrefName, bool* pbDestVal) {
	SetPref(pszPrefLine, pszPrefName, pbDestVal, SetBoolValue);
}

void SetString(WCHAR* pszPrefLine, const WCHAR* pszPrefName, WCHAR *pszDestVal) {
	SetPref(pszPrefLine, pszPrefName, pszDestVal, SetStringValue);
}

void SetOpacity(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"opacity", &g_nBackgroundOpacity, 0, 255);
}

void SetBackgroundColor(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"backgroundcolor", &g_nDefaultBackgroundColor, 0, 0x00ffffff,true);
}

void SetBackgroundDetectionMode(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"backgrounddetectionmode", &g_nBackgroundDetectionMode, 0, 4);
}

void SetSmoothingPasses(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"smoothingpasses", &g_nSmoothingPasses, 0, SUPPORTED_SCALING_LEVELS - 1);
}

void SetMargin(WCHAR* pszPrefLine) {
	SetInt(pszPrefLine, L"margin", &g_nMargin, 0, 50);
}

void SetOutline(WCHAR* pszPrefLine) {
	SetBool(pszPrefLine, L"outline", &g_bDrawOutline);
}

void SetLogoPath(WCHAR* pszPrefLine) {
	SetString(pszPrefLine, L"logopath", g_szLogoPath);
}

void ReadPrefs() {
	WCHAR szBuffer[PREF_BUFFER_SIZE];
	if (GetModuleFileName(g_hInstance, szBuffer, MAX_PATH)) {
		WCHAR* pszLastSlash = wcsrchr(szBuffer, '\\');
		if (pszLastSlash) {
			*(pszLastSlash + 1) = '\0';
			wcscat_s(szBuffer, L"gen_cdgPro.ini");
			FILE* pFile = NULL;
			errno_t error=_wfopen_s(&pFile,szBuffer, L"rt");
			if (pFile && !error) {
				while (fgetws(szBuffer, PREF_BUFFER_SIZE, pFile)) {
					_wcslwr_s(szBuffer);
					TrimLeading(szBuffer);
					SetOpacity(szBuffer);
					SetBackgroundColor(szBuffer);
					SetBackgroundDetectionMode(szBuffer);
					SetSmoothingPasses(szBuffer);
					SetMargin(szBuffer);
					SetOutline(szBuffer);
					SetLogoPath(szBuffer);
				}
				fclose(pFile);
			}
		}
	}
}