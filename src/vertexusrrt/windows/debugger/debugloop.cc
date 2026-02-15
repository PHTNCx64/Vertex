//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>
#include <vertexusrrt/native_handle.hh>

#include <Windows.h>

#include <format>
#include <mutex>

extern void cache_process_architecture();
extern void clear_process_architecture();
extern ProcessArchitecture get_process_architecture();

namespace
{
    DWORD handle_exception(const debugger::DebugLoopContext& ctx,
                           const DEBUG_EVENT& event,
                           const std::stop_token& stopToken,
                           bool& shouldWaitForCommand)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;

        auto msg = std::format("[Vertex] Exception: code=0x{:08X} addr=0x{:016X} firstChance={} thread={}\n",
                               ExceptionRecord.ExceptionCode,
                               reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress),
                               dwFirstChance,
                               event.dwThreadId);
        OutputDebugStringA(msg.c_str());

        if (ExceptionRecord.ExceptionCode == EXCEPTION_BREAKPOINT ||
            ExceptionRecord.ExceptionCode == debugger::STATUS_WX86_BREAKPOINT)
        {
            return debugger::handle_exception_breakpoint(ctx, event, stopToken, shouldWaitForCommand);
        }

        if (ExceptionRecord.ExceptionCode == EXCEPTION_SINGLE_STEP ||
            ExceptionRecord.ExceptionCode == debugger::STATUS_WX86_SINGLE_STEP)
        {
            return debugger::handle_exception_single_step(ctx, event, stopToken, shouldWaitForCommand);
        }

        return debugger::handle_exception_general(ctx, event, stopToken, shouldWaitForCommand);
    }

    DWORD handle_debug_event(const debugger::DebugLoopContext& ctx,
                             const DEBUG_EVENT& event,
                             const std::stop_token& stopToken,
                             bool& shouldWaitForCommand)
    {
        shouldWaitForCommand = false;

        switch (event.dwDebugEventCode)
        {
        case CREATE_PROCESS_DEBUG_EVENT:
            return debugger::handle_create_process(ctx, event);

        case EXIT_PROCESS_DEBUG_EVENT:
            return debugger::handle_exit_process(ctx, event);

        case CREATE_THREAD_DEBUG_EVENT:
            return debugger::handle_create_thread(ctx, event);

        case EXIT_THREAD_DEBUG_EVENT:
            return debugger::handle_exit_thread(ctx, event);

        case LOAD_DLL_DEBUG_EVENT:
            return debugger::handle_load_dll(ctx, event);

        case UNLOAD_DLL_DEBUG_EVENT:
            return debugger::handle_unload_dll(ctx, event);

        case OUTPUT_DEBUG_STRING_EVENT:
            return debugger::handle_output_string(ctx, event);

        case EXCEPTION_DEBUG_EVENT:
            return handle_exception(ctx, event, stopToken, shouldWaitForCommand);

        default:
            return DBG_CONTINUE;
        }
    }
}

namespace debugger
{
    void run_debug_loop(const DebugLoopContext& ctx, const std::stop_token& stopToken)
    {
        const DWORD processId = ctx.pendingAttachProcessId->load(std::memory_order_acquire);
        if (processId == 0)
        {
            return;
        }

        if (!DebugActiveProcess(processId))
        {
            ctx.pendingAttachProcessId->store(0, std::memory_order_release);
            return;
        }
        DebugSetProcessKillOnExit(FALSE);

        cache_process_architecture();

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        ctx.isWow64Process->store(isWow64, std::memory_order_release);
        ctx.attachedProcessId->store(processId, std::memory_order_release);
        ctx.currentState->store(VERTEX_DBG_STATE_RUNNING, std::memory_order_release);
        ctx.pendingAttachProcessId->store(0, std::memory_order_release);
        ctx.initialBreakpointPending->store(true, std::memory_order_release);

        while (!stopToken.stop_requested() && !ctx.stopRequested->load(std::memory_order_acquire))
        {
            DEBUG_EVENT debugEvent{};

            if (!WaitForDebugEventEx(&debugEvent, WAIT_TIMEOUT_MS))
            {
                const DWORD error = GetLastError();
                if (error == ERROR_SEM_TIMEOUT)
                {
                    continue;
                }
                break;
            }

            if (debugEvent.dwDebugEventCode == EXCEPTION_DEBUG_EVENT)
            {
                auto excMsg = std::format("[Vertex] DEBUG_EVENT: EXCEPTION code=0x{:08X} thread={}\n", debugEvent.u.Exception.ExceptionRecord.ExceptionCode, debugEvent.dwThreadId);
                OutputDebugStringA(excMsg.c_str());
            }
            else if (debugEvent.dwDebugEventCode == CREATE_THREAD_DEBUG_EVENT)
            {
                auto thrMsg = std::format("[Vertex] DEBUG_EVENT: CREATE_THREAD thread={}\n", debugEvent.dwThreadId);
                OutputDebugStringA(thrMsg.c_str());
            }

            bool shouldWaitForCommand = false;
            const DWORD continueStatus = handle_debug_event(ctx, debugEvent, stopToken, shouldWaitForCommand);

            auto contMsg = std::format("[Vertex] ContinueDebugEvent: pid={} tid={} status=0x{:08X}\n", debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus);
            OutputDebugStringA(contMsg.c_str());

            if (!ContinueDebugEvent(debugEvent.dwProcessId, debugEvent.dwThreadId, continueStatus))
            {
                auto errMsg = std::format("[Vertex] ContinueDebugEvent FAILED: error={}\n", GetLastError());
                OutputDebugStringA(errMsg.c_str());
                break;
            }
        }

        std::ignore = remove_temp_breakpoint();
        clear_thread_handle_cache();
        clear_process_architecture();
    }
}
