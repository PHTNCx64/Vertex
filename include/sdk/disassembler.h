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
// DISASSEMBLER CONSTANTS                                                                                         //
// ===============================================================================================================//

    // TODO: Remove hardcoded limits and use dynamic memory allocation instead

#define VERTEX_MAX_MNEMONIC_LENGTH      32
#define VERTEX_MAX_OPERANDS_LENGTH      128
#define VERTEX_MAX_COMMENT_LENGTH       256
#define VERTEX_MAX_BYTES_LENGTH         16
#define VERTEX_MAX_SYMBOL_LENGTH        64
#define VERTEX_MAX_SECTION_LENGTH       32

// ===============================================================================================================//
// DISASSEMBLER MACROS                                                                                            //
// ===============================================================================================================//

#define VERTEX_IS_BRANCH(result) \
((result)->flags & (VERTEX_FLAG_BRANCH | VERTEX_FLAG_CALL))

#define VERTEX_IS_MEMORY_OP(result) \
((result)->flags & (VERTEX_FLAG_MEMORY_READ | VERTEX_FLAG_MEMORY_WRITE))

#define VERTEX_IS_CONDITIONAL(result) \
((result)->flags & VERTEX_FLAG_CONDITIONAL)

#define VERTEX_IS_DANGEROUS(result) \
((result)->flags & (VERTEX_FLAG_DANGEROUS | VERTEX_FLAG_PRIVILEGED))

// ===============================================================================================================//
// DISASSEMBLER ENUMS                                                                                             //
// ===============================================================================================================//

typedef enum VertexInstructionCategory : int32_t
{
    VERTEX_INSTRUCTION_UNKNOWN        = 0,
    VERTEX_INSTRUCTION_ARITHMETIC     = 1,    // add, sub, mul, div, etc.
    VERTEX_INSTRUCTION_LOGIC          = 2,    // and, or, xor, not, etc.
    VERTEX_INSTRUCTION_DATA_TRANSFER  = 3,    // mov, lea, push, pop, etc.
    VERTEX_INSTRUCTION_CONTROL_FLOW   = 4,    // jmp, call, ret, je, etc.
    VERTEX_INSTRUCTION_COMPARISON     = 5,    // cmp, test, etc.
    VERTEX_INSTRUCTION_STRING         = 6,    // movs, stos, lods, etc.
    VERTEX_INSTRUCTION_SYSTEM         = 7,    // int, syscall, cpuid, etc.
    VERTEX_INSTRUCTION_FLOATING_POINT = 8,    // fadd, fmul, etc.
    VERTEX_INSTRUCTION_SIMD           = 9,    // SSE, AVX instructions
    VERTEX_INSTRUCTION_CRYPTO         = 10,   // AES, SHA instructions
    VERTEX_INSTRUCTION_PRIVILEGED     = 11
} InstructionCategory;

typedef enum VertexBranchType : int32_t
{
    VERTEX_BRANCH_NONE              = 0,    // Not a branch instruction
    VERTEX_BRANCH_UNCONDITIONAL     = 1,    // jmp (always taken)
    VERTEX_BRANCH_CONDITIONAL       = 2,    // je, jne, jg, jl, etc.
    VERTEX_BRANCH_CALL              = 3,    // call (function invocation)
    VERTEX_BRANCH_RETURN            = 4,    // ret, retn (function return)
    VERTEX_BRANCH_LOOP              = 5,    // loop, loope, loopne (x86)
    VERTEX_BRANCH_INTERRUPT         = 6,    // int, syscall, sysenter
    VERTEX_BRANCH_EXCEPTION         = 7,    // ud2, int3, into
    VERTEX_BRANCH_INDIRECT_JUMP     = 8,    // jmp rax, jmp [mem]
    VERTEX_BRANCH_INDIRECT_CALL     = 9,    // call rax, call [mem]
    VERTEX_BRANCH_CONDITIONAL_MOVE  = 10,   // cmovcc (data-dependent, not control)
    VERTEX_BRANCH_TABLE_SWITCH      = 11
} BranchType;

