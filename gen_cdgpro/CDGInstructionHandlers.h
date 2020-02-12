#pragma once
#include "stdafx.h"

BYTE MemoryPreset(BYTE color, RECT* pInvalidRect);
BYTE BorderPreset(BYTE color, RECT* pInvalidRect);
BYTE TileBlock(BYTE* pData, bool isXor, RECT *pInvalidRect);
BYTE LoadColorTable(BYTE* pData, bool highTable, RECT* pInvalidRect);
BYTE Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy,RECT *pInvalidRect);

extern unsigned short g_nChannelMask;
