#pragma once
#include "stdafx.h"

BYTE MemoryPreset(BYTE color);
void BorderPreset(BYTE color);
BYTE TileBlock(BYTE* pData, bool isXor, RECT *pInvalidRect);
BYTE LoadColorTable(BYTE* pData, bool highTable);
BYTE Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy);
