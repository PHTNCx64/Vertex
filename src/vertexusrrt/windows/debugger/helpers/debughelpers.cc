//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/disassembler.hh>
#include <vertexusrrt/native_handle.hh>

#include <Windows.h>

#include <array>
#include <chrono>

extern native_handle& get_native_handle();

namespace debugger
{
    constexpr DWORD THREAD_CACHE_ACCESS = THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME;

    ThreadHandleCache g_threadHandleCache{};

    ThreadHandleCache& get_thread_handle_cache()
    {
        return g_threadHandleCache;
    }

    void cache_thread_handle(const DWORD threadId)
    {
        const HANDLE handle = OpenThread(THREAD_CACHE_ACCESS, FALSE, threadId);
        if (handle == nullptr)
        {
            return;
        }

        std::scoped_lock lock{g_threadHandleCache.mutex};
        auto [it, inserted] = g_threadHandleCache.handles.emplace(threadId, handle);
        if (!inserted)
        {
            CloseHandle(handle);
        }
    }

    void release_thread_handle(const DWORD threadId)
    {
        std::scoped_lock lock{g_threadHandleCache.mutex};
        const auto it = g_threadHandleCache.handles.find(threadId);
        if (it != g_threadHandleCache.handles.end())
        {
            CloseHandle(it->second);
            g_threadHandleCache.handles.erase(it);
        }
    }

    HANDLE get_cached_thread_handle(const DWORD threadId)
    {
        std::scoped_lock lock{g_threadHandleCache.mutex};
        const auto it = g_threadHandleCache.handles.find(threadId);
        if (it != g_threadHandleCache.handles.end())
        {
            return it->second;
        }
        return nullptr;
    }

    void clear_thread_handle_cache()
    {
        std::scoped_lock lock{g_threadHandleCache.mutex};
        for (const auto& [id, handle] : g_threadHandleCache.handles)
        {
            CloseHandle(handle);
        }
        g_threadHandleCache.handles.clear();
    }

    TempBreakpoint g_tempBreakpoint{};
    std::mutex g_tempBreakpointMutex{};

    BreakpointStepOver g_breakpointStepOver{};
    std::mutex g_breakpointStepOverMutex{};

    bool is_paused_state(const DebuggerState state)
    {
        return state == VERTEX_DBG_STATE_PAUSED ||
               state == VERTEX_DBG_STATE_BREAKPOINT_HIT ||
               state == VERTEX_DBG_STATE_EXCEPTION ||
               state == VERTEX_DBG_STATE_STEPPING;
    }

    namespace
    {
        std::uint64_t get_instruction_pointer_native(const HANDLE threadHandle)
        {
            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;

            if (!GetThreadContext(threadHandle, &context))
            {
                return 0;
            }

            return context.Rip;
        }

        std::uint64_t get_instruction_pointer_wow64(const HANDLE threadHandle)
        {
            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_CONTROL;

            if (!Wow64GetThreadContext(threadHandle, &context))
            {
                return 0;
            }

            return context.Eip;
        }

        std::uint64_t get_stack_pointer_native(const HANDLE threadHandle)
        {
            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;

            if (!GetThreadContext(threadHandle, &context))
            {
                return 0;
            }

            return context.Rsp;
        }

        std::uint64_t get_stack_pointer_wow64(const HANDLE threadHandle)
        {
            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_CONTROL;

            if (!Wow64GetThreadContext(threadHandle, &context))
            {
                return 0;
            }

            return context.Esp;
        }

        bool set_trap_flag_native(const HANDLE threadHandle, const bool enable)
        {
            if (SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return false;
            }

            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;

            bool result = false;
            if (GetThreadContext(threadHandle, &context))
            {
                if (enable)
                {
                    context.EFlags |= EFLAGS_TRAP_FLAG;
                }
                else
                {
                    context.EFlags &= ~EFLAGS_TRAP_FLAG;
                }
                result = SetThreadContext(threadHandle, &context) != FALSE;
            }

            ResumeThread(threadHandle);
            return result;
        }

