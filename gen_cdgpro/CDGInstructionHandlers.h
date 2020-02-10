#pragma once
#include "stdafx.h"

BYTE MemoryPreset(BYTE color, RECT* pRepaintRect);
BYTE BorderPreset(BYTE color);
BYTE TileBlock(BYTE* pData, bool isXor, RECT *pRedrawRect, RECT *pRepaintRect);
BYTE LoadColorTable(BYTE* pData, bool highTable, RECT* pRepaintRect);
BYTE Scroll(BYTE color, BYTE hScroll, BYTE hScrollOffset, BYTE vScroll, BYTE vScrollOffset, bool copy,RECT *pRedrawRect,RECT * pRepaintRect);
