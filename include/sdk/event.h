//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// ===============================================================================================================//
// EVENT TYPES                                                                                                    //
// ===============================================================================================================//

typedef enum VertexEvent : int32_t
{
    // Process events
    VERTEX_PROCESS_OPENED,
    VERTEX_PROCESS_CLOSED,
    VERTEX_PROCESS_KILLED,
    VERTEX_ERROR_OCCURRED,

    // Debugger events
    VERTEX_DEBUGGER_ATTACHED,
    VERTEX_DEBUGGER_DETACHED,
    VERTEX_DEBUGGER_BREAKPOINT_HIT,
    VERTEX_DEBUGGER_STEP_COMPLETE,
    VERTEX_DEBUGGER_EXCEPTION,
} Event;

// ===============================================================================================================//
// EVENT DATA STRUCTURES                                                                                          //
// ===============================================================================================================//

// Data passed with VERTEX_PROCESS_OPENED and VERTEX_DEBUGGER_ATTACHED events
typedef struct VertexProcessEventData
{
    uint32_t processId;
    void* processHandle;    // Platform-specific handle (HANDLE on Windows for example)
} ProcessEventData;

// Data passed with VERTEX_DEBUGGER_BREAKPOINT_HIT event
typedef struct VertexBreakpointEventData
{
    uint32_t breakpointId;
    uint64_t address;
    uint32_t threadId;
} BreakpointEventData;

// Data passed with VERTEX_DEBUGGER_EXCEPTION event
typedef struct VertexExceptionEventData
{
    uint32_t exceptionCode;
    uint64_t address;
    uint32_t threadId;
    uint8_t firstChance; // bool
    uint8_t reserved[3];
} ExceptionEventData;

#ifdef __cplusplus
}
#endif