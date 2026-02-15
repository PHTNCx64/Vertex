# Debugger Architecture

This document describes the architecture of the Vertex debugger subsystem: how the MVVM layers
drive a plugin-based debug engine, the threading model, and how the reference implementation
(`vertexusrrt`) works under the hood.

For the plugin loading system, SDK contract, and registry, see
[Plugin System](PluginSystem.md).

## Table of Contents

- [Overview](#overview)
- [State Machine](#state-machine)
- [Callback System](#callback-system)
- [MVVM Layers](#mvvm-layers)
  - [DebuggerWorker](#debuggerworker)
  - [DebuggerModel](#debuggermodel)
  - [DebuggerViewModel](#debuggerviewmodel)
  - [DebuggerView and Panels](#debuggerview-and-panels)
- [Event Flow](#event-flow)
- [Threading Model](#threading-model)
- [Reference Implementation: vertexusrrt](#reference-implementation-vertexusrrt)
  - [Debug Loop](#debug-loop)
  - [Breakpoint Management](#breakpoint-management)
  - [Watchpoint Management](#watchpoint-management)
  - [Stepping](#stepping)
  - [Thread and Register Access](#thread-and-register-access)
  - [Architecture Detection](#architecture-detection)

---

## Overview

The debugger separates the **engine** (how to debug) from the **frontend** (what to show).
The engine runs inside a plugin DLL that exposes a C API. The frontend consumes it through
four MVVM layers:

```
  DebuggerView          wxFrame with dockable panels (wxAUI)
       │ ViewUpdateEvent (flags)
  DebuggerViewModel     Translates events, tracks UI selection state
       │ DebuggerEvent variant
  DebuggerModel         Caches all debugger data, calls plugin through ILoader
       │ CmdAttach, CmdStep, etc.
  DebuggerWorker        Sends commands to plugin, receives callbacks
       │ C function pointers / DebuggerCallbacks
  Plugin DLL            Runs debug loop on dedicated thread
```

Relevant source locations:

```
include/vertex/debugger/   C++ types, worker, command/event variants
include/vertex/model/      DebuggerModel
include/vertex/viewmodel/  DebuggerViewModel
include/vertex/view/       DebuggerView and panel subclasses
src/vertexusrrt/           Reference plugin (Windows user-mode)
```

---

## State Machine

The debugger defines seven states (`DebuggerState` enum):

```
                      attach()
    DETACHED ─────────────────────> RUNNING
       ^                              |
       |                   breakpoint / exception / pause
       |                              v
       |         continue()      PAUSED / BREAKPOINT_HIT / EXCEPTION
       |      <────────────────
       |                              |
       |                        step_into / over / out
       |                              v
       |                          STEPPING ──> PAUSED (cycle back)
       |
       +──────── detach() from any attached state
```

State transitions are validated in `DebuggerWorker::is_valid_command_for_state()` before
dispatching to the plugin.

---

## Callback System

When the host calls `vertex_debugger_run()`, it passes a `DebuggerCallbacks` structure
with twelve function pointers and a `void* user_data`:

```c
typedef struct VertexDebuggerCallbacks
{
    VertexOnAttached        on_attached;
    VertexOnDetached        on_detached;
    VertexOnStateChanged    on_state_changed;
    VertexOnModuleLoaded    on_module_loaded;
    VertexOnModuleUnloaded  on_module_unloaded;
    VertexOnThreadCreated   on_thread_created;
    VertexOnThreadExited    on_thread_exited;
    VertexOnOutputString    on_debug_output;
    VertexOnBreakpointHit   on_breakpoint_hit;
    VertexOnSingleStep      on_single_step;
    VertexOnException       on_exception;
    VertexOnWatchpointHit   on_watchpoint_hit;
    void*                   user_data;
} DebuggerCallbacks;
```

Callbacks are invoked **synchronously from the debug thread**. The host must not block inside
a callback.

---

## MVVM Layers

### DebuggerWorker

The boundary between the C++ application and the C plugin.

**Commands** (sent by Model, dispatched to plugin via `safe_call`):

```
CmdAttach    CmdDetach    CmdContinue { passException }
CmdPause     CmdStepInto  CmdStepOver
CmdStepOut   CmdRunToAddress { address }   CmdShutdown
```

**Events** (emitted toward Model from plugin callbacks):

```
EvtStateChanged { snapshot }       EvtLog { message }
EvtError { code, message }         EvtAttachFailed { code, message }
EvtBreakpointHit { id, tid, addr } EvtWatchpointHit { id, tid, accessorAddr }
```

A `CallbackGuard` RAII wrapper tracks in-flight callbacks with an atomic counter. During
shutdown, `wait_for_callbacks_to_drain()` blocks up to 5 seconds to ensure all callbacks
complete before the Worker is destroyed.

### DebuggerModel

Owns the `DebuggerWorker` and acts as the single source of truth. Maintains cached snapshots:

| Cache Field | Type | Updated When |
|-------------|------|--------------|
| `m_cachedSnapshot` | `DebuggerSnapshot` | Every state change |
| `m_cachedRegisters` | `RegisterSet` | `read_registers()` |
| `m_cachedDisassembly` | `DisassemblyRange` | `disassemble_at_address()` or extend |
| `m_cachedCallStack` | `CallStack` | State change to Paused |
| `m_cachedBreakpoints` | `vector<Breakpoint>` | Breakpoint mutations |
| `m_cachedModules` | `vector<ModuleInfo>` | `load_modules()` |
| `m_cachedThreads` | `vector<ThreadInfo>` | `load_threads()` |
| `m_cachedWatchpoints` | `vector<Watchpoint>` | Watchpoint mutations |

Exposes `const&` getters. All mutations call the plugin, update the cache, then notify the
ViewModel via the event handler callback.

### DebuggerViewModel

Sits between Model and View:

- Subscribes to `PROCESS_OPEN_EVENT`, `PROCESS_CLOSED_EVENT`, `APPLICATION_SHUTDOWN_EVENT`.
- Translates `DebuggerEvent` variants into `ViewUpdateEvent` with bitwise flags
  (`DEBUGGER_DISASSEMBLY`, `DEBUGGER_REGISTERS`, `DEBUGGER_STATE`, etc.).
- Tracks UI selection state (selected stack frame, selected module).
- Provides registry data access so panels can build register categories dynamically.

### DebuggerView and Panels

Top-level `wxFrame` with wxAUI docking. A 500ms `wxTimer` batches pending update flags and
calls `update_view(flags)`, dispatching to each panel's `update_*` method.

Each panel is a self-contained `wxPanel` with no direct ViewModel reference. User actions
propagate through callback function objects wired in `setup_panel_callbacks()`.

| Panel | Data Source | Key Callbacks |
|-------|------------|---------------|
| `DisassemblyPanel` | `DisassemblyRange` | Navigate, BreakpointToggle, RunToCursor |
| `RegistersPanel` | `RegisterSet` + categories | SetRegister, Refresh |
| `BreakpointsPanel` | `vector<Breakpoint>` | Goto, Remove, Enable |
| `WatchpointsPanel` | `vector<Watchpoint>` | Goto, GotoAccessor, Remove, Enable |
| `StackPanel` | `CallStack` | SelectFrame |
| `ThreadsPanel` | `vector<ThreadInfo>` | Select, Suspend, Resume |
| `MemoryPanel` | `MemoryBlock` | Navigate, WriteMemory |
| `HexEditorPanel` | `MemoryBlock` | Navigate, WriteMemory |
| `ImportExportPanel` | Module imports/exports | Navigate, SelectModule |
| `WatchPanel` | `WatchVariable` tree | Add, Remove, Modify, Expand |
| `ConsolePanel` | `vector<LogEntry>` | Command |

---

## Event Flow

Full propagation path for a state change:

```
Plugin debug thread
    │  DebuggerCallbacks::on_state_changed()
    v
DebuggerWorker::handle_state_changed()
    │  post_event(EvtStateChanged)
    v
DebuggerModel event handler
    │  Updates cached snapshot, forwards to ViewModel
    v
DebuggerViewModel::on_debugger_event()
    │  Posts ViewUpdateEvent(DEBUGGER_ALL) to EventBus
    v
DebuggerView::vertex_event_callback()
    │  m_pendingUpdateFlags |= flags
    v
wxTimer fires (500ms)
    │  update_view(flags) -> each panel refreshes
    v
Panels read from ViewModel getters
```

---

## Threading Model

Three threads participate at runtime:

| Thread | Owner | Role |
|--------|-------|------|
| UI thread | wxWidgets `MainLoop` | Panel rendering, user input, timer-driven updates |
| Debug thread | Plugin (`std::jthread`) | `WaitForDebugEvent` loop, exception handling, command execution |
| Main thread | Application startup | DI container creation, initial plugin load |

Communication:

- **UI -> Debug:** Atomic `DebugCommand` enum + condition variable.
- **Debug -> UI:** `DebuggerCallbacks` invoked from debug thread, translated by Worker into
  `DebuggerEvent` variants, posted through Model/ViewModel to EventBus.

Key synchronization:

| Primitive | Protects |
|-----------|----------|
| `BreakpointManager::mutex` | All breakpoint/watchpoint maps |
| `g_commandMutex` + `g_commandSignal` | Command delivery to debug thread |
| `g_callbackMutex` | `DebuggerCallbacks` optional |
| `ThreadHandleCache::mutex` | Thread handle map |
| `m_drainMutex` + `m_drainCondition` | Callback flight counter (shutdown) |
| `g_capstone_mutex` | Capstone disassembler handle |

All atomic state variables use `memory_order_acquire` / `memory_order_release`.

---

## Reference Implementation: vertexusrrt

The `vertexusrrt` plugin ("Vertex User-Mode Runtime") implements the debugger using Windows
native debugging APIs. It builds as `vertexusrrt.dll` from `src/vertexusrrt/`.

### Debug Loop

Runs on a `std::jthread` spawned by `vertex_debugger_attach()`. Defined in
`src/vertexusrrt/windows/debugger/debugloop.cc`.

```
run_debug_loop(DebugLoopContext, stop_token):
    DebugActiveProcess(pid)
    while (!stopRequested && !stop_token.stop_requested()):
        WaitForDebugEventEx(&event, timeout)

        switch event.dwDebugEventCode:
            CREATE_PROCESS  -> cache main thread, set ATTACHED
            EXIT_PROCESS    -> set DETACHED, break
            CREATE_THREAD   -> cache handle, apply hw breakpoints
            EXIT_THREAD     -> release handle
            LOAD_DLL        -> fire on_module_loaded
            EXCEPTION       -> dispatch to exception handlers
            OUTPUT_STRING   -> fire on_debug_output

        ContinueDebugEvent(pid, tid, status)

    DebugActiveProcessStop(pid)
```

Shared state between the debug thread and host is a `DebugLoopContext` struct of pointers to
atomics and synchronization primitives. When paused (breakpoint, exception, step complete),
the loop blocks in `wait_for_command()` until the UI thread signals a `DebugCommand`.

### Breakpoint Management

Managed by a global `BreakpointManager` singleton (mutex-protected).

**Software breakpoints** (INT3):

1. Save the original byte. Write `0xCC`.
2. On hit: restore original byte, decrement IP by 1, invoke callback, block for command.
3. On resume: set trap flag, single-step one instruction, re-apply `0xCC`.

**Hardware breakpoints** (debug registers DR0-DR3):

1. Allocate a free DR register.
2. Validate address alignment.
3. Program DR address + DR7 control bits on every thread via `Get/SetThreadContext`.
4. New threads receive breakpoints automatically in `handle_create_thread()`.

DR7 control encoding:

```
Bits 0,2,4,6:    Local enable (L0-L3)
Bits 16-17:      DR0 condition (00=exec, 01=write, 11=read/write)
Bits 18-19:      DR0 size (00=1B, 01=2B, 11=4B)
  (repeated for DR1-DR3 at higher bit positions)
```

### Watchpoint Management

Watchpoints reuse the hardware breakpoint infrastructure (same four DR registers):

| Type | DR7 Condition | Trigger |
|------|---------------|---------|
| `VERTEX_WP_WRITE` | `01` | Memory write |
| `VERTEX_WP_READ` / `READWRITE` | `11` | Memory read or write |
| `VERTEX_WP_EXECUTE` | `00` | Instruction fetch |

**Step-over problem:** Resuming after a watchpoint hit would immediately re-trigger the same
watchpoint. The solution:

1. Temporarily disable the watchpoint on **all threads**.
2. Single-step one instruction past the triggering access.
3. Re-enable the watchpoint on all threads.

### Stepping

**Step Into:** Set trap flag (TF) in EFLAGS, resume. The CPU executes one instruction and
raises `EXCEPTION_SINGLE_STEP`.

**Step Over:** Disassemble the current instruction. If it is a `CALL`, set a temporary
breakpoint at the return address (IP + instruction length) and resume. Otherwise, fall through
to Step Into.

**Step Out:** Read the return address from `[RSP]` (x64) or `[ESP]` (x86), set a temporary
breakpoint there, resume.

**Run to Address:** Set a temporary breakpoint at the target, resume.

Only one temporary breakpoint can be active at a time (global `g_tempBreakpoint`).

### Thread and Register Access

Thread handles are cached in a `ThreadHandleCache` singleton on `CREATE_THREAD` /
`CREATE_PROCESS` events and released on `EXIT_THREAD`.

Register access uses offset tables mapping register names to byte offsets within `CONTEXT`
(x64) or `WOW64_CONTEXT` (x86):

1. Retrieve (or open) the thread handle.
2. `GetThreadContext` / `Wow64GetThreadContext`.
3. Read or write at the computed offset.
4. `SetThreadContext` if writing.

### Architecture Detection

Detected at attach time:

1. **Primary:** `IsWow64Process2()` (Win10+) -- returns process and native machine types,
   handles ARM64.
2. **Fallback:** `IsWow64Process()` (older Windows) -- 32-bit vs 64-bit only.

Cached in `std::atomic<ProcessArchitecture>`. Selects `CONTEXT` type, register maps,
and Capstone disassembly mode (`CS_MODE_32` / `CS_MODE_64`).
