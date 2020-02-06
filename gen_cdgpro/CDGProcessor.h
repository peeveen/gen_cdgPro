#pragma once
#include "stdafx.h"

bool StartCDGProcessor();
void StopCDGProcessor();

extern HANDLE g_hCDGProcessingThread;
extern DWORD g_nCDGProcessingThreadID;

extern HANDLE g_hStopCDGProcessingEvent;
extern HANDLE g_hStoppedCDGProcessingEvent;
extern HANDLE g_hStopCDGThreadEvent;
extern HANDLE g_hSongLoadedEvent;