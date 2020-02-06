#pragma once
#include "stdafx.h"

bool StartCDGProcessor();
void StopCDGProcessor();
void ResetProcessor();

extern HANDLE g_hStopCDGProcessingEvent;
extern HANDLE g_hStoppedCDGProcessingEvent;
extern HANDLE g_hStopCDGThreadEvent;
extern HANDLE g_hSongLoadedEvent;

extern DWORD g_nCDGPC;