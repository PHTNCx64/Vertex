//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//
// Dynamic Registration API
// Allows plugins to register architecture-specific metadata (registers, flags, etc.)
//

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include "macro.h"
#include "statuscode.h"

#ifdef __cplusplus
extern "C"
{
#endif

    // ===============================================================================================================//
    // REGISTRY CONSTANTS                                                                                             //
    // ===============================================================================================================//

    // TODO: Some of the constants should be changed such as max categories or mem types.
    // It was easier to deal with on both Runtime and API sides, but it's better in the future to remove these hardcoding limits.

#define VERTEX_MAX_CATEGORY_ID_LENGTH 32
#define VERTEX_MAX_CATEGORY_NAME_LENGTH 64
#define VERTEX_MAX_REGISTER_NAME_LENGTH 16
#define VERTEX_MAX_FLAG_NAME_LENGTH 16
#define VERTEX_MAX_FLAG_DESC_LENGTH 128
#define VERTEX_MAX_CATEGORIES 32
#define VERTEX_MAX_REGISTERS_PER_CATEGORY 64
#define VERTEX_MAX_FLAG_BITS 64
#define VERTEX_MAX_MEMORY_TYPES 32
#define VERTEX_MAX_EXCEPTION_TYPES 64

    // ===============================================================================================================//
    // REGISTRY FLAGS                                                                                                 //
    // ===============================================================================================================//

    typedef enum VertexRegisterFlags : uint32_t
    {
        VERTEX_REG_FLAG_NONE = 0,
        VERTEX_REG_FLAG_READONLY = 1 << 0,        // Register cannot be modified
        VERTEX_REG_FLAG_HIDDEN = 1 << 1,          // Don't show in default view
        VERTEX_REG_FLAG_PROGRAM_COUNTER = 1 << 2, // This is the instruction pointer
        VERTEX_REG_FLAG_STACK_POINTER = 1 << 3,   // This is the stack pointer
        VERTEX_REG_FLAG_FRAME_POINTER = 1 << 4,   // This is the frame/base pointer
        VERTEX_REG_FLAG_FLAGS_REGISTER = 1 << 5,  // This is the flags/status register
        VERTEX_REG_FLAG_FLOATING_POINT = 1 << 6,  // Floating point register
        VERTEX_REG_FLAG_VECTOR = 1 << 7,          // Vector/SIMD register
        VERTEX_REG_FLAG_SEGMENT = 1 << 8,         // Segment register
    } RegisterFlags;

    typedef enum VertexDisasmSyntax : int32_t
    {
        VERTEX_DISASM_SYNTAX_INTEL = 0,
        VERTEX_DISASM_SYNTAX_ATT,
        VERTEX_DISASM_SYNTAX_CUSTOM
    } DisasmSyntax;

    typedef enum VertexEndianness : int32_t
    {
        VERTEX_ENDIAN_LITTLE = 0,
        VERTEX_ENDIAN_BIG
    } Endianness;

    // ===============================================================================================================//
    // REGISTRATION STRUCTURES                                                                                        //
    // ===============================================================================================================//

    // Register category definition
    typedef struct VertexRegisterCategoryDef
    {
        char categoryId[VERTEX_MAX_CATEGORY_ID_LENGTH];
        char displayName[VERTEX_MAX_CATEGORY_NAME_LENGTH];
        uint32_t displayOrder;
        uint8_t collapsedByDefault; // bool
        uint8_t reserved[3];
    } RegisterCategoryDef;

    // Individual register definition
    typedef struct VertexRegisterDef
    {
        char categoryId[VERTEX_MAX_CATEGORY_ID_LENGTH];
        char name[VERTEX_MAX_REGISTER_NAME_LENGTH];
        char parentName[VERTEX_MAX_REGISTER_NAME_LENGTH]; // For sub-registers (AL is part of RAX)
        uint8_t bitWidth;                                 // 8, 16, 32, 64, 128, 256, 512
        uint8_t bitOffset;                                // Offset within parent register (for sub-registers)
        uint16_t flags;                                   // RegisterFlags
        uint32_t displayOrder;
        uint32_t registerId; // Unique ID assigned by plugin for read/write
        void(VERTEX_API* write_func)(void* in, size_t size);
        void(VERTEX_API* read_func)(void* out, size_t size);
    } RegisterDef;

    // Flag bit definition (for flags registers like EFLAGS)
    typedef struct VertexFlagBitDef
    {
        char flagsRegisterName[VERTEX_MAX_REGISTER_NAME_LENGTH];
        char bitName[VERTEX_MAX_FLAG_NAME_LENGTH];
        char description[VERTEX_MAX_FLAG_DESC_LENGTH];
        uint8_t bitPosition;
        uint8_t reserved[3];
    } FlagBitDef;

    typedef struct VertexArchitectureInfo
    {
        Endianness endianness;
        DisasmSyntax preferredSyntax;
        uint8_t addressWidth;
        uint8_t maxHardwareBreakpoints;
        uint8_t stackGrowsDown; // bool
        uint8_t reserved[2];
        char architectureName[32];
    } ArchitectureInfo;

    typedef struct VertexExceptionTypeDef
    {
        uint32_t exceptionCode;
        char name[32];
        char description[128];
        uint8_t isFatal; // bool
        uint8_t reserved[3];
    } ExceptionTypeDef;

    typedef struct VertexCallingConventionDef
    {
        char name[32];
        char parameterRegisters[8][VERTEX_MAX_REGISTER_NAME_LENGTH];
        uint8_t parameterRegisterCount;
        char returnRegister[VERTEX_MAX_REGISTER_NAME_LENGTH];
        uint8_t stackCleanup;
        uint8_t reserved[2];
    } CallingConventionDef;

    // Complete registry snapshot (for bulk registration)
    typedef struct VertexRegistrySnapshot
    {
        ArchitectureInfo archInfo;

        RegisterCategoryDef* categories;
        uint32_t categoryCount;

        RegisterDef* registers;
        uint32_t registerCount;

        FlagBitDef* flagBits;
        uint32_t flagBitCount;

        ExceptionTypeDef* exceptionTypes;
        uint32_t exceptionTypeCount;

        CallingConventionDef* callingConventions;
        uint32_t callingConventionCount;
    } RegistrySnapshot;

    // ===============================================================================================================//
    // REGISTRY FUNCTIONS (called by Vertex core to set instance, by plugins via Runtime struct)                     //
    // ===============================================================================================================//

    // Instance management (called by Vertex core only)
    VERTEX_EXPORT StatusCode VERTEX_API vertex_registry_set_instance(void* handle);
    VERTEX_EXPORT void* VERTEX_API vertex_registry_get_instance();

    // Registration functions (used via Runtime struct in plugins)
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_architecture(const ArchitectureInfo* archInfo);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_category(const RegisterCategoryDef* category);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_unregister_category(const char* categoryId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_register(const RegisterDef* reg);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_unregister_register(const char* registerName);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_flag_bit(const FlagBitDef* flagBit);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_exception_type(const ExceptionTypeDef* exceptionType);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_calling_convention(const CallingConventionDef* callingConv);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_register_snapshot(const RegistrySnapshot* snapshot);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_clear_registry();

#ifdef __cplusplus
}
#endif
