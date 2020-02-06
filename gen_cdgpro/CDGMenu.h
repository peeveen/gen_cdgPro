#pragma once
#include "stdafx.h"

extern bool g_bFullScreen;

void SetFullScreen(bool fullscreen);
INT_PTR AboutDialogProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
void ShowAboutDialog();
void ShowMenu(int xPos, int yPos);
bool CreateRightClickMenu();
void DestroyRightClickMenu();