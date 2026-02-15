# Vertex API Functions

The following functions are exported by the Vertex SDK for use by plugins.

**Thread Safety:** Unless otherwise noted, assume functions may be called from any thread. See [General.md](General.md) for full thread-safety guidelines.

## Plugin Lifecycle

### `vertex_init`
The entry point of the plugin. Called by Vertex when the plugin is loaded. Called on the main application thread.

```c
StatusCode VERTEX_API vertex_init(PluginInformation* pluginInfo, Runtime* runtime);
```

### `vertex_exit`
Called by Vertex when the plugin is unloaded. Called on the main application thread.

```c
StatusCode VERTEX_API vertex_exit();
```

### `vertex_event`
Called when an event occurs that the plugin might be interested in. May be invoked from different threads depending on the event source.

```c
StatusCode VERTEX_API vertex_event(Event event, const void* data);
```

## Process Management

### `vertex_process_open`
Attaches to an existing process using its PID.

```c
StatusCode VERTEX_API vertex_process_open(uint32_t process_id);
```

### `vertex_process_close`
Detaches from the currently opened process.

```c
StatusCode VERTEX_API vertex_process_close();
```

### `vertex_process_kill`
Terminates the currently opened process.

```c
StatusCode VERTEX_API vertex_process_kill();
```

### `vertex_process_open_new`
Launches a new process and attaches to it.

```c
StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const char* argv);
```

| Parameter | Description |
|:---|:---|
| `process_path` | Path to the executable to launch. |
| `argv` | Command-line arguments for the new process. |

### `vertex_process_get_list`
Retrieves a list of running processes.

```c
StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count);
```

### `vertex_process_get_modules_list`
Retrieves a list of modules loaded in the current process.

```c
StatusCode VERTEX_API vertex_process_get_modules_list(ModuleInformation** list, uint32_t* count);
```

### `vertex_process_get_extensions`
Retrieves a list of file extensions supported by the plugin.

```c
StatusCode VERTEX_API vertex_process_get_extensions(char** extensions, uint32_t* count);
```

### `vertex_process_get_injection_methods`
Retrieves the injection methods supported by the plugin.

```c
StatusCode VERTEX_API vertex_process_get_injection_methods(InjectionMethod** methods);
```

### `vertex_process_is_valid`
Checks whether the currently opened process is still valid.

```c
StatusCode VERTEX_API vertex_process_is_valid();
```

### `vertex_process_get_module_imports`
Retrieves the import table of a specific module.

```c
StatusCode VERTEX_API vertex_process_get_module_imports(const ModuleInformation* module, ModuleImport** imports, uint32_t* count);
```

### `vertex_process_get_module_exports`
Retrieves the export table of a specific module.

```c
StatusCode VERTEX_API vertex_process_get_module_exports(const ModuleInformation* module, ModuleExport** exports, uint32_t* count);
```

## Memory Operations

### `vertex_memory_read_process`
Reads memory from the target process.

```c
StatusCode VERTEX_API vertex_memory_read_process(uint64_t address, uint64_t size, char* buffer);
```

### `vertex_memory_write_process`
Writes memory to the target process.

```c
StatusCode VERTEX_API vertex_memory_write_process(uint64_t address, uint64_t size, const char* buffer);
```

### `vertex_memory_allocate`
Allocates memory in the target process.

```c
StatusCode VERTEX_API vertex_memory_allocate(uint64_t address, uint64_t size, const MemoryAttributeOption** protection, size_t attributeSize, uint64_t* targetAddress);
```

### `vertex_memory_free`
Frees previously allocated memory in the target process.

```c
StatusCode VERTEX_API vertex_memory_free(uint64_t address, uint64_t size);
```

### `vertex_memory_change_protection`
Changes the protection flags of a memory region.

```c
StatusCode VERTEX_API vertex_memory_change_protection(uint64_t address, uint64_t size, MemoryAttributeOption option);
```

### `vertex_memory_query_regions`
Queries the memory regions of the target process.

