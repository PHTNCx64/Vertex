# Vertex Structures

## Core Structures

### `PluginInformation`
Basic information about the plugin to be loaded.

```c
typedef struct VertexPluginInformation
{
    const char* pluginName;
    const char* pluginVersion;
    const char* pluginDescription;
    const char* pluginAuthor;
    uint32_t apiVersion;
    uint64_t featureCapability;
} PluginInformation;
```

### `Runtime`
The function table provided by the Vertex runtime to the plugin. Allows registration of types, architectures, registers, and logging. All function pointers use typedef'd types for clarity.

```c
typedef struct VertexRuntime
{
    VertexLog_t vertex_log_info;
    VertexLog_t vertex_log_error;
    VertexLog_t vertex_log_warn;
    VertexQueueEvent_t vertex_queue_event;

    VertexRegisterDatatype_t vertex_register_datatype;
    VertexUnregisterDatatype_t vertex_unregister_datatype;

    VertexRegisterArchitecture_t vertex_register_architecture;

    VertexRegisterCategory_t vertex_register_category;
    VertexUnregisterCategory_t vertex_unregister_category;

    VertexRegisterRegister_t vertex_register_register;
    VertexUnregisterRegister_t vertex_unregister_register;

    VertexRegisterFlagBit_t vertex_register_flag_bit;

    VertexRegisterExceptionType_t vertex_register_exception_type;

    VertexRegisterCallingConvention_t vertex_register_calling_convention;

    VertexRegisterSnapshot_t vertex_register_snapshot;
    VertexClearRegistry_t vertex_clear_registry;
} Runtime;
```

#### Runtime Function Pointer Types

```c
typedef StatusCode (VERTEX_API *VertexLog_t)(const char* msg, ...);
typedef StatusCode (VERTEX_API *VertexQueueEvent_t)(VertexEvent* evt, void* userData);
typedef StatusCode (VERTEX_API *VertexRegisterDatatype_t)(const DataType* datatype);
typedef StatusCode (VERTEX_API *VertexUnregisterDatatype_t)(const DataType* datatype);
typedef StatusCode (VERTEX_API *VertexRegisterArchitecture_t)(const ArchitectureInfo* archInfo);
typedef StatusCode (VERTEX_API *VertexRegisterCategory_t)(const RegisterCategoryDef* category);
typedef StatusCode (VERTEX_API *VertexUnregisterCategory_t)(const char* categoryId);
typedef StatusCode (VERTEX_API *VertexRegisterRegister_t)(const RegisterDef* reg);
typedef StatusCode (VERTEX_API *VertexUnregisterRegister_t)(const char* registerName);
typedef StatusCode (VERTEX_API *VertexRegisterFlagBit_t)(const FlagBitDef* flagBit);
typedef StatusCode (VERTEX_API *VertexRegisterExceptionType_t)(const ExceptionTypeDef* exceptionType);
typedef StatusCode (VERTEX_API *VertexRegisterCallingConvention_t)(const CallingConventionDef* callingConv);
typedef StatusCode (VERTEX_API *VertexRegisterSnapshot_t)(const RegistrySnapshot* snapshot);
typedef StatusCode (VERTEX_API *VertexClearRegistry_t)();
```

## Process & Modules

### `ProcessInformation`

```c
typedef struct VertexProcessInformation
{
    char processName[VERTEX_MAX_NAME_LENGTH];
    char processOwner[VERTEX_MAX_OWNER_LENGTH];
    uint32_t processId;
} ProcessInformation;
```

Platform-dependent constants:

| Constant                  | Windows | Linux | macOS | BSD  |
|:--------------------------|:--------|:------|:------|:-----|
| `VERTEX_MAX_PATH_LENGTH`  | 260     | 4096  | 1024  | 1024 |
| `VERTEX_MAX_NAME_LENGTH`  | 260     | 255   | 255   | 255  |
| `VERTEX_MAX_OWNER_LENGTH` | 256     | 32    | 255   | 32   |

### `ModuleInformation`

```c
typedef struct VertexModuleInformation
{
    char moduleName[VERTEX_MAX_NAME_LENGTH];
    char modulePath[VERTEX_MAX_PATH_LENGTH];
    uint64_t baseAddress;
    uint64_t size;
} ModuleInformation;
```

### `InjectionMethod`

```c
typedef struct VertexInjectionMethod
{
    char methodName[VERTEX_MAX_NAME_LENGTH];
    StatusCode(VERTEX_API* injectableFunction)(const char* path);
} InjectionMethod;
```

