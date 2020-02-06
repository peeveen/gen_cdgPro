#pragma once
#include "stdafx.h"

BYTE MemoryPreset(BYTE color, BYTE repeat);
void BorderPreset(BYTE color);
BYTE TileBlock(BYTE* pData, bool isXor, RECT **ppInvalidRect);
BYTE LoadColorTable(BYTE* pData, bool highTable);
BYTE Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy);
