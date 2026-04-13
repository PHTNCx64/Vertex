//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugloopcontext.hh>
#include <vertexusrrt/debugger_internal.hh>

#include <sdk/api.h>

#include <windows.h>

#include <algorithm>
#include <format>

extern native_handle& get_native_handle();

extern void cache_process_architecture();
extern void clear_process_architecture();
extern ProcessArchitecture get_process_architecture();

namespace
{
    debugger::TickState g_tickState{};
}

namespace debugger
{
    ExceptionCode map_windows_exception_code(const DWORD code)
    {
        switch (code)
        {
            case EXCEPTION_ACCESS_VIOLATION:     return VERTEX_EXCEPTION_ACCESS_VIOLATION;
            case EXCEPTION_BREAKPOINT:           return VERTEX_EXCEPTION_BREAKPOINT;
            case EXCEPTION_SINGLE_STEP:          return VERTEX_EXCEPTION_SINGLE_STEP;
            case EXCEPTION_ARRAY_BOUNDS_EXCEEDED:return VERTEX_EXCEPTION_ARRAY_BOUNDS_EXCEEDED;
            case EXCEPTION_DATATYPE_MISALIGNMENT:return VERTEX_EXCEPTION_DATATYPE_MISALIGNMENT;
            case EXCEPTION_FLT_DENORMAL_OPERAND: return VERTEX_EXCEPTION_FLT_DENORMAL_OPERAND;
            case EXCEPTION_FLT_DIVIDE_BY_ZERO:   return VERTEX_EXCEPTION_FLT_DIVIDE_BY_ZERO;
            case EXCEPTION_FLT_INEXACT_RESULT:   return VERTEX_EXCEPTION_FLT_INEXACT_RESULT;
            case EXCEPTION_FLT_INVALID_OPERATION:return VERTEX_EXCEPTION_FLT_INVALID_OPERATION;
            case EXCEPTION_FLT_OVERFLOW:         return VERTEX_EXCEPTION_FLT_OVERFLOW;
            case EXCEPTION_FLT_STACK_CHECK:      return VERTEX_EXCEPTION_FLT_STACK_CHECK;
            case EXCEPTION_FLT_UNDERFLOW:        return VERTEX_EXCEPTION_FLT_UNDERFLOW;
            case EXCEPTION_ILLEGAL_INSTRUCTION:  return VERTEX_EXCEPTION_ILLEGAL_INSTRUCTION;
            case EXCEPTION_INT_DIVIDE_BY_ZERO:   return VERTEX_EXCEPTION_INT_DIVIDE_BY_ZERO;
            case EXCEPTION_INT_OVERFLOW:         return VERTEX_EXCEPTION_INT_OVERFLOW;
            case EXCEPTION_PRIV_INSTRUCTION:     return VERTEX_EXCEPTION_PRIV_INSTRUCTION;
            case EXCEPTION_STACK_OVERFLOW:       return VERTEX_EXCEPTION_STACK_OVERFLOW;
            default:                             return VERTEX_EXCEPTION_UNKNOWN;
        }
    }

