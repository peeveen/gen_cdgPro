#include "stdafx.h"
#include "zip.h"
#include <malloc.h>

// Current CDG data.
CDGPacket* g_pCDGData = NULL;
DWORD g_nCDGPackets = 0;

void clearExistingCDGData() {
	if (g_pCDGData) {
		free(g_pCDGData);
		g_pCDGData = NULL;
	}
	g_nCDGPackets = 0;
}

bool readCDGData(const WCHAR* pFileBeingPlayed) {
	clearExistingCDGData();
	bool result = false;
	WCHAR pathBuffer[MAX_PATH + 1];
	char zipEntryName[MAX_PATH + 1];
	zip_stat_t fileStat;

	wcscpy_s(pathBuffer, pFileBeingPlayed);
	pathBuffer[MAX_PATH] = '\0';
	size_t pathLength = wcslen(pathBuffer);
	_wcslwr_s(pathBuffer);
	const WCHAR* zipPrefixLocation = wcsstr(pathBuffer, L"zip://");
	bool isZipFile = !wcscmp(pathBuffer + (pathLength - 4), L".zip");
	if (zipPrefixLocation || isZipFile) {
		// Format of string might be zip://somepathtoazipfile,n
		// n will be the indexed entry in the zip file that is being played.
		// First of all, get rid of the zip:// bit.
		if (zipPrefixLocation) {
			wcscpy_s(pathBuffer, pathBuffer + 6);
			// Now we need to get rid of the ,n bit
			WCHAR* pComma = wcsrchr(pathBuffer, ',');
			if (pComma - pathBuffer)
				*pComma = '\0';
		}
		// OK, we now have the path to the zip file. We can go ahead and read it, looking for the
		// CDG file.
		FILE* pFile;
		errno_t fileError = _wfopen_s(&pFile, pathBuffer, L"rb");
		if (!fileError && pFile) {
			zip_source_t* pZipSource = zip_source_filep_create(pFile, 0, 0, NULL);
			if (pZipSource) {
				zip_t* pZip = zip_open_from_source(pZipSource, ZIP_RDONLY, NULL);
				if (pZip) {
					zip_int64_t nZipEntries = zip_get_num_entries(pZip, ZIP_FL_UNCHANGED);
					for (--nZipEntries; ~nZipEntries; --nZipEntries) {
						const char* pEntryName = zip_get_name(pZip, nZipEntries, ZIP_FL_ENC_GUESS);
						strcpy_s(zipEntryName, pEntryName);
						_strlwr_s(zipEntryName);
						size_t nameLength = strlen(zipEntryName);
						if (strstr(zipEntryName, ".cdg") == zipEntryName + (nameLength - 4)) {
							if (!zip_stat_index(pZip, nZipEntries, ZIP_FL_UNCHANGED, &fileStat)) {
								zip_file_t* pCDGFile = zip_fopen_index(pZip, nZipEntries, ZIP_FL_UNCHANGED);
								if (pCDGFile) {
									g_nCDGPackets = (DWORD)(fileStat.size / sizeof(CDGPacket));
									g_pCDGData = (CDGPacket*)malloc((size_t)g_nCDGPackets * sizeof(CDGPacket));
									zip_fread(pCDGFile, g_pCDGData, fileStat.size);
									zip_fclose(pCDGFile);
									result = true;
									break;
								}
							}
						}
					}
					zip_close(pZip);
				}
				zip_source_close(pZipSource);
			}
			fclose(pFile);
		}
	}
	else {
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
	}
	return result;
}
