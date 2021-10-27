#pragma once
#include "stdafx.h"

// Functions relating to the CDG processor thread.

/// <summary>
/// Starts the CDG processor thread.
/// </summary>
/// <returns>True if started successfully.</returns>
bool StartCDGProcessor();

/// <summary>
/// Stops the CDG processor thread.
/// </summary>
void StopCDGProcessor();

/// <summary>
/// Resets the CDG processor (clears state).
/// </summary>
void ResetProcessor();

/// <summary>
/// Reads a CDG file into the instruction buffer.
/// </summary>
/// <param name="pFileBeingPlayed">Path to CDG file.</param>
/// <returns>True if read successfully.</returns>
bool ReadCDGData(const WCHAR* pFileBeingPlayed);

/// <summary>
/// Clears and de-allocates the existing CDG buffer.
/// </summary>
void ClearExistingCDGData();

/// <summary>
/// Events to control and coordinate the startup/shutdown of the CDG processor thread.
/// </summary>
extern HANDLE g_hStopCDGProcessingEvent;
extern HANDLE g_hStoppedCDGProcessingEvent;
extern HANDLE g_hStopCDGThreadEvent;
extern HANDLE g_hSongLoadedEvent;

/// <summary>
/// CDG state machine "program counter".
/// </summary>
extern DWORD g_nCDGPC;

/// <summary>
/// List of CDG instructions from a CDG file.
/// </summary>
extern CDGPacket* g_pCDGData;

/// <summary>
/// Number of packets contained in g_pCDGData
/// </summary>
extern DWORD g_nCDGPackets;

