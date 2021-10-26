#pragma once
#include "stdafx.h"

// Functions relating to the right-click menu.

void ShowMenu(int xPos, int yPos);
bool CreateRightClickMenu();
void DestroyRightClickMenu();
void SetMenuItemCheckmark(UINT nMenuItemID, bool set);