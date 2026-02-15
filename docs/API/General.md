# Vertex Plugin API

Vertex is modular and supports dynamic loadable modules which can be used by a user to utilize the software for
different platforms other than the host platform.

# Overview

Generally speaking, Vertex consists of the Runtime written in C++ and the C ABI compatible Plugin system.

The repository contains a plugin SDK consisting of several C headers which can be used by plugin developers to interact
with the software.

The plugin API is designed to be C ABI compatible, which allows developers to write extensions and modifications in
different languages other than C or C++.

## Entry Point

A plugin must export the `vertex_init` function and `vertex_exit`.

```c
StatusCode VERTEX_API vertex_init(PluginInformation* pluginInfo, Runtime* runtime);
StatusCode VERTEX_API vertex_exit();
```

## Thread Safety

**IMPORTANT:** Vertex internally uses several functions across multiple threads. General thread-safety is strongly
recommended for all plugin implementations. Plugins must guarantee thread-safety for their internal data structures
and operations, as callbacks and API functions may be invoked concurrently from different threads.

-   The debugger loop (`vertex_debugger_run`) runs on a dedicated thread spawned by the runtime.
-   `vertex_init` and `vertex_exit` are called on the main application thread.
-   Debugger callbacks (`DebuggerCallbacks`) are invoked from the debugger thread.
-   Event callbacks via `vertex_event` may be invoked from different threads depending on the event source (e.g., UI thread, debugger thread).
-   Execution control functions (`vertex_debugger_continue`, `vertex_debugger_step`, `vertex_debugger_pause`, `vertex_debugger_request_stop`) are designed to be called from any thread (thread-safe).

Plugins should use appropriate synchronization primitives (mutexes, atomics, etc.) when accessing shared resources or
global variables. Even if your plugin only uses a subset of the API, assume that any exported function may be called
from a thread other than the one that called `vertex_init`.

## Data handling

-   Strings (`char*`) shall be handled in UTF-8 format.
-   Plugins maintain responsibility to allocate and provide data to the runtime, unless specified otherwise (e.g., via `vertex_memory_allocate`).
-   Boolean fields in SDK structures use `uint8_t` (not `bool`) for C ABI compatibility.
-   Fixed-width integer types from `<stdint.h>` are used throughout for portability.