### `ModuleEntry`
Base structure for module imports and exports.

```c
typedef struct VertexModuleEntry
{
    const char* name;
    void* address;
    size_t size;
    int32_t ordinal;
    uint8_t isFunction;    // bool
    uint8_t isImport;      // bool
    uint8_t isForwarder;   // bool
    uint8_t reserved;
    const char* forwarderName;
    void* moduleHandle;
} ModuleEntry;
```

### `ModuleImport`

```c
typedef struct VertexModuleImport
{
    ModuleEntry entry;
    const char* libraryName;
    void* importAddress;
    int32_t hint;
    uint8_t isOrdinal; // bool
    uint8_t reserved[3];
} ModuleImport;
```

### `ModuleExport`

```c
typedef struct VertexModuleExport
{
    ModuleEntry entry;
    const char* moduleName;
    uint8_t isData;  // bool
    uint8_t isThunk; // bool
    uint8_t reserved[2];
    void* relocationTable;
    int32_t characteristics;
} ModuleExport;
```

## Memory Structures

### `MemoryRegion`
Describes a region of memory in the target process.

```c
typedef struct VertexMemoryRegion
{
    const char* baseModuleName;
    uint64_t baseAddress;
    uint64_t regionSize;
} MemoryRegion;
```

### `MemoryAttributeType`

```c
typedef enum VertexMemoryAttributeType : int32_t
{
    VERTEX_PROTECTION = 0,
    VERTEX_STATE,
    VERTEX_TYPE,
    VERTEX_OTHER,
} MemoryAttributeType;
```

### `MemoryAttributeOption`
Used for querying memory regions with specific attributes.

```c
typedef void (*VertexOptionState_t)(uint8_t state);

typedef struct VertexMemoryAttributeOption
{
    const char* memoryAttributeName;
    VertexOptionState_t stateFunction;
    MemoryAttributeType memoryAttributeType;
    uint8_t* currentState; // bool
} MemoryAttributeOption;
```

### `MemoryDataType`

```c
typedef struct VertexMemoryDataType
{
    const char* memoryDataTypeName;
    int64_t memoryDataTypeSize;
    VertexValidateInput_t validateInputFunction;
    uint8_t supportsNumericSystems; // bool
    uint8_t reserved[3];
} MemoryDataType;
```

#### Memory Data Type Function Pointers

```c
typedef StatusCode (*VertexValidateInput_t)(NumericSystem system, const char* input, char* output);
typedef StatusCode (*VertexMatchesEqual_t)(const char* input);
typedef StatusCode (*VertexMatchesLesser_t)(const char* input);
typedef StatusCode (*VertexMatchesGreater_t)(const char* input);
typedef StatusCode (*VertexMatchesBetween_t)(const char* input1, const char* input2);
typedef StatusCode (*VertexMatchesUnknownInitialValue_t)(const char* input);
```

## Dynamic Data Types (Scanning)

### `DataType`
Defines a custom data type for the memory scanner.

```c
typedef struct VertexDataType
{
    const char* typeName;
    size_t valueSize;                // Size in bytes or VERTEX_VARIABLE_LENGTH
    VertexConverter_t converter;
    VertexExtractor_t extractor;
    VertexFormatter_t formatter;
    ScanMode* scanModes;
    size_t scanModeCount;
} DataType;
```

### `ScanMode`

```c
typedef struct VertexScanMode
{
    const char* scanModeName;
    VertexComparator_t comparator;
    uint8_t needsInput;    // bool
    uint8_t needsPrevious; // bool
    uint8_t reserved[2];
} ScanMode;
```

#### Scan Type Function Pointers

```c
typedef StatusCode (*VertexConverter_t)(const char* input, NumericSystem numericBase,
                                        char* output, size_t outputSize, size_t* bytesWritten);
typedef StatusCode (*VertexExtractor_t)(const char* memoryBytes, size_t memorySize,
                                        char* output, size_t outputSize);
typedef StatusCode (*VertexFormatter_t)(const char* extractedValue, char* output, size_t outputSize);
typedef StatusCode (*VertexComparator_t)(const char* currentValue, const char* previousValue,
                                         const char* userInput, uint8_t* result);
```

## Debugger Enumerations

### `DebuggerState`

```c
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
```

### `StepMode`