```c
StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, uint64_t* size);
```

### `vertex_memory_construct_attribute_filters`
Constructs the available memory attribute filter options.

```c
StatusCode VERTEX_API vertex_memory_construct_attribute_filters(MemoryAttributeOption** options, uint32_t* count);
```

### `vertex_memory_get_process_pointer_size`
Retrieves the pointer size of the target process (4 or 8 bytes).

```c
StatusCode VERTEX_API vertex_memory_get_process_pointer_size(uint64_t* size);
```

### `vertex_memory_get_min_process_address`
Retrieves the minimum valid process address.

```c
StatusCode VERTEX_API vertex_memory_get_min_process_address(uint64_t* address);
```

### `vertex_memory_get_max_process_address`
Retrieves the maximum valid process address.

```c
StatusCode VERTEX_API vertex_memory_get_max_process_address(uint64_t* address);
```

## Disassembly

### `vertex_process_disassemble_range`
Disassembles a range of instructions starting at the given address.

```c
StatusCode VERTEX_API vertex_process_disassemble_range(uint64_t address, uint32_t size, DisassemblerResults* results);
```

## Debugger Functions

### Debugger Lifecycle

### `vertex_debugger_attach`
Attaches the debugger to the currently opened process.

```c
StatusCode VERTEX_API vertex_debugger_attach();
```

### `vertex_debugger_detach`
Detaches the debugger from the process.

```c
StatusCode VERTEX_API vertex_debugger_detach();
```

### `vertex_debugger_run`
Starts the main debugger loop. This function blocks until the debugger stops. Runs on a dedicated thread spawned by the runtime.

```c
StatusCode VERTEX_API vertex_debugger_run(const DebuggerCallbacks* callbacks);
```

### `vertex_debugger_request_stop`
Requests the debugger loop to stop. Thread-safe.

```c
StatusCode VERTEX_API vertex_debugger_request_stop();
```

### `vertex_debugger_get_state`
Retrieves the current debugger state.

```c
StatusCode VERTEX_API vertex_debugger_get_state(DebuggerState* state);
```

### Execution Control

These functions are thread-safe and can be called from any thread (e.g., UI thread while debugger runs on its own thread).

### `vertex_debugger_continue`
Continues execution of the debugged process.

```c
StatusCode VERTEX_API vertex_debugger_continue(uint8_t passException);
```

| Parameter | Description |
|:---|:---|
| `passException` | If non-zero, passes the current exception to the debuggee. |

### `vertex_debugger_pause`
Pauses (breaks into) the debugged process.

```c
StatusCode VERTEX_API vertex_debugger_pause();
```

### `vertex_debugger_step`
Steps execution (single step, step over, step out).

```c
StatusCode VERTEX_API vertex_debugger_step(StepMode mode);
```

### `vertex_debugger_run_to_address`
Continues execution until the specified address is reached.

```c
StatusCode VERTEX_API vertex_debugger_run_to_address(uint64_t address);
```

### Breakpoints

### `vertex_debugger_set_breakpoint`
Sets a breakpoint at the specified address.

```c
StatusCode VERTEX_API vertex_debugger_set_breakpoint(uint64_t address, BreakpointType type, uint32_t* breakpointId);
```

### `vertex_debugger_remove_breakpoint`
Removes a breakpoint by its ID.

```c
StatusCode VERTEX_API vertex_debugger_remove_breakpoint(uint32_t breakpointId);
```

