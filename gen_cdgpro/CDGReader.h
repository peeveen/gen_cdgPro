#pragma once
#include "stdafx.h"
#include "CDGDefs.h"

// Current CDG data.
extern CDGPacket* g_pCDGData;
extern DWORD g_nCDGPackets;

bool ReadCDGData(const WCHAR* pFileBeingPlayed);
void ClearExistingCDGData();