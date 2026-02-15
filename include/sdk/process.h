//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>

#include "macro.h"
#include "statuscode.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===============================================================================================================//
// PROCESS CONSTANTS                                                                                              //
// ===============================================================================================================//

#if defined(_WIN32) || defined(_WIN64)
    // Windows also supports lengths up to 32767 bytes, but for compatibility reasons
    // we go down to 260 which was used prior to the new limit introduction.
    #define VERTEX_MAX_PATH_LENGTH 260
    #define VERTEX_MAX_NAME_LENGTH 260
    #define VERTEX_MAX_OWNER_LENGTH 256
#elif defined(__linux__) || defined(__linux) || defined(linux) || defined(__gnu_linux__)
    #define VERTEX_MAX_PATH_LENGTH 4096
    #define VERTEX_MAX_NAME_LENGTH 255
    #define VERTEX_MAX_OWNER_LENGTH 32
#elif defined(__APPLE__) || defined(__MACH__)
    #define VERTEX_MAX_PATH_LENGTH 1024
    #define VERTEX_MAX_NAME_LENGTH 255
    #define VERTEX_MAX_OWNER_LENGTH 255
#elif defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__) || defined(__DragonFly__)
    #define VERTEX_MAX_PATH_LENGTH 1024
    #define VERTEX_MAX_NAME_LENGTH 255
    #define VERTEX_MAX_OWNER_LENGTH 32
#endif

// ===============================================================================================================//
// PROCESS STRUCTURES                                                                                             //
// ===============================================================================================================//

typedef struct VertexProcessInformation
{
    char processName[VERTEX_MAX_NAME_LENGTH];
    char processOwner[VERTEX_MAX_OWNER_LENGTH];
    uint32_t processId;
} ProcessInformation;

typedef struct VertexModuleInformation
{
    char moduleName[VERTEX_MAX_NAME_LENGTH];
    char modulePath[VERTEX_MAX_PATH_LENGTH];
    uint64_t baseAddress;
    uint64_t size;
} ModuleInformation;

typedef struct VertexInjectionMethod
{
    char methodName[VERTEX_MAX_NAME_LENGTH];
    StatusCode(VERTEX_API* injectableFunction)(const char* path);
} InjectionMethod;

typedef struct VertexModuleEntry
{
    const char* name;
    void* address;
    size_t size;
    int32_t ordinal;
    uint8_t isFunction; // bool
    uint8_t isImport; // bool
    uint8_t isForwarder; // bool
    uint8_t reserved;
    const char* forwarderName;
    void* moduleHandle;
} ModuleEntry;

typedef struct VertexModuleImport
{
    ModuleEntry entry;
    const char* libraryName;
    void* importAddress;
    int32_t hint;
    uint8_t isOrdinal; // bool
    uint8_t reserved[3];
} ModuleImport;

typedef struct VertexModuleExport
{
    ModuleEntry entry;
    const char* moduleName;
    uint8_t isData; // bool
    uint8_t isThunk; // bool
    uint8_t reserved[2];
    void* relocationTable;
    int32_t characteristics;
} ModuleExport;

#ifdef __cplusplus
}
#endif