```c
typedef enum VertexStepMode : int32_t
{
    VERTEX_STEP_INTO = 0,
    VERTEX_STEP_OVER,
    VERTEX_STEP_OUT
} StepMode;
```

### `BreakpointType`

```c
typedef enum VertexBreakpointType : int32_t
{
    VERTEX_BP_EXECUTE = 0,
    VERTEX_BP_READ,
    VERTEX_BP_WRITE,
    VERTEX_BP_READWRITE,
} BreakpointType;
```

### `BreakpointState`

```c
typedef enum VertexBreakpointState : int32_t
{
    VERTEX_BP_STATE_ENABLED = 0,
    VERTEX_BP_STATE_DISABLED,
    VERTEX_BP_STATE_PENDING,
    VERTEX_BP_STATE_ERROR,
} BreakpointState;
```

### `ThreadState`

```c
typedef enum VertexThreadState : int32_t
{
    VERTEX_THREAD_RUNNING = 0,
    VERTEX_THREAD_SUSPENDED,
    VERTEX_THREAD_WAITING,
    VERTEX_THREAD_TERMINATED,
} ThreadState;
```

### `DebugEventType`

```c
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
```

### `RegisterCategory`

```c
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
```

### `ExceptionCode`

```c
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
```

### `SymbolType`

```c
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
```

### `SymbolFlags`

```c
typedef enum VertexSymbolFlags : uint32_t
{
    VERTEX_SYM_FLAG_NONE     = 0,
    VERTEX_SYM_FLAG_EXPORT   = 1 << 0,
    VERTEX_SYM_FLAG_IMPORT   = 1 << 1,
    VERTEX_SYM_FLAG_STATIC   = 1 << 2,
    VERTEX_SYM_FLAG_VIRTUAL  = 1 << 3,
    VERTEX_SYM_FLAG_CONST    = 1 << 4,
    VERTEX_SYM_FLAG_VOLATILE = 1 << 5,
} SymbolFlags;
```

### `ExpressionValueType`

```c
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
```

### `BreakpointConditionType`

```c
typedef enum VertexBreakpointConditionType : int32_t
{
    VERTEX_BP_COND_NONE = 0,
    VERTEX_BP_COND_EXPRESSION,
    VERTEX_BP_COND_HIT_COUNT_EQUAL,
    VERTEX_BP_COND_HIT_COUNT_GREATER,
    VERTEX_BP_COND_HIT_COUNT_MULTIPLE,
} BreakpointConditionType;
```

### `WatchpointType`

```c
typedef enum VertexWatchpointType : int32_t
{
    VERTEX_WP_READ = 0,
    VERTEX_WP_WRITE,
    VERTEX_WP_READWRITE,
    VERTEX_WP_EXECUTE,
} WatchpointType;
```

### `NumericSystem`

```c
typedef enum VertexNumericSystem : int32_t
{
    VERTEX_NONE        = 0,
    VERTEX_BINARY      = 2,
    VERTEX_OCTAL       = 8,
    VERTEX_DECIMAL     = 10,
    VERTEX_HEXADECIMAL = 16,
} NumericSystem;
```

## Debugger Structures

### `Register`

```c
typedef struct VertexRegister
{
    char name[VERTEX_MAX_REGISTER_NAME_LENGTH];
    RegisterCategory category;
    uint64_t value;
    uint64_t previousValue;
    uint8_t bitWidth;
    uint8_t modified; // bool
} Register;
```

### `RegisterSet`

```c
typedef struct VertexRegisterSet
{
    Register registers[VERTEX_MAX_REGISTERS];
    uint32_t registerCount;
    uint64_t instructionPointer;
    uint64_t stackPointer;
    uint64_t basePointer;
    uint64_t flagsRegister;
} RegisterSet;
```

### `StackFrame`

```c
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
```

### `CallStack`

```c
typedef struct VertexCallStack
{
    StackFrame frames[VERTEX_MAX_STACK_FRAMES];
    uint32_t frameCount;
    uint32_t currentFrameIndex;
} CallStack;
```

### `ThreadInfo`

```c
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
```

### `ThreadList`

```c
typedef struct VertexThreadList
{
    ThreadInfo threads[VERTEX_MAX_THREADS];
    uint32_t threadCount;
    uint32_t currentThreadId;
} ThreadList;
```

### `BreakpointInfo`

```c
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
```

### `DebugEvent`

