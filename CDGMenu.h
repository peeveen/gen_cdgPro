#pragma once
#include "stdafx.h"

// Functions relating to the right-click menu.

/// <summary>
/// Show the menu at the given coordinates.
/// </summary>
/// <param name="xPos">X coordinate.</param>
/// <param name="yPos">Y coordinate.</param>
void ShowMenu(int xPos, int yPos);

/// <summary>
/// Create the menu from the resource file.
/// </summary>
/// <returns>True if successful.</returns>
bool CreateRightClickMenu();

/// <summary>
/// Cleanup the menu objects.
/// </summary>
void DestroyRightClickMenu();

/// <summary>
/// Apply a checkmark to a menu item.
/// </summary>
void SetMenuItemCheckmark(UINT nMenuItemID, bool set);