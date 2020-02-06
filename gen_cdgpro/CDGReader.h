#pragma once
#include "stdafx.h"
#include "CDGDefs.h"

// Current CDG data.
extern CDGPacket* g_pCDGData;
extern int g_nCDGPackets;

void readCDGData(const WCHAR* pFileBeingPlayed);
void clearExistingCDGData();