```c
typedef struct VertexDebugEvent
{
    DebugEventType type;
    uint32_t threadId;
    uint64_t address;
    uint32_t exceptionCode;
    uint8_t firstChance; // bool
    char description[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    uint32_t breakpointId;
} DebugEvent;
```

### `ExceptionInfo`

```c
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
```

## Debugger Callback Structures

### `DebuggerCallbacks`
Callback table for the debugger loop.

```c
typedef struct VertexDebuggerCallbacks
{
    // Execution events
    VertexOnBreakpointHit on_breakpoint_hit;
    VertexOnSingleStep on_single_step;
    VertexOnException on_exception;
    VertexOnWatchpointHit on_watchpoint_hit;

    // Thread events
    VertexOnThreadCreated on_thread_created;
    VertexOnThreadExited on_thread_exited;

    // Module events
    VertexOnModuleLoaded on_module_loaded;
    VertexOnModuleUnloaded on_module_unloaded;

    // Process events
    VertexOnProcessExited on_process_exited;

    // Debug output
    VertexOnOutputString on_output_string;

    // State management
    VertexOnStateChanged on_state_changed;
    VertexOnAttached on_attached;
    VertexOnDetached on_detached;

    // Error handling
    VertexOnError on_error;

    void* user_data;
} DebuggerCallbacks;
```

#### Callback Function Pointer Types

```c
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
```

#### Callback Event Structures

### `ThreadEvent`

```c
typedef struct VertexThreadEvent
{
    uint32_t threadId;
    uint64_t entryPoint;      // Start address (valid on create, 0 on exit)
    uint64_t stackBase;       // Thread stack base address
    int32_t exitCode;         // Exit code (valid on exit, 0 on create)
} ThreadEvent;
```

### `ModuleEvent`

```c
typedef struct VertexModuleEvent
{
    char moduleName[VERTEX_MAX_NAME_LENGTH];
    char modulePath[VERTEX_MAX_PATH_LENGTH];
    uint64_t baseAddress;
    uint64_t size;
    uint8_t isMainModule; // bool
    uint8_t reserved[3];
} ModuleEvent;
```

### `OutputStringEvent`

```c
typedef struct VertexOutputStringEvent
{
    uint32_t threadId;
    char message[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
} OutputStringEvent;
```

### `WatchpointEvent`

```c
typedef struct VertexWatchpointEvent
{
    uint32_t breakpointId;
    uint32_t threadId;
    uint64_t address;         // Address that was accessed
    uint64_t accessAddress;   // Instruction that caused the access
    WatchpointType type;
    uint8_t size;             // Size of the access (1, 2, 4, 8 bytes)
} WatchpointEvent;
```

### `DebuggerError`

```c
typedef struct VertexDebuggerError
{
    StatusCode code;
    char message[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    uint8_t isFatal; // bool
    uint8_t reserved[3];
} DebuggerError;
```

## Watchpoint Structures

### `Watchpoint`

```c
typedef struct VertexWatchpoint
{
    WatchpointType type;
    uint64_t address;
    uint32_t size;
    uint8_t active; // bool
    uint8_t reserved[3];
} Watchpoint;
```

### `WatchpointInfo`

```c
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
```

## Breakpoint Condition Structures

### `BreakpointCondition`

```c
typedef struct VertexBreakpointCondition
{
    BreakpointConditionType type;
    char expression[VERTEX_MAX_CONDITION_LENGTH];
    uint32_t hitCountTarget;
    uint8_t enabled; // bool
    uint8_t reserved[3];
} BreakpointCondition;
```

### `BreakpointAction`

```c
typedef struct VertexBreakpointAction
{
    uint8_t logMessage;         // bool
    uint8_t continueExecution;  // bool
    uint8_t playSound;          // bool
    uint8_t reserved;
    char logFormat[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
} BreakpointAction;
```

### `ConditionalBreakpoint`

```c
typedef struct VertexConditionalBreakpoint
{
    uint32_t breakpointId;
    BreakpointCondition condition;
    BreakpointAction action;
} ConditionalBreakpoint;
```

### `HardwareBreakpointStatus`

```c
typedef struct VertexHardwareBreakpointStatus
{
    uint8_t registerInUse[VERTEX_MAX_HW_BREAKPOINTS];
    uint32_t breakpointIds[VERTEX_MAX_HW_BREAKPOINTS];
    uint64_t addresses[VERTEX_MAX_HW_BREAKPOINTS];
    BreakpointType types[VERTEX_MAX_HW_BREAKPOINTS];
    uint8_t sizes[VERTEX_MAX_HW_BREAKPOINTS];
    uint32_t availableCount;
} HardwareBreakpointStatus;
```

