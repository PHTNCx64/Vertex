# Plugin System

This document describes how Vertex discovers, loads, and communicates with plugins. For
debugger-specific behavior built on top of this system, see [Debugger Architecture](Debugger.md).

## Table of Contents

- [Overview](#overview)
- [SDK Contract](#sdk-contract)
  - [Required Exports](#required-exports)
  - [API Function Categories](#api-function-categories)
  - [Status Codes](#status-codes)
- [Discovery and Loading](#discovery-and-loading)
- [Function Resolution](#function-resolution)
- [Runtime Services Injection](#runtime-services-injection)
- [Registry System](#registry-system)
- [Plugin Lifecycle](#plugin-lifecycle)
- [Event Dispatch](#event-dispatch)

---

## Overview

Vertex uses a plugin architecture where platform-specific functionality is implemented in
dynamically loaded shared libraries (`.dll` on Windows, `.so` on Linux). The host application
interacts with plugins through a flat C API defined in `include/sdk/`.

Key properties:

- **Single active plugin.** Only one plugin may be active at a time, stored in
  `Loader::m_activePlugin`.
- **On-demand loading.** Plugins are discovered at startup but loaded only when activated.
- **Auto-generated bindings.** A CMake script parses `sdk/api.h` and generates function pointer
  declarations, registration macros, and move semantics for the `Plugin` class.
- **Safe calls.** All host-to-plugin calls go through `Runtime::safe_call`, which catches
  structured exceptions and returns `std::expected<StatusCode, CallerError>`.

Relevant source locations:

```
include/sdk/              C API headers (the plugin contract)
include/vertex/runtime/   Plugin loader, registry, safe_call
src/vertex/runtime/       Loader implementation
cmake/gen_plugin_functions.cmake   Code generation
```

---

## SDK Contract

### Required Exports

A conforming plugin must export at minimum:

| Symbol        | Signature                                  | Purpose                                                                                                              |
|---------------|--------------------------------------------|----------------------------------------------------------------------------------------------------------------------|
| `vertex_init` | `StatusCode(PluginInformation*, Runtime*)` | Called once after DLL load. The plugin fills `PluginInformation` with its metadata and stores the `Runtime` pointer. |
| `vertex_exit` | `StatusCode()`                             | Called before DLL unload. The plugin releases all resources.                                                         |

An optional third symbol handles lifecycle events:

| Symbol         | Signature                        | Purpose                                                                    |
|----------------|----------------------------------|----------------------------------------------------------------------------|
| `vertex_event` | `StatusCode(Event, const void*)` | Dispatched by the host on process open/close, debugger attach/detach, etc. |

Beyond these, the plugin may export any subset of the ~110 API functions declared in `api.h`.
Missing optional functions are left as null pointers; calls to them are guarded by null checks.

### API Function Categories

| Category           | Prefix             | Examples                                                          |
|--------------------|--------------------|-------------------------------------------------------------------|
| Process management | `vertex_process_`  | `open`, `close`, `kill`, `get_list`, `get_modules_list`           |
| Memory operations  | `vertex_memory_`   | `read_process`, `write_process`, `allocate`, `query_regions`      |
| Debugger lifecycle | `vertex_debugger_` | `attach`, `detach`, `run`, `pause`, `continue`                    |
| Execution control  | `vertex_debugger_` | `step`, `run_to_address`, `get_instruction_pointer`               |
| Breakpoints        | `vertex_debugger_` | `set_breakpoint`, `remove_breakpoint`, `enable_breakpoint`        |
| Watchpoints        | `vertex_debugger_` | `set_watchpoint`, `remove_watchpoint`, `enable_watchpoint`        |
| Threads            | `vertex_debugger_` | `get_threads`, `suspend_thread`, `resume_thread`, `get_registers` |
| Registers          | `vertex_debugger_` | `read_register`, `write_register`                                 |
| Disassembly        | `vertex_process_`  | `disassemble_range`                                               |
| Symbols            | `vertex_symbol_`   | `load`, `search`, `get_source`                                    |

All fallible functions return `StatusCode`. Output is written through pointer parameters.

### Status Codes

Defined in `statuscode.h`. Key codes:

| Code                                         | Meaning                                             |
|----------------------------------------------|-----------------------------------------------------|
| `STATUS_OK`                                  | Success                                             |
| `STATUS_ERROR_DEBUGGER_NOT_ATTACHED`         | Operation requires an attached process              |
| `STATUS_ERROR_DEBUGGER_ALREADY_RUNNING`      | Attempted to attach while already attached          |
| `STATUS_ERROR_BREAKPOINT_NOT_FOUND`          | Breakpoint ID does not exist                        |
| `STATUS_ERROR_BREAKPOINT_LIMIT_REACHED`      | All four hardware debug registers are in use        |
| `STATUS_ERROR_BREAKPOINT_ADDRESS_MISALIGNED` | Hardware breakpoint address violates alignment      |
| `STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS`     | A breakpoint already exists at the given address    |
| `STATUS_ERROR_REGISTER_NOT_FOUND`            | Named register does not exist for this architecture |
| `STATUS_ERROR_NOT_IMPLEMENTED`               | Plugin has not implemented this function            |

---

## Discovery and Loading

Plugin management is handled by `Runtime::Loader`, injected as a singleton via Boost.DI.

**Discovery** (application startup):

1. Read `settings["plugins"]["pluginPaths"]` from `Settings.json`.
2. Scan each path for files matching `PLUGIN_EXTENSION` (`.dll` / `.so`).
3. Create a `Plugin` object for each file, storing only the path. No DLL is loaded yet.

**Loading** (on-demand, when the user activates a plugin):

1. `LoadLibraryA()` (or `dlopen()`) loads the DLL.
2. The `Runtime` structure is populated with host-side function pointers.
3. All exported functions are resolved via `GetProcAddress()`.
4. `vertex_init()` is called with `PluginInformation*` and `Runtime*`.

---

## Function Resolution

Function pointer declarations, registration macros, and move semantics are **auto-generated** by
`cmake/gen_plugin_functions.cmake`, which parses `sdk/api.h` and produces:

| Generated Header                  | Content                                                                            |
|-----------------------------------|------------------------------------------------------------------------------------|
| `plugin_functions.hh`             | `VERTEX_PLUGIN_FUNCTION_POINTERS` macro: ~111 function pointer member declarations |
| `plugin_function_registration.hh` | `VERTEX_PLUGIN_REGISTER_FUNCTIONS` macro: maps symbol names to pointer fields      |
| `plugin_move_semantics.hh`        | Move constructor initializer list and nullification macros                         |

The `Plugin` class embeds these generated pointers as member fields. At load time, a
`FunctionRegistry` resolves each symbol. Required functions cause a load failure if missing;
optional functions are silently left null.

---

## Runtime Services Injection

Before calling `vertex_init()`, the Loader populates the `Runtime` structure with function
pointers that the plugin can call back into the host:

```
Runtime
├── Logging
│   ├── vertex_log_info    ──> Log::ILog singleton
│   ├── vertex_log_warn    ──> Log::ILog singleton
│   └── vertex_log_error   ──> Log::ILog singleton
│
├── Events
│   └── vertex_queue_event ──> EventBus dispatch
│
└── Registry (13 functions)
    ├── vertex_register_architecture
    ├── vertex_register_category
    ├── vertex_register_register
    ├── vertex_register_flag_bit
    ├── vertex_register_exception_type
    ├── vertex_register_calling_convention
    ├── vertex_register_snapshot
    ├── vertex_unregister_category
    └── vertex_clear_registry
```

These reference static trampoline functions that forward to the appropriate singleton. The
singletons are set via `vertex_log_set_instance()` and `vertex_registry_set_instance()` before
resolution.

---

## Registry System

Plugins describe their target architecture at initialization time by calling registry functions
from the `Runtime` structure. This allows the host to display architecture-appropriate UI without
hardcoding any platform knowledge.

| Registration        | Structure              | Purpose                                                                     |
|---------------------|------------------------|-----------------------------------------------------------------------------|
| Architecture        | `ArchitectureInfo`     | Address width, endianness, max hardware breakpoints, stack growth direction |
| Register categories | `RegisterCategoryDef`  | Groups like "General Purpose", "Segment", "XMM" with display ordering       |
| Registers           | `RegisterDef`          | Individual registers (name, bit width, category)                            |
| Flag bits           | `FlagBitDef`           | Individual bits within a flags register (CF, ZF, SF, etc.)                  |
| Exception types     | `ExceptionTypeDef`     | Exception codes with human-readable names and fatality flags                |
| Calling conventions | `CallingConventionDef` | Parameter registers, return register, stack cleanup rules                   |

The host stores metadata in a thread-safe `Registry` singleton. The ViewModel queries this to
build register panels, flag tooltips, and exception descriptions dynamically.

---

## Plugin Lifecycle

```
  ┌──────────────────┐
  │    DISCOVERED     │  Plugin object created, path stored, DLL not loaded
  └────────┬─────────┘
           │ load_plugin()
           v
  ┌──────────────────┐
  │     LOADED        │  DLL loaded, functions resolved, vertex_init() called
  └────────┬─────────┘
           │ set_active_plugin()
           v
  ┌──────────────────┐
  │     ACTIVE        │  Receives vertex_event() dispatches, API callable
  └────────┬─────────┘
           │ unload or app exit
           v
  ┌──────────────────┐
  │    UNLOADED       │  vertex_exit() called, DLL freed
  └──────────────────┘
```

---

## Event Dispatch

The host dispatches lifecycle events to the active plugin via `vertex_event()`:

| Event                      | Trigger                          |
|----------------------------|----------------------------------|
| `VERTEX_PROCESS_OPENED`    | User opens/attaches to a process |
| `VERTEX_PROCESS_CLOSED`    | Process is closed or terminated  |
| `VERTEX_DEBUGGER_ATTACHED` | Debugger successfully attached   |
| `VERTEX_DEBUGGER_DETACHED` | Debugger detached from process   |

Plugins handle these events to perform initialization and cleanup tasks (e.g., caching
architecture info on process open, clearing module caches on close).