        bool set_trap_flag_wow64(const HANDLE threadHandle, const bool enable)
        {
            if (Wow64SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return false;
            }

            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_CONTROL;

            bool result = false;
            if (Wow64GetThreadContext(threadHandle, &context))
            {
                if (enable)
                {
                    context.EFlags |= EFLAGS_TRAP_FLAG;
                }
                else
                {
                    context.EFlags &= ~EFLAGS_TRAP_FLAG;
                }
                result = Wow64SetThreadContext(threadHandle, &context) != FALSE;
            }

            ResumeThread(threadHandle);
            return result;
        }
    }

    std::uint64_t get_instruction_pointer(const DWORD threadId, const bool isWow64)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached : OpenThread(THREAD_GET_CONTEXT, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return 0;
        }

        const std::uint64_t rip = isWow64
            ? get_instruction_pointer_wow64(threadHandle)
            : get_instruction_pointer_native(threadHandle);

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return rip;
    }

    std::uint64_t get_stack_pointer(const DWORD threadId, const bool isWow64)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached : OpenThread(THREAD_GET_CONTEXT, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return 0;
        }

        const std::uint64_t rsp = isWow64
            ? get_stack_pointer_wow64(threadHandle)
            : get_stack_pointer_native(threadHandle);

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return rsp;
    }

    bool read_process_memory(const std::uint64_t address, void* buffer, const std::size_t size)
    {
        const native_handle& processHandle = get_native_handle();
        if (processHandle == INVALID_HANDLE_VALUE || processHandle == nullptr)
        {
            return false;
        }

        SIZE_T bytesRead = 0;
        return ReadProcessMemory(processHandle, reinterpret_cast<LPCVOID>(address),
                                buffer, size, &bytesRead) && bytesRead == size;
    }

    bool write_process_memory(const std::uint64_t address, const void* buffer, const std::size_t size)
    {
        const native_handle& processHandle = get_native_handle();
        if (processHandle == INVALID_HANDLE_VALUE || processHandle == nullptr)
        {
            return false;
        }

        SIZE_T bytesWritten = 0;
        return WriteProcessMemory(processHandle, reinterpret_cast<LPVOID>(address),
                                 buffer, size, &bytesWritten) && bytesWritten == size;
    }

    bool set_trap_flag(const HANDLE threadHandle, const bool isWow64, const bool enable)
    {
        if (isWow64)
        {
            return set_trap_flag_wow64(threadHandle, enable);
        }

        return set_trap_flag_native(threadHandle, enable);
    }

    bool set_trap_flag(const DWORD threadId, const bool isWow64, const bool enable)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached
            : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return false;
        }

        const bool result = set_trap_flag(threadHandle, isWow64, enable);

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return result;
    }

    bool decrement_instruction_pointer(const DWORD threadId, const bool isWow64)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached
            : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return false;
        }

        bool result = false;

        if (isWow64)
        {
            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_CONTROL;
            if (Wow64GetThreadContext(threadHandle, &context))
            {
                context.Eip -= 1;
                result = Wow64SetThreadContext(threadHandle, &context) != FALSE;
            }
        }
        else
        {
            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_CONTROL;
            if (GetThreadContext(threadHandle, &context))
            {
                context.Rip -= 1;
                result = SetThreadContext(threadHandle, &context) != FALSE;
            }
        }

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return result;
    }

    bool set_temp_breakpoint(const std::uint64_t address)
    {
        std::scoped_lock lock{g_tempBreakpointMutex};

        if (g_tempBreakpoint.active)
        {
            return false;
        }

        std::uint8_t originalByte = 0;
        if (!read_process_memory(address, &originalByte, 1))
        {
            return false;
        }

        if (!write_process_memory(address, &INT3_OPCODE, 1))
        {
            return false;
        }

        g_tempBreakpoint.address = address;
        g_tempBreakpoint.originalByte = originalByte;
        g_tempBreakpoint.active = true;

        return true;
    }

    bool remove_temp_breakpoint()
    {
        std::scoped_lock lock{g_tempBreakpointMutex};

        if (!g_tempBreakpoint.active)
        {
            return false;
        }

        if (!write_process_memory(g_tempBreakpoint.address, &g_tempBreakpoint.originalByte, 1))
        {
            return false;
        }

        g_tempBreakpoint.active = false;
        g_tempBreakpoint.address = 0;
        g_tempBreakpoint.originalByte = 0;

        return true;
    }

    bool is_temp_breakpoint_hit(const std::uint64_t address)
    {
        std::scoped_lock lock{g_tempBreakpointMutex};
        return g_tempBreakpoint.active && g_tempBreakpoint.address == address;
    }

    std::optional<std::uint64_t> get_step_over_target(const DWORD threadId, const bool isWow64)
    {
        const std::uint64_t rip = get_instruction_pointer(threadId, isWow64);
        if (rip == 0)
        {
            return std::nullopt;
        }

        std::array<std::uint8_t, MAX_INSTRUCTION_SIZE> codeBuffer{};
        if (!read_process_memory(rip, codeBuffer.data(), MAX_INSTRUCTION_SIZE))
        {
            return std::nullopt;
        }

        DisassemblerResult result{};
        const std::uint32_t size = PluginRuntime::disassemble_single(
            rip,
            std::span<const std::uint8_t>{codeBuffer},
            &result
        );

        if (size == 0)
        {
            return std::nullopt;
        }

        if (result.branchType == VERTEX_BRANCH_CALL ||
            result.branchType == VERTEX_BRANCH_INDIRECT_CALL)
        {
            return result.fallthroughAddress;
        }

        return std::nullopt;
    }

    std::optional<std::uint64_t> get_step_out_target(const DWORD threadId, const bool isWow64)
    {
        const std::uint64_t rsp = get_stack_pointer(threadId, isWow64);
        if (rsp == 0)
        {
            return std::nullopt;
        }

        std::uint64_t returnAddress = 0;

        if (isWow64)
        {
            std::uint32_t ret32 = 0;
            if (!read_process_memory(rsp, &ret32, sizeof(ret32)))
            {
                return std::nullopt;
            }
            returnAddress = ret32;
        }
        else
        {
            if (!read_process_memory(rsp, &returnAddress, sizeof(returnAddress)))
            {
                return std::nullopt;
            }
        }

        return returnAddress;
    }

    DebugCommand wait_for_command(const DebugLoopContext& ctx, const std::stop_token& stopToken)
    {
        std::unique_lock lock{*ctx.commandMutex};

        ctx.commandSignal->wait(lock, [&] {
            return stopToken.stop_requested() ||
                   ctx.stopRequested->load(std::memory_order_acquire) ||
                   ctx.pendingCommand->load(std::memory_order_acquire) != DebugCommand::None;
        });

        if (stopToken.stop_requested() || ctx.stopRequested->load(std::memory_order_acquire))
        {
            return DebugCommand::Continue;
        }

        const auto cmd = ctx.pendingCommand->exchange(DebugCommand::None, std::memory_order_acq_rel);
        return cmd;
    }

    void set_breakpoint_step_over(const std::uint64_t address)
    {
        std::scoped_lock lock{g_breakpointStepOverMutex};
        g_breakpointStepOver.address = address;
        g_breakpointStepOver.active = true;
    }

    void clear_breakpoint_step_over()
    {
        std::scoped_lock lock{g_breakpointStepOverMutex};
        g_breakpointStepOver.address = 0;
        g_breakpointStepOver.active = false;
    }

    bool is_stepping_over_breakpoint(std::uint64_t* address)
    {
        std::scoped_lock lock{g_breakpointStepOverMutex};
        if (g_breakpointStepOver.active && address != nullptr)
        {
            *address = g_breakpointStepOver.address;
        }
        return g_breakpointStepOver.active;
    }

    std::unordered_map<DWORD, WatchpointStepOver> g_watchpointStepOvers{};
    std::mutex g_watchpointStepOverMutex{};

    void set_watchpoint_step_over(const std::uint32_t watchpointId, const std::uint8_t registerIndex, const DWORD threadId)
    {
        std::scoped_lock lock{g_watchpointStepOverMutex};
        g_watchpointStepOvers[threadId] = WatchpointStepOver{
            .watchpointId = watchpointId,
            .registerIndex = registerIndex,
            .threadId = threadId,
            .active = true
        };
    }

    void clear_watchpoint_step_over(const DWORD threadId)
    {
        std::scoped_lock lock{g_watchpointStepOverMutex};
        g_watchpointStepOvers.erase(threadId);
    }

    bool is_stepping_over_watchpoint(const DWORD threadId, std::uint32_t* watchpointId)
    {
        std::scoped_lock lock{g_watchpointStepOverMutex};
        const auto it = g_watchpointStepOvers.find(threadId);
        if (it == g_watchpointStepOvers.end())
        {
            return false;
        }

        if (watchpointId != nullptr)
        {
            *watchpointId = it->second.watchpointId;
        }
        return true;
    }

    StatusCode disable_watchpoint_on_thread(const HANDLE threadHandle, const std::uint8_t registerIndex, const bool isWow64)
    {
        StatusCode result = STATUS_OK;

        if (isWow64)
        {
            if (Wow64SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return STATUS_ERROR_THREAD_NOT_FOUND;
            }

            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;

            if (Wow64GetThreadContext(threadHandle, &context))
            {
                const auto localEnableBit = static_cast<DWORD>(1 << (registerIndex * 2));
                context.Dr7 &= ~localEnableBit;
                if (!Wow64SetThreadContext(threadHandle, &context))
                {
                    result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
                }
            }
            else
            {
                result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }
        else
        {
            if (SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return STATUS_ERROR_THREAD_NOT_FOUND;
            }

            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (GetThreadContext(threadHandle, &context))
            {
                const auto localEnableBit = static_cast<std::uint64_t>(1) << (registerIndex * 2);
                context.Dr7 &= ~localEnableBit;
                if (!SetThreadContext(threadHandle, &context))
                {
                    result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
                }
            }
            else
            {
                result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }

        ResumeThread(threadHandle);
        return result;
    }

    StatusCode disable_watchpoint_on_thread(const DWORD threadId, const std::uint8_t registerIndex, const bool isWow64)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached
            : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return STATUS_ERROR_THREAD_NOT_FOUND;
        }

        const StatusCode result = disable_watchpoint_on_thread(threadHandle, registerIndex, isWow64);

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return result;
    }

    StatusCode enable_watchpoint_on_thread(const HANDLE threadHandle, const std::uint8_t registerIndex, const bool isWow64)
    {
        StatusCode result = STATUS_OK;

        if (isWow64)
        {
            if (Wow64SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return STATUS_ERROR_THREAD_NOT_FOUND;
            }

            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;

            if (Wow64GetThreadContext(threadHandle, &context))
            {
                const auto localEnableBit = static_cast<DWORD>(1 << (registerIndex * 2));
                context.Dr7 |= localEnableBit;
                if (!Wow64SetThreadContext(threadHandle, &context))
                {
                    result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
                }
            }
            else
            {
                result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }
        else
        {
            if (SuspendThread(threadHandle) == static_cast<DWORD>(-1))
            {
                return STATUS_ERROR_THREAD_NOT_FOUND;
            }

            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (GetThreadContext(threadHandle, &context))
            {
                const auto localEnableBit = static_cast<std::uint64_t>(1) << (registerIndex * 2);
                context.Dr7 |= localEnableBit;
                if (!SetThreadContext(threadHandle, &context))
                {
                    result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
                }
            }
            else
            {
                result = STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }

        ResumeThread(threadHandle);
        return result;
    }

    StatusCode enable_watchpoint_on_thread(const DWORD threadId, const std::uint8_t registerIndex, const bool isWow64)
    {
        const HANDLE cached = get_cached_thread_handle(threadId);
        const HANDLE threadHandle = cached != nullptr ? cached
            : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (threadHandle == nullptr)
        {
            return STATUS_ERROR_THREAD_NOT_FOUND;
        }

        const StatusCode result = enable_watchpoint_on_thread(threadHandle, registerIndex, isWow64);

        if (cached == nullptr)
        {
            CloseHandle(threadHandle);
        }
        return result;
    }
}