    StatusCode fill_exception_info(ExceptionInfo* out)
    {
        if (out == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        if (g_tickState.lastPauseReason != PauseReason::Exception || !g_tickState.hasPendingEvent)
        {
            return STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        const auto& [ExceptionRecord, dwFirstChance] = g_tickState.pendingEvent.u.Exception;
        const auto& record = ExceptionRecord;

        out->code = map_windows_exception_code(record.ExceptionCode);
        out->address = reinterpret_cast<std::uint64_t>(record.ExceptionAddress);
        out->firstChance = dwFirstChance != 0 ? 1 : 0;
        out->continuable = (record.ExceptionFlags == 0) ? 1 : 0;
        out->threadId = g_tickState.pendingEvent.dwThreadId;
        out->reserved = 0;

        if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
        {
            out->isWrite = static_cast<std::uint8_t>(record.ExceptionInformation[0]);
            out->accessAddress = record.ExceptionInformation[1];
        }
        else
        {
            out->isWrite = 0;
            out->accessAddress = 0;
        }

        std::string desc {};
        if (record.ExceptionCode == EXCEPTION_ACCESS_VIOLATION && record.NumberParameters >= 2)
        {
            const char* accessType = record.ExceptionInformation[0] == 0 ? "reading" :
                                     record.ExceptionInformation[0] == 1 ? "writing" : "executing";
            desc = std::format("Access violation {} address 0x{:016X}",
                               accessType, record.ExceptionInformation[1]);
        }
        else
        {
            desc = std::format("Exception 0x{:08X}", record.ExceptionCode);
        }

        std::fill_n(out->description, VERTEX_MAX_EXCEPTION_DESC_LENGTH, '\0');
        std::copy_n(desc.c_str(),
                    std::min(desc.size() + 1, static_cast<std::size_t>(VERTEX_MAX_EXCEPTION_DESC_LENGTH)),
                    out->description);

        return STATUS_OK;
    }
}

std::uint32_t get_current_debug_thread_id()
{
    return g_tickState.currentThreadId;
}

std::uint32_t get_attached_process_id()
{
    return g_tickState.attachedProcessId;
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_attach()
    {
        const native_handle& handle = get_native_handle();
        if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        const DWORD processId = GetProcessId(handle);
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        if (g_tickState.attachedProcessId != 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ALREADY_ATTACHED;
        }

        if (!DebugActiveProcess(processId))
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ATTACH_FAILED;
        }

        DebugSetProcessKillOnExit(FALSE);

        cache_process_architecture();

        g_tickState.attachedProcessId = processId;
        g_tickState.currentThreadId = 0;
        g_tickState.initialBreakpointPending = true;
        g_tickState.stopRequested = false;
        g_tickState.pauseRequested = false;
        g_tickState.passException = false;
        g_tickState.pendingCommand = debugger::DebugCommand::None;
        g_tickState.pendingTargetAddress = 0;
        g_tickState.hasPendingEvent = false;
        g_tickState.lastPauseReason = debugger::PauseReason::None;
        g_tickState.lastPauseAddress = 0;
        g_tickState.lastPauseBreakpointId = 0;
        g_tickState.lastPauseWatchpointId = 0;

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_detach()
    {
        if (g_tickState.attachedProcessId == 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        if (g_tickState.hasPendingEvent)
        {
            ContinueDebugEvent(g_tickState.pendingEvent.dwProcessId,
                               g_tickState.pendingEvent.dwThreadId,
                               DBG_CONTINUE);
            g_tickState.hasPendingEvent = false;
        }

        std::ignore = debugger::remove_temp_breakpoint();
        debugger::reset_breakpoint_manager();
        debugger::clear_all_breakpoint_step_overs();
        debugger::clear_all_watchpoint_step_overs();
        debugger::clear_thread_handle_cache();

        DebugActiveProcessStop(g_tickState.attachedProcessId);

        g_tickState.attachedProcessId = 0;
        g_tickState.currentThreadId = 0;
        g_tickState.stopRequested = false;
        g_tickState.pauseRequested = false;
        g_tickState.passException = false;
        g_tickState.initialBreakpointPending = false;
        g_tickState.pendingCommand = debugger::DebugCommand::None;
        g_tickState.pendingTargetAddress = 0;
        g_tickState.lastPauseReason = debugger::PauseReason::None;
        g_tickState.lastPauseAddress = 0;
        g_tickState.lastPauseBreakpointId = 0;
        g_tickState.lastPauseWatchpointId = 0;

        clear_process_architecture();

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_callbacks(const DebuggerCallbacks* callbacks)
    {
        std::scoped_lock lock{g_tickState.callbackMutex};
        if (callbacks != nullptr)
        {
            g_tickState.callbacks = *callbacks;
        }
        else
        {
            g_tickState.callbacks.reset();
        }
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_tick(const std::uint32_t timeout_ms)
    {
        if (g_tickState.attachedProcessId == 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        return debugger::tick_debug_loop(g_tickState, timeout_ms);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_continue(const std::uint8_t passException)
    {
        if (!g_tickState.hasPendingEvent)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        g_tickState.passException = passException != 0;
        g_tickState.pendingCommand = debugger::DebugCommand::Continue;

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_pause()
    {
        if (g_tickState.attachedProcessId == 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        if (g_tickState.hasPendingEvent)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        g_tickState.pauseRequested = true;

        if (!DebugBreakProcess(get_native_handle()))
        {
            g_tickState.pauseRequested = false;
            return StatusCode::STATUS_ERROR_DEBUGGER_BREAK_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_step(const StepMode mode)
    {
        if (!g_tickState.hasPendingEvent)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        switch (mode)
        {
        case VERTEX_STEP_INTO:
            g_tickState.pendingCommand = debugger::DebugCommand::StepInto;
            break;
        case VERTEX_STEP_OVER:
            g_tickState.pendingCommand = debugger::DebugCommand::StepOver;
            break;
        case VERTEX_STEP_OUT:
            g_tickState.pendingCommand = debugger::DebugCommand::StepOut;
            break;
        default:
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run_to_address(const std::uint64_t address)
    {
        if (!g_tickState.hasPendingEvent)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        g_tickState.pendingTargetAddress = address;
        g_tickState.pendingCommand = debugger::DebugCommand::RunToAddress;

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_instruction_pointer(const std::uint32_t threadId, std::uint64_t* address)
    {
        if (address == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (hThread == nullptr)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;

        if (isWow64)
        {
            WOW64_CONTEXT ctx{};
            ctx.ContextFlags = WOW64_CONTEXT_CONTROL;

            if (!Wow64GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }

            *address = ctx.Eip;
        }
        else
        {
            alignas(16) CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;

            if (!GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }

            *address = ctx.Rip;
        }

        CloseHandle(hThread);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_instruction_pointer(const std::uint32_t threadId, const std::uint64_t address)
    {
        const HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadId);
        if (hThread == nullptr)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;

        if (isWow64)
        {
            WOW64_CONTEXT ctx{};
            ctx.ContextFlags = WOW64_CONTEXT_CONTROL;

            if (!Wow64GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }

            ctx.Eip = static_cast<DWORD>(address);

            if (!Wow64SetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }
        else
        {
            alignas(16) CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL;

            if (!GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }

            ctx.Rip = address;

            if (!SetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
            }
        }

        CloseHandle(hThread);
        return StatusCode::STATUS_OK;
    }
}