## Symbol Resolution Structures

### `SymbolInfo`

```c
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
```

### `SourceLocation`

```c
typedef struct VertexSourceLocation
{
    char fileName[VERTEX_MAX_SOURCE_FILE_LENGTH];
    uint32_t lineNumber;
    uint32_t columnNumber;
    uint64_t address;
    uint64_t endAddress;
} SourceLocation;
```

### `SymbolSearchResult`

```c
typedef struct VertexSymbolSearchResult
{
    SymbolInfo* symbols;
    uint32_t symbolCount;
    uint32_t totalMatches;
    uint8_t hasMore; // bool
    uint8_t reserved[3];
} SymbolSearchResult;
```

### `LineInfo`

```c
typedef struct VertexLineInfo
{
    uint64_t address;
    uint32_t lineNumber;
    uint32_t lineEndNumber;
    uint8_t isStatement; // bool
    uint8_t reserved[3];
} LineInfo;
```

### `SourceFileInfo`

```c
typedef struct VertexSourceFileInfo
{
    char fileName[VERTEX_MAX_SOURCE_FILE_LENGTH];
    char compiledPath[VERTEX_MAX_PATH_LENGTH];
    uint64_t checksum;
    uint32_t lineCount;
    LineInfo* lines;
} SourceFileInfo;
```

## Expression Evaluation Structures

### `ExpressionValue`

```c
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
    uint8_t isValid;       // bool
    uint8_t isReadOnly;    // bool
    uint8_t hasChildren;   // bool
    uint8_t reserved;
} ExpressionValue;
```

### `ExpressionResult`

```c
typedef struct VertexExpressionResult
{
    char expression[VERTEX_MAX_EXPRESSION_LENGTH];
    ExpressionValue value;
    char errorMessage[VERTEX_MAX_EXCEPTION_DESC_LENGTH];
    uint8_t success; // bool
    uint8_t reserved[3];
} ExpressionResult;
```

### `WatchEntry`

```c
typedef struct VertexWatchEntry
{
    uint32_t id;
    char expression[VERTEX_MAX_EXPRESSION_LENGTH];
    ExpressionValue currentValue;
    ExpressionValue previousValue;
    uint8_t enabled;       // bool
    uint8_t valueChanged;  // bool
    uint8_t reserved[2];
} WatchEntry;
```

## Local Variable Structures

### `LocalVariable`

```c
typedef struct VertexLocalVariable
{
    char name[VERTEX_MAX_NAME_LENGTH];
    char typeName[VERTEX_MAX_NAME_LENGTH];
    uint64_t address;
    int32_t stackOffset;
    uint32_t size;
    ExpressionValueType valueType;
    uint8_t isParameter;   // bool
    uint8_t isRegister;    // bool
    uint8_t registerIndex;
    uint8_t reserved;
} LocalVariable;
```

### `LocalVariableList`

```c
typedef struct VertexLocalVariableList
{
    LocalVariable* variables;
    uint32_t variableCount;
    uint32_t frameIndex;
} LocalVariableList;
```

## Runtime Register Access

This API provides runtime register access via function pointers, enabling dynamic architecture switching
(WOW64/native x64/ARM64) without recompilation and a unified interface for all register sizes (8-bit to 512-bit).

### `RegisterAccess`

```c
typedef StatusCode (*VertexRegisterRead_t)(uint32_t thread_id, void* out, size_t size);
typedef StatusCode (*VertexRegisterWrite_t)(uint32_t thread_id, const void* value, size_t size);

typedef struct VertexRegisterAccess
{
    char name[VERTEX_MAX_REGISTER_NAME_LENGTH];
    uint8_t bitWidth;
    RegisterCategory category;
    uint32_t registerId;
    uint16_t flags;         // RegisterFlags from registry.h
    uint8_t reserved[2];
    VertexRegisterRead_t read;
    VertexRegisterWrite_t write;   // NULL if read-only
} RegisterAccess;
```

### `RegisterAccessSet`

```c
typedef struct VertexRegisterAccessSet
{
    RegisterAccess* registers;
    uint32_t registerCount;
    uint32_t instructionPointerRegId;
    uint32_t stackPointerRegId;
    uint32_t basePointerRegId;
    uint32_t flagsRegId;
} RegisterAccessSet;
```

