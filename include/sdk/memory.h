//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>

#include "statuscode.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===============================================================================================================//
// MEMORY CONSTANTS                                                                                               //
// ===============================================================================================================//

#define VERTEX_VARIABLE_LENGTH (0)
#define VERTEX_MAX_STRING_LENGTH 255

#define VERTEX_NUMERIC_SYSTEMS_SUPPORTED 1
#define VERTEX_NUMERIC_SYSTEMS_NOT_SUPPORTED 0

// ===============================================================================================================//
// MEMORY ENUMS                                                                                                   //
// ===============================================================================================================//

typedef enum VertexNumericSystem : int32_t
{
    VERTEX_NONE = 0,
    VERTEX_BINARY = 2,
    VERTEX_OCTAL = 8,
    VERTEX_DECIMAL = 10,
    VERTEX_HEXADECIMAL = 16,
} NumericSystem;

// ===============================================================================================================//
// MEMORY STRUCTURES                                                                                              //
// ===============================================================================================================//

typedef struct VertexMemoryRegion
{
    const char* baseModuleName;
    uint64_t baseAddress;
    uint64_t regionSize;
} MemoryRegion;

typedef enum VertexMemoryAttributeType : int32_t
{
    VERTEX_PROTECTION = 0,
    VERTEX_STATE,
    VERTEX_TYPE,
    VERTEX_OTHER,
} MemoryAttributeType;

typedef void (*VertexOptionState_t)(uint8_t state);

typedef struct VertexMemoryAttributeOption
{
    const char* memoryAttributeName;
    VertexOptionState_t stateFunction;
    MemoryAttributeType memoryAttributeType;
    uint8_t* currentState; // bool
} MemoryAttributeOption;

typedef StatusCode (*VertexValidateInput_t)(NumericSystem system, const char* input, char* output);
typedef StatusCode (*VertexMatchesEqual_t)(const char* input);
typedef StatusCode (*VertexMatchesLesser_t)(const char* input);
typedef StatusCode (*VertexMatchesGreater_t)(const char* input);
typedef StatusCode (*VertexMatchesBetween_t)(const char* input1, const char* input2);
typedef StatusCode (*VertexMatchesUnknownInitialValue_t)(const char* input);

typedef struct VertexMemoryDataType
{
    const char* memoryDataTypeName;
    int64_t memoryDataTypeSize;
    VertexValidateInput_t validateInputFunction;
    uint8_t supportsNumericSystems; // bool
    uint8_t reserved[3];
} MemoryDataType;

// ===============================================================================================================//
// DYNAMIC SCAN TYPE API - Custom Data Types in C/C++                                                            //
// ===============================================================================================================//

typedef StatusCode (*VertexConverter_t)(const char* input, NumericSystem numericBase, char* output, size_t outputSize, size_t* bytesWritten);

typedef StatusCode (*VertexExtractor_t)(const char* memoryBytes, size_t memorySize, char* output, size_t outputSize);

typedef StatusCode (*VertexFormatter_t)(const char* extractedValue, char* output, size_t outputSize);

typedef StatusCode (*VertexComparator_t)(const char* currentValue, const char* previousValue, const char* userInput, uint8_t* result);

typedef struct VertexScanMode
{
    const char* scanModeName;
    VertexComparator_t comparator;
    uint8_t needsInput; // bool
    uint8_t needsPrevious; // bool
    uint8_t reserved[2];
} ScanMode;

typedef struct VertexDataType
{
    const char* typeName;
    size_t valueSize;
    VertexConverter_t converter;
    VertexExtractor_t extractor;
    VertexFormatter_t formatter;
    ScanMode* scanModes;
    size_t scanModeCount;
} DataType;

#ifdef __cplusplus
}
#endif