### `vertex_debugger_enable_breakpoint`
Enables or disables a breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_enable_breakpoint(uint32_t breakpointId, uint8_t enable);
```

### `vertex_debugger_get_breakpoints`
Retrieves all current breakpoints.

```c
StatusCode VERTEX_API vertex_debugger_get_breakpoints(BreakpointInfo** breakpoints, uint32_t* count);
```

### Watchpoints

### `vertex_debugger_set_watchpoint`
Sets a hardware watchpoint (data breakpoint).

```c
StatusCode VERTEX_API vertex_debugger_set_watchpoint(const Watchpoint* watchpoint, uint32_t* watchpointId);
```

### `vertex_debugger_remove_watchpoint`
Removes a watchpoint by its ID.

```c
StatusCode VERTEX_API vertex_debugger_remove_watchpoint(uint32_t watchpointId);
```

### `vertex_debugger_enable_watchpoint`
Enables or disables a watchpoint.

```c
StatusCode VERTEX_API vertex_debugger_enable_watchpoint(uint32_t watchpointId, uint8_t enable);
```

### `vertex_debugger_get_watchpoints`
Retrieves all current watchpoints.

```c
StatusCode VERTEX_API vertex_debugger_get_watchpoints(WatchpointInfo** watchpoints, uint32_t* count);
```

### `vertex_debugger_get_watchpoint_hit_count`
Retrieves the hit count for a watchpoint.

```c
StatusCode VERTEX_API vertex_debugger_get_watchpoint_hit_count(uint32_t watchpointId, uint32_t* hitCount);
```

### `vertex_debugger_reset_watchpoint_hit_count`
Resets the hit count for a watchpoint.

```c
StatusCode VERTEX_API vertex_debugger_reset_watchpoint_hit_count(uint32_t watchpointId);
```

### Threads

### `vertex_debugger_get_threads`
Retrieves the list of threads in the debugged process.

```c
StatusCode VERTEX_API vertex_debugger_get_threads(ThreadList* threadList);
```

### `vertex_debugger_get_current_thread`
Retrieves the ID of the currently active thread.

```c
StatusCode VERTEX_API vertex_debugger_get_current_thread(const uint32_t* threadId);
```

### `vertex_debugger_suspend_thread`
Suspends a thread by its ID.

```c
StatusCode VERTEX_API vertex_debugger_suspend_thread(uint32_t threadId);
```

### `vertex_debugger_resume_thread`
Resumes a suspended thread by its ID.

```c
StatusCode VERTEX_API vertex_debugger_resume_thread(uint32_t threadId);
```

### `vertex_debugger_thread_priority_value_to_string`
Converts a thread priority value to a human-readable string.

```c
StatusCode VERTEX_API vertex_debugger_thread_priority_value_to_string(int32_t priority, char** out, size_t* outSize);
```

### Registers

### `vertex_debugger_get_registers`
Retrieves the full register set for a specific thread.

```c
StatusCode VERTEX_API vertex_debugger_get_registers(uint32_t threadId, RegisterSet* registers);
```

### `vertex_debugger_read_register`
Reads a single register by name. Supports registers of any width (8-bit to 512-bit).

```c
StatusCode VERTEX_API vertex_debugger_read_register(uint32_t threadId, const char* name, void* out, size_t size);
```

### `vertex_debugger_write_register`
Writes a value to a single register by name.

```c
StatusCode VERTEX_API vertex_debugger_write_register(uint32_t threadId, const char* name, const void* in, size_t size);
```

### `vertex_debugger_get_instruction_pointer`
Retrieves the instruction pointer (RIP/EIP/PC) for a thread.

```c
StatusCode VERTEX_API vertex_debugger_get_instruction_pointer(uint32_t threadId, uint64_t* address);
```

### `vertex_debugger_set_instruction_pointer`
Sets the instruction pointer for a thread.

```c
StatusCode VERTEX_API vertex_debugger_set_instruction_pointer(uint32_t threadId, uint64_t address);
```

### Call Stack & Exceptions

### `vertex_debugger_get_call_stack`
Retrieves the call stack for a specific thread.

```c
StatusCode VERTEX_API vertex_debugger_get_call_stack(uint32_t threadId, const CallStack* callStack);
```

### `vertex_debugger_get_exception_info`
Retrieves information about the current exception.

```c
StatusCode VERTEX_API vertex_debugger_get_exception_info(const ExceptionInfo* exception);
```

### Conditional Breakpoints

### `vertex_debugger_set_breakpoint_condition`
Sets a condition on an existing breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_set_breakpoint_condition(uint32_t breakpointId, const BreakpointCondition* condition);
```