## Registry Structures (Architecture Registration)

### `RegisterFlags`

```c
typedef enum VertexRegisterFlags : uint32_t
{
    VERTEX_REG_FLAG_NONE           = 0,
    VERTEX_REG_FLAG_READONLY       = 1 << 0,
    VERTEX_REG_FLAG_HIDDEN         = 1 << 1,
    VERTEX_REG_FLAG_PROGRAM_COUNTER = 1 << 2,
    VERTEX_REG_FLAG_STACK_POINTER  = 1 << 3,
    VERTEX_REG_FLAG_FRAME_POINTER  = 1 << 4,
    VERTEX_REG_FLAG_FLAGS_REGISTER = 1 << 5,
    VERTEX_REG_FLAG_FLOATING_POINT = 1 << 6,
    VERTEX_REG_FLAG_VECTOR         = 1 << 7,
    VERTEX_REG_FLAG_SEGMENT        = 1 << 8,
} RegisterFlags;
```

### `DisasmSyntax`

```c
typedef enum VertexDisasmSyntax : int32_t
{
    VERTEX_DISASM_SYNTAX_INTEL = 0,
    VERTEX_DISASM_SYNTAX_ATT,
    VERTEX_DISASM_SYNTAX_CUSTOM
} DisasmSyntax;
```

### `Endianness`

```c
typedef enum VertexEndianness : int32_t
{
    VERTEX_ENDIAN_LITTLE = 0,
    VERTEX_ENDIAN_BIG
} Endianness;
```

### `RegisterCategoryDef`

```c
typedef struct VertexRegisterCategoryDef
{
    char categoryId[VERTEX_MAX_CATEGORY_ID_LENGTH];
    char displayName[VERTEX_MAX_CATEGORY_NAME_LENGTH];
    uint32_t displayOrder;
    uint8_t collapsedByDefault; // bool
    uint8_t reserved[3];
} RegisterCategoryDef;
```

### `RegisterDef`

```c
typedef struct VertexRegisterDef
{
    char categoryId[VERTEX_MAX_CATEGORY_ID_LENGTH];
    char name[VERTEX_MAX_REGISTER_NAME_LENGTH];
    char parentName[VERTEX_MAX_REGISTER_NAME_LENGTH];
    uint8_t bitWidth;
    uint8_t bitOffset;
    uint16_t flags;          // RegisterFlags
    uint32_t displayOrder;
    uint32_t registerId;
    void(VERTEX_API* write_func)(void* in, size_t size);
    void(VERTEX_API* read_func)(void* out, size_t size);
} RegisterDef;
```

### `FlagBitDef`

```c
typedef struct VertexFlagBitDef
{
    char flagsRegisterName[VERTEX_MAX_REGISTER_NAME_LENGTH];
    char bitName[VERTEX_MAX_FLAG_NAME_LENGTH];
    char description[VERTEX_MAX_FLAG_DESC_LENGTH];
    uint8_t bitPosition;
    uint8_t reserved[3];
} FlagBitDef;
```

### `ArchitectureInfo`

```c
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
```

### `ExceptionTypeDef`

```c
typedef struct VertexExceptionTypeDef
{
    uint32_t exceptionCode;
    char name[32];
    char description[128];
    uint8_t isFatal; // bool
    uint8_t reserved[3];
} ExceptionTypeDef;
```

### `CallingConventionDef`

```c
typedef struct VertexCallingConventionDef
{
    char name[32];
    char parameterRegisters[8][VERTEX_MAX_REGISTER_NAME_LENGTH];
    uint8_t parameterRegisterCount;
    char returnRegister[VERTEX_MAX_REGISTER_NAME_LENGTH];
    uint8_t stackCleanup;
    uint8_t reserved[2];
} CallingConventionDef;
```

### `RegistrySnapshot`
Used for bulk registration of architecture metadata.

```c
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
```

## Disassembler Structures

### `InstructionCategory`

```c
typedef enum VertexInstructionCategory : int32_t
{
    VERTEX_INSTRUCTION_UNKNOWN        = 0,
    VERTEX_INSTRUCTION_ARITHMETIC     = 1,
    VERTEX_INSTRUCTION_LOGIC          = 2,
    VERTEX_INSTRUCTION_DATA_TRANSFER  = 3,
    VERTEX_INSTRUCTION_CONTROL_FLOW   = 4,
    VERTEX_INSTRUCTION_COMPARISON     = 5,
    VERTEX_INSTRUCTION_STRING         = 6,
    VERTEX_INSTRUCTION_SYSTEM         = 7,
    VERTEX_INSTRUCTION_FLOATING_POINT = 8,
    VERTEX_INSTRUCTION_SIMD           = 9,
    VERTEX_INSTRUCTION_CRYPTO         = 10,
    VERTEX_INSTRUCTION_PRIVILEGED     = 11
} InstructionCategory;
```

