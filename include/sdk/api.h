//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include <stdint.h>

#include "statuscode.h"
#include "debugger.h"
#include "disassembler.h"
#include "event.h"
#include "macro.h"
#include "memory.h"
#include "process.h"
#include "registry.h"
#include "ui.h"
#include "feature.hh"

#ifdef __cplusplus
extern "C" {
#endif

#define VERTEX_MAJOR_API_VERSION 0
#define VERTEX_MINOR_API_VERSION 1
#define VERTEX_PATCH_API_VERSION 0

#define VERTEX_TARGET_API_VERSION(major, minor, patch) (uint32_t)(((major << 24) | (minor << 16) | (patch << 8)))

    typedef struct VertexPluginInformation
    {
        const char* pluginName;
        const char* pluginVersion;
        const char* pluginDescription;
        const char* pluginAuthor;
        uint32_t apiVersion;
        uint64_t featureCapability;
    } PluginInformation;

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

        VertexRegisterUIPanel_t vertex_register_ui_panel;
        VertexGetUIValue_t vertex_get_ui_value;
    } Runtime;

    // ===============================================================================================================//
    // ENTRY AND EXIT POINT OF A PLUGIN                                                                               //
    // ===============================================================================================================//
    VERTEX_EXPORT StatusCode VERTEX_API vertex_init(PluginInformation* pluginInfo, Runtime* runtime, bool singleThreadModeInit);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_exit();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_event(Event event, const void* data);

    // ===============================================================================================================//
    // PROCESS RELATED API FUNCTIONS                                                                                  //
    // ===============================================================================================================//
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open(uint32_t processId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_close();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_kill();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_executable_extensions(char** extensions, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_library_extensions(char** extensions, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const char* argv);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_modules_list(ModuleInformation** list, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_injection_methods(InjectionMethod** methods, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_is_valid();

    // Module Import/Export Resolution API
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_imports(const ModuleInformation* module, ModuleImport** imports, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_exports(const ModuleInformation* module, ModuleExport** exports, uint32_t* count);

    // ===============================================================================================================//
    // PROCESS MEMORY RELATED API FUNCTIONS                                                                           //
    // ===============================================================================================================//
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_read_process(uint64_t address, uint64_t size, char* buffer);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process(uint64_t address, uint64_t size, const char* buffer);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_allocate(uint64_t address, uint64_t size, const MemoryAttributeOption** protection, size_t attributeSize, uint64_t* targetAddress);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_change_protection(uint64_t address, uint64_t size, MemoryAttributeOption option);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, uint64_t* size);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_construct_attribute_filters(MemoryAttributeOption** options, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_process_pointer_size(uint64_t* size);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_free(uint64_t address, uint64_t size);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_min_process_address(uint64_t* address);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_max_process_address(uint64_t* address);

    // ===============================================================================================================//
    // PROCESS DISASSEMBLY API FUNCTIONS                                                                              //
    // ===============================================================================================================//
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_disassemble_range(uint64_t address, uint32_t size, DisassemblerResults* results);

    // ===============================================================================================================//
    // DEBUGGER API FUNCTIONS                                                                                         //
    // ===============================================================================================================//

    // ---------------------------------------------------------------------------------------------------------------
    // Debugger Lifecycle
    // ---------------------------------------------------------------------------------------------------------------

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_attach();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_detach();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run(const DebuggerCallbacks* callbacks);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_request_stop();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_state(DebuggerState* state);

    // ---------------------------------------------------------------------------------------------------------------
    // Execution Control (Thread-safe)
    // ---------------------------------------------------------------------------------------------------------------

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_continue(uint8_t passException);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_pause();
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_step(StepMode mode);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run_to_address(uint64_t address);

    // Breakpoints
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint(uint64_t address, BreakpointType type, uint32_t* breakpointId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_breakpoint(uint32_t breakpointId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_breakpoint(uint32_t breakpointId, uint8_t enable);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_watchpoint(const Watchpoint* watchpoint, uint32_t* watchpointId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_watchpoint(uint32_t watchpointId, uint8_t enable);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoints(BreakpointInfo** breakpoints, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_watchpoint(uint32_t watchpointId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoints(WatchpointInfo** watchpoints, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoint_hit_count(uint32_t watchpointId, uint32_t* hitCount);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_reset_watchpoint_hit_count(uint32_t watchpointId);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_threads(ThreadList* threadList);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_current_thread(uint32_t* threadId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_thread_priority_value_to_string(int32_t priority, char** out, size_t* outSize);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_suspend_thread(uint32_t threadId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_resume_thread(uint32_t threadId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_registers(uint32_t threadId, RegisterSet* registers);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_call_stack(uint32_t threadId, const CallStack* callStack);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_exception_info(const ExceptionInfo* exception);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_instruction_pointer(uint32_t threadId, uint64_t* address);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_instruction_pointer(uint32_t threadId, uint64_t address);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_read_register(uint32_t threadId, const char* name, void* out, size_t size);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_write_register(uint32_t threadId, const char* name, const void* in, size_t size);

    // ---------------------------------------------------------------------------------------------------------------
    // Conditional Breakpoints
    // ---------------------------------------------------------------------------------------------------------------

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint_condition(uint32_t breakpointId, const BreakpointCondition* condition);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoint_condition(uint32_t breakpointId, BreakpointCondition* condition);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_clear_breakpoint_condition(uint32_t breakpointId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint_action(uint32_t breakpointId, const BreakpointAction* action);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoint_action(uint32_t breakpointId, BreakpointAction* action);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_reset_hit_count(uint32_t breakpointId);

    // ===============================================================================================================//
    // SYMBOL RESOLUTION API FUNCTIONS                                                                                //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_load_for_module(uint64_t moduleBase);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_unload_for_module(uint64_t moduleBase);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_load_from_file(const char* symbolPath, uint64_t moduleBase);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_set_search_path(const char* searchPath);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_get_search_path(char* searchPath, size_t size);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_from_address(uint64_t address, SymbolInfo* symbol);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_from_name(const char* name, const char* moduleName, SymbolInfo* symbol);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_search(const char* pattern, const char* moduleName, uint32_t maxResults, SymbolSearchResult* result);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_free_search_result(SymbolSearchResult* result);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_get_source_location(uint64_t address, SourceLocation* location);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_get_address_from_line(const char* fileName, uint32_t lineNumber, uint64_t* address);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_get_source_file_info(const char* fileName, SourceFileInfo* info);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_free_source_file_info(SourceFileInfo* info);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_enumerate_functions(uint64_t moduleBase, SymbolInfo** functions, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_symbol_free_enumeration(SymbolInfo* symbols);

    // ===============================================================================================================//
    // EXPRESSION EVALUATION API FUNCTIONS                                                                            //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_evaluate(uint32_t threadId, uint32_t frameIndex, const char* expression, ExpressionResult* result);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_evaluate_async(uint32_t threadId, uint32_t frameIndex, const char* expression, uint32_t* requestId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_get_async_result(uint32_t requestId, ExpressionResult* result);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_cancel_async(uint32_t requestId);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_get_children(uint32_t threadId, uint32_t frameIndex, const char* expression, uint32_t startIndex, uint32_t count, ExpressionResult** children, uint32_t* childCount);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_free_children(ExpressionResult* children, uint32_t count);

    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_set_value(uint32_t threadId, uint32_t frameIndex, const char* expression, const char* newValue);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_expression_get_type_info(uint32_t threadId, uint32_t frameIndex, const char* expression, char* typeInfo, size_t size);

    // ===============================================================================================================//
    // WATCH VARIABLE API FUNCTIONS                                                                                   //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_add(const char* expression, uint32_t* watchId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_remove(uint32_t watchId);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_enable(uint32_t watchId, uint8_t enable);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_get_value(uint32_t watchId, uint32_t threadId, uint32_t frameIndex, WatchEntry* entry);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_get_all(uint32_t threadId, uint32_t frameIndex, WatchEntry** entries, uint32_t* count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_free_entries(WatchEntry* entries, uint32_t count);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_watch_update_all(uint32_t threadId, uint32_t frameIndex);

    // ===============================================================================================================//
    // LOCAL VARIABLE API FUNCTIONS                                                                                   //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_local_variables(uint32_t threadId, uint32_t frameIndex, LocalVariableList* locals);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_free_local_variables(LocalVariableList* locals);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_local_variable_value(uint32_t threadId, uint32_t frameIndex, const char* name, ExpressionValue* value);
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_local_variable_value(uint32_t threadId, uint32_t frameIndex, const char* name, const void* value, size_t size);

#ifdef  __cplusplus
    }
#endif
