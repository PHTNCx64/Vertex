//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>
#include "macro.h"
#include "statuscode.h"
#include "process.h"

#ifdef __cplusplus
extern "C" {
#endif

    // ===============================================================================================================//
    // DEBUGGER CONSTANTS                                                                                             //
    // ===============================================================================================================//

    // TODO: Some of the constants should be changed such as Max Threads, max breakpoints etc.
    // It was easier to deal with on both Runtime and API sides, but it's better in the future to remove these hardcoding limits.

#define VERTEX_MAX_REGISTER_NAME_LENGTH 16
#define VERTEX_MAX_FUNCTION_NAME_LENGTH 256
#define VERTEX_MAX_SOURCE_FILE_LENGTH 512
#define VERTEX_MAX_REGISTERS 128
#define VERTEX_MAX_STACK_FRAMES 256
#define VERTEX_MAX_THREADS 256
#define VERTEX_MAX_BREAKPOINTS 1024
#define VERTEX_MAX_EXCEPTION_DESC_LENGTH 512
#define VERTEX_MAX_SYMBOL_NAME_LENGTH 512
#define VERTEX_MAX_EXPRESSION_LENGTH 1024
#define VERTEX_MAX_CONDITION_LENGTH 256
#define VERTEX_MAX_SYMBOLS 4096
#define VERTEX_MAX_HW_BREAKPOINTS 4
#define VERTEX_INFINITE_WAIT 0xFFFFFFFF

    // ===============================================================================================================//
    // DEBUGGER STATE ENUMS                                                                                           //
    // ===============================================================================================================//

    typedef enum VertexDebuggerState : int32_t
    {
        VERTEX_DBG_STATE_DETACHED = 0,
        VERTEX_DBG_STATE_ATTACHED,
        VERTEX_DBG_STATE_RUNNING,
        VERTEX_DBG_STATE_PAUSED,
        VERTEX_DBG_STATE_STEPPING,
        VERTEX_DBG_STATE_BREAKPOINT_HIT,
        VERTEX_DBG_STATE_EXCEPTION
    } DebuggerState;

    typedef enum VertexStepMode : int32_t
    {
        VERTEX_STEP_INTO = 0,
        VERTEX_STEP_OVER,
        VERTEX_STEP_OUT
    } StepMode;

    typedef enum VertexBreakpointType : int32_t
    {
        VERTEX_BP_EXECUTE = 0,
        VERTEX_BP_READ,
        VERTEX_BP_WRITE,
        VERTEX_BP_READWRITE,
    } BreakpointType;

    typedef enum VertexBreakpointState : int32_t
    {
        VERTEX_BP_STATE_ENABLED = 0,
        VERTEX_BP_STATE_DISABLED,
        VERTEX_BP_STATE_PENDING,
        VERTEX_BP_STATE_ERROR,
    } BreakpointState;

    typedef enum VertexThreadState : int32_t
    {
        VERTEX_THREAD_RUNNING = 0,
        VERTEX_THREAD_SUSPENDED,
        VERTEX_THREAD_WAITING,
        VERTEX_THREAD_TERMINATED,
    } ThreadState;

    typedef enum VertexDebugEventType : int32_t
    {
        VERTEX_DBG_EVENT_NONE = 0,
        VERTEX_DBG_EVENT_BREAKPOINT,
        VERTEX_DBG_EVENT_SINGLE_STEP,
        VERTEX_DBG_EVENT_EXCEPTION,
        VERTEX_DBG_EVENT_THREAD_CREATE,
        VERTEX_DBG_EVENT_THREAD_EXIT,
        VERTEX_DBG_EVENT_PROCESS_CREATE,
        VERTEX_DBG_EVENT_PROCESS_EXIT,
        VERTEX_DBG_EVENT_LIBRARY_LOAD,
        VERTEX_DBG_EVENT_LIBRARY_UNLOAD,
        VERTEX_DBG_EVENT_OUTPUT_STRING,
    } DebugEventType;

    typedef enum VertexRegisterCategory : int32_t
    {
        VERTEX_REG_GENERAL = 0,
        VERTEX_REG_SEGMENT,
        VERTEX_REG_FLAGS,
        VERTEX_REG_FLOATING_POINT,
        VERTEX_REG_VECTOR,
        VERTEX_REG_DEBUG,
        VERTEX_REG_CONTROL,
    } RegisterCategory;

    typedef enum VertexExceptionCode : int32_t
    {
        VERTEX_EXCEPTION_NONE = 0,
        VERTEX_EXCEPTION_ACCESS_VIOLATION,
        VERTEX_EXCEPTION_BREAKPOINT,
        VERTEX_EXCEPTION_SINGLE_STEP,
        VERTEX_EXCEPTION_ARRAY_BOUNDS_EXCEEDED,
        VERTEX_EXCEPTION_DATATYPE_MISALIGNMENT,
        VERTEX_EXCEPTION_FLT_DENORMAL_OPERAND,
        VERTEX_EXCEPTION_FLT_DIVIDE_BY_ZERO,
        VERTEX_EXCEPTION_FLT_INEXACT_RESULT,
        VERTEX_EXCEPTION_FLT_INVALID_OPERATION,
        VERTEX_EXCEPTION_FLT_OVERFLOW,
        VERTEX_EXCEPTION_FLT_STACK_CHECK,
        VERTEX_EXCEPTION_FLT_UNDERFLOW,
        VERTEX_EXCEPTION_ILLEGAL_INSTRUCTION,
        VERTEX_EXCEPTION_INT_DIVIDE_BY_ZERO,
        VERTEX_EXCEPTION_INT_OVERFLOW,
        VERTEX_EXCEPTION_PRIV_INSTRUCTION,
        VERTEX_EXCEPTION_STACK_OVERFLOW,
        VERTEX_EXCEPTION_UNKNOWN,
    } ExceptionCode;

    typedef enum VertexSymbolType : int32_t
    {
        VERTEX_SYM_UNKNOWN = 0,
        VERTEX_SYM_FUNCTION,
        VERTEX_SYM_DATA,
        VERTEX_SYM_LABEL,
        VERTEX_SYM_PUBLIC,
        VERTEX_SYM_PARAMETER,
        VERTEX_SYM_LOCAL,
        VERTEX_SYM_TYPEDEF,
        VERTEX_SYM_ENUM,
        VERTEX_SYM_STRUCT,
        VERTEX_SYM_UNION,
        VERTEX_SYM_CLASS,
        VERTEX_SYM_THUNK,
    } SymbolType;

    typedef enum VertexSymbolFlags : uint32_t
    {
        VERTEX_SYM_FLAG_NONE = 0,
        VERTEX_SYM_FLAG_EXPORT = 1 << 0,
        VERTEX_SYM_FLAG_IMPORT = 1 << 1,
        VERTEX_SYM_FLAG_STATIC = 1 << 2,
        VERTEX_SYM_FLAG_VIRTUAL = 1 << 3,
        VERTEX_SYM_FLAG_CONST = 1 << 4,
        VERTEX_SYM_FLAG_VOLATILE = 1 << 5,
    } SymbolFlags;

    typedef enum VertexExpressionValueType : int32_t
    {
        VERTEX_EXPR_TYPE_INVALID = 0,
        VERTEX_EXPR_TYPE_INT8,
        VERTEX_EXPR_TYPE_UINT8,
        VERTEX_EXPR_TYPE_INT16,
        VERTEX_EXPR_TYPE_UINT16,
        VERTEX_EXPR_TYPE_INT32,
        VERTEX_EXPR_TYPE_UINT32,
        VERTEX_EXPR_TYPE_INT64,
        VERTEX_EXPR_TYPE_UINT64,
        VERTEX_EXPR_TYPE_FLOAT32,
        VERTEX_EXPR_TYPE_FLOAT64,
        VERTEX_EXPR_TYPE_POINTER,
        VERTEX_EXPR_TYPE_STRING,
        VERTEX_EXPR_TYPE_ARRAY,
        VERTEX_EXPR_TYPE_STRUCT,
        VERTEX_EXPR_TYPE_VOID,
    } ExpressionValueType;

    typedef enum VertexBreakpointConditionType : int32_t
    {
        VERTEX_BP_COND_NONE = 0,
        VERTEX_BP_COND_EXPRESSION,
        VERTEX_BP_COND_HIT_COUNT_EQUAL,
        VERTEX_BP_COND_HIT_COUNT_GREATER,
        VERTEX_BP_COND_HIT_COUNT_MULTIPLE,
    } BreakpointConditionType;

    // ===============================================================================================================//
    // DEBUGGER STRUCTURES                                                                                            //
    // ===============================================================================================================//

    typedef struct VertexRegister
    {
        char name[VERTEX_MAX_REGISTER_NAME_LENGTH];
        RegisterCategory category;
        uint64_t value;
        uint64_t previousValue;
        uint8_t bitWidth;
        uint8_t modified; // bool
    } Register;

    typedef struct VertexRegisterSet
    {
        Register registers[VERTEX_MAX_REGISTERS];
        uint32_t registerCount;
        uint64_t instructionPointer;
        uint64_t stackPointer;
        uint64_t basePointer;
        uint64_t flagsRegister;
    } RegisterSet;

    typedef struct VertexStackFrame
    {
        uint32_t frameIndex;
        uint64_t returnAddress;
        uint64_t framePointer;
        uint64_t stackPointer;
        char functionName[VERTEX_MAX_FUNCTION_NAME_LENGTH];
        char moduleName[VERTEX_MAX_NAME_LENGTH];
        char sourceFile[VERTEX_MAX_SOURCE_FILE_LENGTH];
        uint32_t sourceLine;
    } StackFrame;

    typedef struct VertexCallStack
    {
        StackFrame frames[VERTEX_MAX_STACK_FRAMES];
        uint32_t frameCount;
        uint32_t currentFrameIndex;
    } CallStack;

    typedef struct VertexThreadInfo
    {
        uint32_t id;
        char name[VERTEX_MAX_NAME_LENGTH];
        ThreadState state;
        uint64_t instructionPointer;
        uint64_t stackPointer;
        uint64_t entryPoint;
        int32_t priority;
        uint8_t isCurrent; // bool
    } ThreadInfo;

    typedef struct VertexThreadList
    {
        ThreadInfo threads[VERTEX_MAX_THREADS];
        uint32_t threadCount;
        uint32_t currentThreadId;
    } ThreadList;

    typedef struct VertexBreakpointInfo
    {
        uint32_t id;
        uint64_t address;
        BreakpointType type;
        BreakpointState state;
        char moduleName[VERTEX_MAX_NAME_LENGTH];
        uint32_t hitCount;
        uint8_t temporary;       // bool
        uint8_t originalByte;    // For software breakpoints
        uint8_t hwRegisterIndex; // For hardware breakpoints (0-3 on x86)
        uint8_t reserved;
    } BreakpointInfo;

    typedef struct VertexDebugEvent
    {
        DebugEventType type;
        uint32_t threadId;
        uint64_t address;
        uint32_t exceptionCode;
        uint8_t firstChance; // bool
        char description[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
        uint32_t breakpointId; // If type is BREAKPOINT
    } DebugEvent;

    typedef struct VertexExceptionInfo
    {
        ExceptionCode code;
        uint64_t address;
        uint64_t accessAddress; // For access violation
        uint8_t isWrite;        // For access violation: 0=read, 1=write, 8=execute
        uint8_t firstChance;
        uint8_t continuable;
        uint8_t reserved;
        uint32_t threadId;
        char description[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    } ExceptionInfo;

    // ===============================================================================================================//
    // SYMBOL RESOLUTION STRUCTURES                                                                                  //
    // ===============================================================================================================//

    typedef struct VertexSymbolInfo
    {
        char name[VERTEX_MAX_SYMBOL_NAME_LENGTH];
        char moduleName[VERTEX_MAX_NAME_LENGTH];
        uint64_t address;
        uint64_t size;
        SymbolType type;
        uint32_t flags;
        uint32_t typeId;
        uint32_t parentId;
    } SymbolInfo;

    typedef struct VertexSourceLocation
    {
        char fileName[VERTEX_MAX_SOURCE_FILE_LENGTH];
        uint32_t lineNumber;
        uint32_t columnNumber;
        uint64_t address;
        uint64_t endAddress;
    } SourceLocation;

    typedef struct VertexSymbolSearchResult
    {
        SymbolInfo* symbols;
        uint32_t symbolCount;
        uint32_t totalMatches;
        uint8_t hasMore; // bool
        uint8_t reserved[3];
    } SymbolSearchResult;

    typedef struct VertexLineInfo
    {
        uint64_t address;
        uint32_t lineNumber;
        uint32_t lineEndNumber;
        uint8_t isStatement; // bool
        uint8_t reserved[3];
    } LineInfo;

    typedef struct VertexSourceFileInfo
    {
        char fileName[VERTEX_MAX_SOURCE_FILE_LENGTH];
        char compiledPath[VERTEX_MAX_PATH_LENGTH];
        uint64_t checksum;
        uint32_t lineCount;
        LineInfo* lines;
    } SourceFileInfo;

    // ===============================================================================================================//
    // EXPRESSION EVALUATION STRUCTURES                                                                              //
    // ===============================================================================================================//

    typedef struct VertexExpressionValue
    {
        ExpressionValueType type;
        uint32_t size;
        union
        {
            int8_t i8;
            uint8_t u8;
            int16_t i16;
            uint16_t u16;
            int32_t i32;
            uint32_t u32;
            int64_t i64;
            uint64_t u64;
            float f32;
            double f64;
            uint64_t pointer;
            char* string;
            void* rawData;
        } data;
        char typeName[VERTEX_MAX_NAME_LENGTH];
        uint64_t address;
        uint8_t isValid; // bool
        uint8_t isReadOnly; // bool
        uint8_t hasChildren; // bool
        uint8_t reserved;
    } ExpressionValue;

    typedef struct VertexExpressionResult
    {
        char expression[VERTEX_MAX_EXPRESSION_LENGTH];
        ExpressionValue value;
        char errorMessage[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
        uint8_t success; // bool
        uint8_t reserved[3];
    } ExpressionResult;

    typedef struct VertexWatchEntry
    {
        uint32_t id;
        char expression[VERTEX_MAX_EXPRESSION_LENGTH];
        ExpressionValue currentValue;
        ExpressionValue previousValue;
        uint8_t enabled; // bool
        uint8_t valueChanged; // bool
        uint8_t reserved[2];
    } WatchEntry;

    // ===============================================================================================================//
    // CONDITIONAL BREAKPOINT STRUCTURES                                                                             //
    // ===============================================================================================================//

    typedef struct VertexBreakpointCondition
    {
        BreakpointConditionType type;
        char expression[VERTEX_MAX_CONDITION_LENGTH];
        uint32_t hitCountTarget;
        uint8_t enabled; // bool
        uint8_t reserved[3];
    } BreakpointCondition;

    typedef struct VertexBreakpointAction
    {
        uint8_t logMessage; // bool
        uint8_t continueExecution; // bool
        uint8_t playSound; // bool
        uint8_t reserved;
        char logFormat[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    } BreakpointAction;

    typedef struct VertexConditionalBreakpoint
    {
        uint32_t breakpointId;
        BreakpointCondition condition;
        BreakpointAction action;
    } ConditionalBreakpoint;

    // ===============================================================================================================//
    // HARDWARE BREAKPOINT STATUS                                                                                    //
    // ===============================================================================================================//

    typedef struct VertexHardwareBreakpointStatus
    {
        uint8_t registerInUse[VERTEX_MAX_HW_BREAKPOINTS];
        uint32_t breakpointIds[VERTEX_MAX_HW_BREAKPOINTS];
        uint64_t addresses[VERTEX_MAX_HW_BREAKPOINTS];
        BreakpointType types[VERTEX_MAX_HW_BREAKPOINTS];
        uint8_t sizes[VERTEX_MAX_HW_BREAKPOINTS];
        uint32_t availableCount;
    } HardwareBreakpointStatus;

    // ===============================================================================================================//
    // LOCAL VARIABLE STRUCTURES                                                                                     //
    // ===============================================================================================================//

    typedef struct VertexLocalVariable
    {
        char name[VERTEX_MAX_NAME_LENGTH];
        char typeName[VERTEX_MAX_NAME_LENGTH];
        uint64_t address;
        int32_t stackOffset;
        uint32_t size;
        ExpressionValueType valueType;
        uint8_t isParameter; // bool
        uint8_t isRegister; // bool
        uint8_t registerIndex;
        uint8_t reserved;
    } LocalVariable;

    typedef struct VertexLocalVariableList
    {
        LocalVariable* variables;
        uint32_t variableCount;
        uint32_t frameIndex;
    } LocalVariableList;

    // ===============================================================================================================//
    // DEBUGGER CALLBACK STRUCTURES                                                                                //
    // ===============================================================================================================//

    // Forward declaration for callback context
    struct VertexDebuggerCallbacks;

    // Thread event information (for create/exit callbacks)
    typedef struct VertexThreadEvent
    {
        uint32_t threadId;
        uint64_t entryPoint;      // Start address (valid on create, 0 on exit)
        uint64_t stackBase;       // Thread stack base address
        int32_t exitCode;         // Exit code (valid on exit, 0 on create)
    } ThreadEvent;

    // Module event information (for load/unload callbacks)
    typedef struct VertexModuleEvent
    {
        char moduleName[VERTEX_MAX_NAME_LENGTH];
        char modulePath[VERTEX_MAX_PATH_LENGTH];
        uint64_t baseAddress;
        uint64_t size;
        uint8_t isMainModule; // bool
        uint8_t reserved[3];
    } ModuleEvent;

    // Output string event (debug output)
    typedef struct VertexOutputStringEvent
    {
        uint32_t threadId;
        char message[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    } OutputStringEvent;

    typedef enum VertexWatchpointType : int32_t
    {
        VERTEX_WP_READ = 0,
        VERTEX_WP_WRITE,
        VERTEX_WP_READWRITE,
        VERTEX_WP_EXECUTE,
    } WatchpointType;

    // Memory watchpoint hit event
    typedef struct VertexWatchpointEvent
    {
        uint32_t breakpointId;
        uint32_t threadId;
        uint64_t address;         // Address that was accessed
        uint64_t accessAddress;   // Instruction that caused the access
        WatchpointType type;      // READ, WRITE, or ACCESS
        uint8_t size;             // Size of the access (1, 2, 4, 8 bytes)
    } WatchpointEvent;

    // Debugger error event
    typedef struct VertexDebuggerError
    {
        StatusCode code;
        char message[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
        uint8_t isFatal; // bool
        uint8_t reserved[3];
    } DebuggerError;

    // Callback function pointer typedefs
    typedef void (*VertexOnBreakpointHit)(const DebugEvent* event, void* user_data);
    typedef void (*VertexOnSingleStep)(const DebugEvent* event, void* user_data);
    typedef void (*VertexOnException)(const DebugEvent* event, void* user_data);
    typedef void (*VertexOnThreadCreated)(const ThreadEvent* event, void* user_data);
    typedef void (*VertexOnThreadExited)(const ThreadEvent* event, void* user_data);
    typedef void (*VertexOnModuleLoaded)(const ModuleEvent* module, void* user_data);
    typedef void (*VertexOnModuleUnloaded)(const ModuleEvent* module, void* user_data);
    typedef void (*VertexOnProcessExited)(int32_t exit_code, void* user_data);
    typedef void (*VertexOnOutputString)(const OutputStringEvent* event, void* user_data);
    typedef void (*VertexOnWatchpointHit)(const WatchpointEvent* event, void* user_data);
    typedef void (*VertexOnStateChanged)(DebuggerState old_state, DebuggerState new_state, void* user_data);
    typedef void (*VertexOnAttached)(uint32_t process_id, void* user_data);
    typedef void (*VertexOnDetached)(uint32_t process_id, void* user_data);
    typedef void (*VertexOnError)(const DebuggerError* error, void* user_data);

    typedef struct VertexDebuggerCallbacks
    {
        // Execution events
        VertexOnBreakpointHit on_breakpoint_hit;    // Software/hardware breakpoint hit
        VertexOnSingleStep on_single_step;          // Single-step completed
        VertexOnException on_exception;             // Exception occurred
        VertexOnWatchpointHit on_watchpoint_hit;    // Memory watchpoint triggered

        // Thread events
        VertexOnThreadCreated on_thread_created;    // New thread created
        VertexOnThreadExited on_thread_exited;      // Thread exited

        // Module events
        VertexOnModuleLoaded on_module_loaded;      // Module/DLL loaded
        VertexOnModuleUnloaded on_module_unloaded;  // Module/DLL unloaded

        // Process events
        VertexOnProcessExited on_process_exited;    // Target process exited

        // Debug output
        VertexOnOutputString on_output_string;      // Debug output string received

        // State management
        VertexOnStateChanged on_state_changed;      // Debugger state changed
        VertexOnAttached on_attached;               // Successfully attached to process
        VertexOnDetached on_detached;               // Detached from process

        // Error handling
        VertexOnError on_error;                     // Error occurred in debugger

        void* user_data;                            // User-defined data pointer passed to callbacks
    } DebuggerCallbacks;

    // Helper macro to initialize callbacks struct with proper size
    #define VERTEX_INIT_DEBUGGER_CALLBACKS(cb) \
        do { \
            memset(&(cb), 0, sizeof(DebuggerCallbacks)); \
            (cb).structSize = sizeof(DebuggerCallbacks); \
        } while(0)


    typedef struct VertexWatchpoint
    {
        WatchpointType type;
        uint64_t address;
        uint32_t size;
        uint8_t active; // bool
        uint8_t reserved[3];
    } Watchpoint;

    typedef struct VertexWatchpointInfo
    {
        uint32_t id;
        uint64_t address;
        uint32_t size;
        WatchpointType type;
        uint8_t enabled; // bool
        uint8_t hwRegisterIndex;
        uint8_t reserved[2];
        uint32_t hitCount;
    } WatchpointInfo;

    // ===============================================================================================================//
    // RUNTIME REGISTER ACCESS                                                                                        //
    // ===============================================================================================================//
    //
    // This API provides runtime register access via function pointers, enabling:
    // - Dynamic architecture switching (WOW64/native x64/ARM64) without recompilation
    // - Unified interface for all register sizes (8-bit to 512-bit)
    // - Thread context caching for performance optimization
    //
    // Usage:
    //   1. Call vertex_debugger_get_register_accessors() to get the accessor set
    //   2. Use vertex_debugger_begin_register_batch() before reading multiple registers
    //   3. Call accessor->read() or accessor->write() for each register
    //   4. Call vertex_debugger_end_register_batch() when done
    //

    // Forward declaration for RegisterAccess
    struct VertexRegisterAccess;

    // Function pointer type for reading a register
    // thread_id: OS thread identifier
    // out: pointer to buffer to receive register value (size determined by bitWidth)
    // size: buffer size in bytes (must be >= bitWidth/8)
    // Returns: STATUS_OK on success, error code on failure
    typedef StatusCode (*VertexRegisterRead_t)(uint32_t thread_id, void* out, size_t size);

    // Function pointer type for writing a register
    // thread_id: OS thread identifier
    // value: pointer to buffer containing new value (size determined by bitWidth)
    // size: buffer size in bytes (must be >= bitWidth/8)
    // Returns: STATUS_OK on success, error code on failure
    typedef StatusCode (*VertexRegisterWrite_t)(uint32_t thread_id, const void* value, size_t size);

    // Individual register accessor descriptor
    // Provides read/write access to a single register via function pointers
    typedef struct VertexRegisterAccess
    {
        char name[VERTEX_MAX_REGISTER_NAME_LENGTH];  // Register name (e.g., "RAX", "XMM0")
        uint8_t bitWidth;                            // Register width: 8, 16, 32, 64, 128, 256, 512
        RegisterCategory category;                   // Category for UI grouping
        uint32_t registerId;                         // Unique ID (matches RegisterDef.registerId)
        uint16_t flags;                              // RegisterFlags from registry.h
        uint8_t reserved[2];                         // Padding for alignment
        VertexRegisterRead_t read;                   // Function to read this register
        VertexRegisterWrite_t write;                 // Function to write (NULL if read-only)
    } RegisterAccess;

    // Complete register accessor set for an architecture
    // Contains all register accessors and identifies special registers
    typedef struct VertexRegisterAccessSet
    {
        RegisterAccess* registers;                   // Array of register accessors
        uint32_t registerCount;                      // Number of registers in array
        uint32_t instructionPointerRegId;            // registerId of IP/PC register
        uint32_t stackPointerRegId;                  // registerId of SP register
        uint32_t basePointerRegId;                   // registerId of BP/FP register
        uint32_t flagsRegId;                         // registerId of FLAGS/NZCV register
    } RegisterAccessSet;

#ifdef __cplusplus
}
#endif