### `BranchType`

```c
typedef enum VertexBranchType : int32_t
{
    VERTEX_BRANCH_NONE              = 0,
    VERTEX_BRANCH_UNCONDITIONAL     = 1,
    VERTEX_BRANCH_CONDITIONAL       = 2,
    VERTEX_BRANCH_CALL              = 3,
    VERTEX_BRANCH_RETURN            = 4,
    VERTEX_BRANCH_LOOP              = 5,
    VERTEX_BRANCH_INTERRUPT         = 6,
    VERTEX_BRANCH_EXCEPTION         = 7,
    VERTEX_BRANCH_INDIRECT_JUMP     = 8,
    VERTEX_BRANCH_INDIRECT_CALL     = 9,
    VERTEX_BRANCH_CONDITIONAL_MOVE  = 10,
    VERTEX_BRANCH_TABLE_SWITCH      = 11
} BranchType;
```

### `BranchDirection`

```c
typedef enum VertexBranchDirection : int32_t
{
    VERTEX_DIRECTION_NONE          = 0,
    VERTEX_DIRECTION_FORWARD       = 1,
    VERTEX_DIRECTION_BACKWARD      = 2,
    VERTEX_DIRECTION_SELF          = 3,
    VERTEX_DIRECTION_OUT_OF_BLOCK  = 4,
    VERTEX_DIRECTION_OUT_OF_FUNC   = 5,
    VERTEX_DIRECTION_EXTERNAL      = 6,
    VERTEX_DIRECTION_UNKNOWN       = 7
} BranchDirection;
```

### `InstructionFlags`

```c
typedef enum VertexInstructionFlags : uint32_t
{
    VERTEX_FLAG_NONE           = 0x00000000,
    VERTEX_FLAG_BRANCH         = 0x00000001,
    VERTEX_FLAG_CONDITIONAL    = 0x00000002,
    VERTEX_FLAG_CALL           = 0x00000004,
    VERTEX_FLAG_RETURN         = 0x00000008,
    VERTEX_FLAG_PRIVILEGED     = 0x00000010,
    VERTEX_FLAG_MEMORY_READ    = 0x00000020,
    VERTEX_FLAG_MEMORY_WRITE   = 0x00000040,
    VERTEX_FLAG_STACK_OP       = 0x00000080,
    VERTEX_FLAG_INDIRECT       = 0x00000100,
    VERTEX_FLAG_CRYPTO         = 0x00000200,
    VERTEX_FLAG_DANGEROUS      = 0x00000400,
    VERTEX_FLAG_BREAKPOINT     = 0x00000800,
    VERTEX_FLAG_ANALYZED       = 0x00001000,
    VERTEX_FLAG_PATCHED        = 0x00002000,
    VERTEX_FLAG_ENTRY_POINT    = 0x00004000,
    VERTEX_FLAG_HOT_PATH       = 0x00008000
} InstructionFlags;
```

### `DisassemblerResult`

```c
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
```

### `DisassemblerResults`

```c
typedef struct VertexDisassemblerResults
{
    DisassemblerResult* results;
    uint32_t count;
    uint32_t capacity;
    uint64_t startAddress;
    uint64_t endAddress;
    uint32_t totalSize;
} DisassemblerResults;
```

### `XReference`

```c
typedef struct VertexXReference
{
    uint64_t fromAddress;
    uint64_t toAddress;
    char fromSymbol[VERTEX_MAX_SYMBOL_LENGTH];
    char toSymbol[VERTEX_MAX_SYMBOL_LENGTH];
    uint32_t xrefType;
} XReference;
```

## Event Data Structures

### `Event` (General Events)

```c
typedef enum VertexEvent : int32_t
{
    VERTEX_PROCESS_OPENED,
    VERTEX_PROCESS_CLOSED,
    VERTEX_PROCESS_KILLED,
    VERTEX_ERROR_OCCURRED,

    VERTEX_DEBUGGER_ATTACHED,
    VERTEX_DEBUGGER_DETACHED,
    VERTEX_DEBUGGER_BREAKPOINT_HIT,
    VERTEX_DEBUGGER_STEP_COMPLETE,
    VERTEX_DEBUGGER_EXCEPTION,
} Event;
```