typedef enum VertexBranchDirection : int32_t
{
    VERTEX_DIRECTION_NONE           = 0,    // Not applicable (not a branch)
    VERTEX_DIRECTION_FORWARD        = 1,    // Target address > current address
    VERTEX_DIRECTION_BACKWARD       = 2,    // Target address < current (potential loop)
    VERTEX_DIRECTION_SELF           = 3,    // Target == current (infinite loop / spin)
    VERTEX_DIRECTION_OUT_OF_BLOCK   = 4,    // Leaves current basic block
    VERTEX_DIRECTION_OUT_OF_FUNC    = 5,    // Leaves current function (tail call, etc.)
    VERTEX_DIRECTION_EXTERNAL       = 6,    // Jumps to different module
    VERTEX_DIRECTION_UNKNOWN        = 7
} BranchDirection;

typedef enum VertexInstructionFlags : uint32_t
{
    VERTEX_FLAG_NONE           = 0x00000000,
    VERTEX_FLAG_BRANCH         = 0x00000001,  // Branch instruction
    VERTEX_FLAG_CONDITIONAL    = 0x00000002,  // Conditional branch/instruction
    VERTEX_FLAG_CALL           = 0x00000004,  // Function call
    VERTEX_FLAG_RETURN         = 0x00000008,  // Function return
    VERTEX_FLAG_PRIVILEGED     = 0x00000010,  // Requires elevated privileges
    VERTEX_FLAG_MEMORY_READ    = 0x00000020,  // Reads from memory
    VERTEX_FLAG_MEMORY_WRITE   = 0x00000040,  // Writes to memory
    VERTEX_FLAG_STACK_OP       = 0x00000080,  // Stack operation (push/pop)
    VERTEX_FLAG_INDIRECT       = 0x00000100,  // Indirect addressing
    VERTEX_FLAG_CRYPTO         = 0x00000200,  // Cryptographic instruction
    VERTEX_FLAG_DANGEROUS      = 0x00000400,  // Potentially dangerous operation
    VERTEX_FLAG_BREAKPOINT     = 0x00000800,  // User-set breakpoint
    VERTEX_FLAG_ANALYZED       = 0x00001000,  // Already analyzed by engine
    VERTEX_FLAG_PATCHED        = 0x00002000,  // Instruction has been patched
    VERTEX_FLAG_ENTRY_POINT    = 0x00004000,  // Function entry point
    VERTEX_FLAG_HOT_PATH       = 0x00008000
} InstructionFlags;

// ===============================================================================================================//
// DISASSEMBLER STRUCTURES                                                                                        //
// ===============================================================================================================//

typedef struct VertexDisassemblerResult
{
    uint64_t address;
    uint64_t physicalAddress;
    uint32_t size;
    uint8_t rawBytes[VERTEX_MAX_BYTES_LENGTH];

    char mnemonic[VERTEX_MAX_MNEMONIC_LENGTH];
    char operands[VERTEX_MAX_OPERANDS_LENGTH];
    char comment[VERTEX_MAX_COMMENT_LENGTH];

    InstructionCategory category;
    uint32_t flags;

    BranchType branchType;
    BranchDirection branchDirection;

    uint64_t targetAddress;
    uint64_t fallthroughAddress;
    char targetSymbol[VERTEX_MAX_SYMBOL_LENGTH];
    char sectionName[VERTEX_MAX_SECTION_LENGTH];

    uint32_t executionCount;
    uint64_t timestamp;

    uint32_t xrefCount;
    uint64_t functionStart;
    uint32_t instructionIndex;

    uint32_t reserved[4];

} DisassemblerResult;

typedef struct VertexDisassemblerResults
{
    DisassemblerResult* results;
    uint32_t count;
    uint32_t capacity;
    uint64_t startAddress;
    uint64_t endAddress;
    uint32_t totalSize;
} DisassemblerResults;

typedef struct VertexXReference
{
    uint64_t fromAddress;
    uint64_t toAddress;
    char fromSymbol[VERTEX_MAX_SYMBOL_LENGTH];
    char toSymbol[VERTEX_MAX_SYMBOL_LENGTH];
    uint32_t xrefType;
} XReference;

#ifdef __cplusplus
}
#endif