### `vertex_debugger_get_breakpoint_condition`
Retrieves the condition set on a breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_get_breakpoint_condition(uint32_t breakpointId, BreakpointCondition* condition);
```

### `vertex_debugger_clear_breakpoint_condition`
Clears the condition from a breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_clear_breakpoint_condition(uint32_t breakpointId);
```

### `vertex_debugger_set_breakpoint_action`
Sets an action to perform when a breakpoint is hit (e.g., log message, continue execution).

```c
StatusCode VERTEX_API vertex_debugger_set_breakpoint_action(uint32_t breakpointId, const BreakpointAction* action);
```

### `vertex_debugger_get_breakpoint_action`
Retrieves the action set on a breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_get_breakpoint_action(uint32_t breakpointId, BreakpointAction* action);
```

### `vertex_debugger_reset_hit_count`
Resets the hit count for a breakpoint.

```c
StatusCode VERTEX_API vertex_debugger_reset_hit_count(uint32_t breakpointId);
```

## Symbol Resolution

### `vertex_symbol_load_for_module`
Loads symbols for a module identified by its base address.

```c
StatusCode VERTEX_API vertex_symbol_load_for_module(uint64_t moduleBase);
```

### `vertex_symbol_unload_for_module`
Unloads symbols for a module.

```c
StatusCode VERTEX_API vertex_symbol_unload_for_module(uint64_t moduleBase);
```

### `vertex_symbol_load_from_file`
Loads symbols from a specific symbol file (e.g., PDB).

```c
StatusCode VERTEX_API vertex_symbol_load_from_file(const char* symbolPath, uint64_t moduleBase);
```

### `vertex_symbol_set_search_path`
Sets the symbol search path (e.g., for Microsoft Symbol Server).

```c
StatusCode VERTEX_API vertex_symbol_set_search_path(const char* searchPath);
```

### `vertex_symbol_get_search_path`
Retrieves the current symbol search path.

```c
StatusCode VERTEX_API vertex_symbol_get_search_path(char* searchPath, size_t size);
```

### `vertex_symbol_from_address`
Resolves a symbol from an address.

```c
StatusCode VERTEX_API vertex_symbol_from_address(uint64_t address, SymbolInfo* symbol);
```

### `vertex_symbol_from_name`
Resolves a symbol from its name, optionally scoped to a module.

```c
StatusCode VERTEX_API vertex_symbol_from_name(const char* name, const char* moduleName, SymbolInfo* symbol);
```

### `vertex_symbol_search`
Searches for symbols matching a pattern.

```c
StatusCode VERTEX_API vertex_symbol_search(const char* pattern, const char* moduleName, uint32_t maxResults, SymbolSearchResult* result);
```

### `vertex_symbol_free_search_result`
Frees memory allocated by a symbol search.

```c
StatusCode VERTEX_API vertex_symbol_free_search_result(SymbolSearchResult* result);
```

### `vertex_symbol_get_source_location`
Retrieves source file and line information for an address.

```c
StatusCode VERTEX_API vertex_symbol_get_source_location(uint64_t address, SourceLocation* location);
```

### `vertex_symbol_get_address_from_line`
Resolves an address from a source file name and line number.

```c
StatusCode VERTEX_API vertex_symbol_get_address_from_line(const char* fileName, uint32_t lineNumber, uint64_t* address);
```

### `vertex_symbol_get_source_file_info`
Retrieves detailed information about a source file.

```c
StatusCode VERTEX_API vertex_symbol_get_source_file_info(const char* fileName, SourceFileInfo* info);
```

### `vertex_symbol_free_source_file_info`
Frees memory allocated by source file info retrieval.

```c
StatusCode VERTEX_API vertex_symbol_free_source_file_info(SourceFileInfo* info);
```

### `vertex_symbol_enumerate_functions`
Enumerates all functions in a module.

```c
StatusCode VERTEX_API vertex_symbol_enumerate_functions(uint64_t moduleBase, SymbolInfo** functions, uint32_t* count);
```

### `vertex_symbol_free_enumeration`
Frees memory allocated by a symbol enumeration.

```c
StatusCode VERTEX_API vertex_symbol_free_enumeration(SymbolInfo* symbols);
```

## Expression Evaluation

### `vertex_expression_evaluate`
Evaluates an expression in the context of a specific thread and stack frame.

```c
StatusCode VERTEX_API vertex_expression_evaluate(uint32_t threadId, uint32_t frameIndex, const char* expression, ExpressionResult* result);
```

### `vertex_expression_evaluate_async`
Starts asynchronous evaluation of an expression. Returns a request ID for later retrieval.

```c
StatusCode VERTEX_API vertex_expression_evaluate_async(uint32_t threadId, uint32_t frameIndex, const char* expression, uint32_t* requestId);
```

### `vertex_expression_get_async_result`
Retrieves the result of an asynchronous expression evaluation.

```c
StatusCode VERTEX_API vertex_expression_get_async_result(uint32_t requestId, ExpressionResult* result);
```

### `vertex_expression_cancel_async`
Cancels a pending asynchronous expression evaluation.

```c
StatusCode VERTEX_API vertex_expression_cancel_async(uint32_t requestId);
```

### `vertex_expression_get_children`
Retrieves child members of a compound expression (e.g., struct fields, array elements).

```c
StatusCode VERTEX_API vertex_expression_get_children(uint32_t threadId, uint32_t frameIndex, const char* expression, uint32_t startIndex, uint32_t count, ExpressionResult** children, uint32_t* childCount);
```

### `vertex_expression_free_children`
Frees memory allocated by child expression retrieval.

```c
StatusCode VERTEX_API vertex_expression_free_children(ExpressionResult* children, uint32_t count);
```

### `vertex_expression_set_value`
Modifies the value of an expression (e.g., a variable).

```c
StatusCode VERTEX_API vertex_expression_set_value(uint32_t threadId, uint32_t frameIndex, const char* expression, const char* newValue);
```

### `vertex_expression_get_type_info`
Retrieves type information for an expression.

```c
StatusCode VERTEX_API vertex_expression_get_type_info(uint32_t threadId, uint32_t frameIndex, const char* expression, char* typeInfo, size_t size);
```

## Watch Variables

### `vertex_watch_add`
Adds a watch expression.

```c
StatusCode VERTEX_API vertex_watch_add(const char* expression, uint32_t* watchId);
```

### `vertex_watch_remove`
Removes a watch expression by its ID.

```c
StatusCode VERTEX_API vertex_watch_remove(uint32_t watchId);
```

### `vertex_watch_enable`
Enables or disables a watch expression.

```c
StatusCode VERTEX_API vertex_watch_enable(uint32_t watchId, uint8_t enable);
```

### `vertex_watch_get_value`
Retrieves the current value of a watch expression.

```c
StatusCode VERTEX_API vertex_watch_get_value(uint32_t watchId, uint32_t threadId, uint32_t frameIndex, WatchEntry* entry);
```

### `vertex_watch_get_all`
Retrieves all watch entries with their current values.

```c
StatusCode VERTEX_API vertex_watch_get_all(uint32_t threadId, uint32_t frameIndex, WatchEntry** entries, uint32_t* count);
```

### `vertex_watch_free_entries`
Frees memory allocated by watch entry retrieval.

```c
StatusCode VERTEX_API vertex_watch_free_entries(WatchEntry* entries, uint32_t count);
```

### `vertex_watch_update_all`
Forces re-evaluation of all watch expressions.

```c
StatusCode VERTEX_API vertex_watch_update_all(uint32_t threadId, uint32_t frameIndex);
```

## Local Variables

### `vertex_debugger_get_local_variables`
Retrieves the local variables for a specific thread and stack frame.

```c
StatusCode VERTEX_API vertex_debugger_get_local_variables(uint32_t threadId, uint32_t frameIndex, LocalVariableList* locals);
```

### `vertex_debugger_free_local_variables`
Frees memory allocated by local variable retrieval.

```c
StatusCode VERTEX_API vertex_debugger_free_local_variables(LocalVariableList* locals);
```

### `vertex_debugger_get_local_variable_value`
Retrieves the value of a specific local variable by name.

```c
StatusCode VERTEX_API vertex_debugger_get_local_variable_value(uint32_t threadId, uint32_t frameIndex, const char* name, ExpressionValue* value);
```

### `vertex_debugger_set_local_variable_value`
Sets the value of a specific local variable.

```c
StatusCode VERTEX_API vertex_debugger_set_local_variable_value(uint32_t threadId, uint32_t frameIndex, const char* name, const void* value, size_t size);
```

## Logging

### `vertex_log_info`
Logs an informational message.

```c
StatusCode VERTEX_API vertex_log_info(const char* msg, ...);
```

### `vertex_log_warn`
Logs a warning message.

```c
StatusCode VERTEX_API vertex_log_warn(const char* msg, ...);
```

### `vertex_log_error`
Logs an error message.

```c
StatusCode VERTEX_API vertex_log_error(const char* msg, ...);
```

### `vertex_log_get_instance`
Retrieves the current log handle. Used internally by the runtime.

```c
VertexLogHandle VERTEX_API vertex_log_get_instance();
```

### `vertex_log_set_instance`
Sets the log handle. Used internally by the runtime.

```c
StatusCode VERTEX_API vertex_log_set_instance(VertexLogHandle handle);
```

## Registry (Architecture Registration)

These functions are typically called through the `Runtime` struct provided to `vertex_init`, but are also exported directly.

### `vertex_register_architecture`
Registers architecture metadata (endianness, address width, etc.).

```c
StatusCode VERTEX_API vertex_register_architecture(const ArchitectureInfo* archInfo);
```

### `vertex_register_category`
Registers a register category (e.g., "General Purpose", "Flags").

```c
StatusCode VERTEX_API vertex_register_category(const RegisterCategoryDef* category);
```

### `vertex_unregister_category`
Unregisters a register category by its ID.

```c
StatusCode VERTEX_API vertex_unregister_category(const char* categoryId);
```

### `vertex_register_register`
Registers an individual register definition.

```c
StatusCode VERTEX_API vertex_register_register(const RegisterDef* reg);
```

### `vertex_unregister_register`
Unregisters a register by name.

```c
StatusCode VERTEX_API vertex_unregister_register(const char* registerName);
```

### `vertex_register_flag_bit`
Registers a flag bit definition (e.g., CF, ZF in EFLAGS).

```c
StatusCode VERTEX_API vertex_register_flag_bit(const FlagBitDef* flagBit);
```

### `vertex_register_exception_type`
Registers a platform-specific exception type.

```c
StatusCode VERTEX_API vertex_register_exception_type(const ExceptionTypeDef* exceptionType);
```

### `vertex_register_calling_convention`
Registers a calling convention definition.

```c
StatusCode VERTEX_API vertex_register_calling_convention(const CallingConventionDef* callingConv);
```

### `vertex_register_snapshot`
Performs bulk registration of all architecture metadata at once.

```c
StatusCode VERTEX_API vertex_register_snapshot(const RegistrySnapshot* snapshot);
```

### `vertex_clear_registry`
Clears all registered architecture metadata.

```c
StatusCode VERTEX_API vertex_clear_registry();
```

### `vertex_registry_set_instance`
Sets the registry instance handle. Used internally by the runtime.

```c
StatusCode VERTEX_API vertex_registry_set_instance(void* handle);
```

### `vertex_registry_get_instance`
Retrieves the registry instance handle. Used internally by the runtime.

```c
void* VERTEX_API vertex_registry_get_instance();
```