### `ProcessEventData`

```c
typedef struct VertexProcessEventData
{
    uint32_t processId;
    void* processHandle; // Platform-specific handle (HANDLE on Windows)
} ProcessEventData;
```

### `BreakpointEventData`

```c
typedef struct VertexBreakpointEventData
{
    uint32_t breakpointId;
    uint64_t address;
    uint32_t threadId;
} BreakpointEventData;
```

### `ExceptionEventData`

```c
typedef struct VertexExceptionEventData
{
    uint32_t exceptionCode;
    uint64_t address;
    uint32_t threadId;
    uint8_t firstChance; // bool
    uint8_t reserved[3];
} ExceptionEventData;
```

## Constants Reference

### Debugger Constants

| Constant                           | Value      |
|:-----------------------------------|:-----------|
| `VERTEX_MAX_REGISTER_NAME_LENGTH`  | 16         |
| `VERTEX_MAX_FUNCTION_NAME_LENGTH`  | 256        |
| `VERTEX_MAX_SOURCE_FILE_LENGTH`    | 512        |
| `VERTEX_MAX_REGISTERS`             | 128        |
| `VERTEX_MAX_STACK_FRAMES`          | 256        |
| `VERTEX_MAX_THREADS`               | 256        |
| `VERTEX_MAX_BREAKPOINTS`           | 1024       |
| `VERTEX_MAX_EXCEPTION_DESC_LENGTH` | 512        |
| `VERTEX_MAX_SYMBOL_NAME_LENGTH`    | 512        |
| `VERTEX_MAX_EXPRESSION_LENGTH`     | 1024       |
| `VERTEX_MAX_CONDITION_LENGTH`      | 256        |
| `VERTEX_MAX_SYMBOLS`               | 4096       |
| `VERTEX_MAX_HW_BREAKPOINTS`        | 4          |
| `VERTEX_INFINITE_WAIT`             | 0xFFFFFFFF |

### Memory Constants

| Constant                   | Value |
|:---------------------------|:------|
| `VERTEX_VARIABLE_LENGTH`   | 0     |
| `VERTEX_MAX_STRING_LENGTH` | 255   |

### Disassembler Constants

| Constant                     | Value |
|:-----------------------------|:------|
| `VERTEX_MAX_MNEMONIC_LENGTH` | 32    |
| `VERTEX_MAX_OPERANDS_LENGTH` | 128   |
| `VERTEX_MAX_COMMENT_LENGTH`  | 256   |
| `VERTEX_MAX_BYTES_LENGTH`    | 16    |
| `VERTEX_MAX_SYMBOL_LENGTH`   | 64    |
| `VERTEX_MAX_SECTION_LENGTH`  | 32    |

### Registry Constants

| Constant                            | Value |
|:------------------------------------|:------|
| `VERTEX_MAX_CATEGORY_ID_LENGTH`     | 32    |
| `VERTEX_MAX_CATEGORY_NAME_LENGTH`   | 64    |
| `VERTEX_MAX_FLAG_NAME_LENGTH`       | 16    |
| `VERTEX_MAX_FLAG_DESC_LENGTH`       | 128   |
| `VERTEX_MAX_CATEGORIES`             | 32    |
| `VERTEX_MAX_REGISTERS_PER_CATEGORY` | 64    |
| `VERTEX_MAX_FLAG_BITS`              | 64    |
| `VERTEX_MAX_MEMORY_TYPES`           | 32    |
| `VERTEX_MAX_EXCEPTION_TYPES`        | 64    |

### Helper Macros

```c
#define VERTEX_IS_BRANCH(result)      ((result)->flags & (VERTEX_FLAG_BRANCH | VERTEX_FLAG_CALL))
#define VERTEX_IS_MEMORY_OP(result)   ((result)->flags & (VERTEX_FLAG_MEMORY_READ | VERTEX_FLAG_MEMORY_WRITE))
#define VERTEX_IS_CONDITIONAL(result) ((result)->flags & VERTEX_FLAG_CONDITIONAL)
#define VERTEX_IS_DANGEROUS(result)   ((result)->flags & (VERTEX_FLAG_DANGEROUS | VERTEX_FLAG_PRIVILEGED))
```
