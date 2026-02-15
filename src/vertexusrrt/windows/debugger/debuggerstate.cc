//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugloopcontext.hh>

#include <sdk/api.h>

#include <Windows.h>

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>

extern native_handle& get_native_handle();

namespace
{
    std::atomic<bool> g_stopRequested{false};
    std::atomic<::DebuggerState> g_currentState{VERTEX_DBG_STATE_DETACHED};
    std::atomic<std::uint32_t> g_attachedProcessId{0};
    std::atomic<std::uint32_t> g_pendingAttachProcessId{0};
    std::atomic<std::uint32_t> g_currentThreadId{0};
    std::atomic<bool> g_passException{false};
    std::atomic<bool> g_initialBreakpointPending{false};
    std::optional<::DebuggerCallbacks> g_callbacks{};
    std::mutex g_callbackMutex{};
    std::jthread g_debugThread{};

    std::atomic<debugger::DebugCommand> g_pendingCommand{debugger::DebugCommand::None};
    std::atomic<std::uint64_t> g_targetAddress{0};
    std::condition_variable g_commandSignal{};
    std::mutex g_commandMutex{};
    std::atomic<bool> g_isWow64Process{false};
    std::atomic<bool> g_pauseRequested{false};

    debugger::DebugLoopContext make_context()
    {
        return debugger::DebugLoopContext{
            .stopRequested = &g_stopRequested,
            .currentState = &g_currentState,
            .attachedProcessId = &g_attachedProcessId,
            .pendingAttachProcessId = &g_pendingAttachProcessId,
            .currentThreadId = &g_currentThreadId,
            .passException = &g_passException,
            .callbacks = &g_callbacks,
            .callbackMutex = &g_callbackMutex,
            .pendingCommand = &g_pendingCommand,
            .targetAddress = &g_targetAddress,
            .commandSignal = &g_commandSignal,
            .commandMutex = &g_commandMutex,
            .isWow64Process = &g_isWow64Process,
            .initialBreakpointPending = &g_initialBreakpointPending,
            .pauseRequested = &g_pauseRequested
        };
    }
}

std::uint32_t get_current_debug_thread_id()
{
    return g_currentThreadId.load(std::memory_order_acquire);
}

std::uint32_t get_attached_process_id()
{
    return g_attachedProcessId.load(std::memory_order_acquire);
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

        const DebuggerState currentState = g_currentState.load(std::memory_order_acquire);
        if (currentState != VERTEX_DBG_STATE_DETACHED)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ALREADY_ATTACHED;
        }

        g_pendingAttachProcessId.store(processId, std::memory_order_release);

        if (g_debugThread.joinable())
        {
            g_debugThread.request_stop();
            g_debugThread.join();
        }

        g_debugThread = std::jthread([](const std::stop_token& stopToken) {
            debugger::run_debug_loop(make_context(), stopToken);
        });

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_detach()
    {
        const std::uint32_t attachedPid = g_attachedProcessId.load(std::memory_order_acquire);
        if (attachedPid == 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        g_stopRequested.store(true, std::memory_order_release);

        {
            std::scoped_lock lock{g_commandMutex};
            g_pendingCommand.store(debugger::DebugCommand::None, std::memory_order_release);
        }
        g_commandSignal.notify_all();

        if (g_debugThread.joinable())
        {
            g_debugThread.request_stop();
            g_debugThread.join();
        }

        DebugActiveProcessStop(attachedPid);

        const auto oldState = g_currentState.load(std::memory_order_acquire);
        g_attachedProcessId.store(0, std::memory_order_release);
        g_pendingAttachProcessId.store(0, std::memory_order_release);
        g_currentThreadId.store(0, std::memory_order_release);
        g_currentState.store(VERTEX_DBG_STATE_DETACHED, std::memory_order_release);
        g_passException.store(false, std::memory_order_release);
        g_initialBreakpointPending.store(false, std::memory_order_release);
        g_targetAddress.store(0, std::memory_order_release);
        g_stopRequested.store(false, std::memory_order_release);
        g_pauseRequested.store(false, std::memory_order_release);

        {
            std::scoped_lock lock{g_callbackMutex};
            if (g_callbacks.has_value())
            {
                const auto& cb = g_callbacks.value();
                if (cb.on_detached != nullptr)
                {
                    cb.on_detached(attachedPid, cb.user_data);
                }
                if (cb.on_state_changed != nullptr)
                {
                    cb.on_state_changed(oldState, VERTEX_DBG_STATE_DETACHED, cb.user_data);
                }
            }
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run(const DebuggerCallbacks* callbacks)
    {
        g_stopRequested.store(false, std::memory_order_release);

        {
            std::scoped_lock lock{g_callbackMutex};
            if (callbacks != nullptr)
            {
                g_callbacks = *callbacks;
            }
            else
            {
                g_callbacks.reset();
            }
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_request_stop()
    {
        g_stopRequested.store(true, std::memory_order_release);

        if (g_debugThread.joinable())
        {
            g_debugThread.request_stop();
            g_debugThread.join();
        }

        const std::uint32_t attachedPid = g_attachedProcessId.load(std::memory_order_acquire);
        if (attachedPid != 0)
        {
            DebugActiveProcessStop(attachedPid);
            g_attachedProcessId.store(0, std::memory_order_release);
        }

        g_currentThreadId.store(0, std::memory_order_release);
        g_currentState.store(VERTEX_DBG_STATE_DETACHED, std::memory_order_release);

        {
            std::scoped_lock lock{g_callbackMutex};
            g_callbacks.reset();
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_state(DebuggerState* state)
    {
        if (state == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        *state = g_currentState.load(std::memory_order_acquire);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_continue(const std::uint8_t passException)
    {
        const DebuggerState currentState = g_currentState.load(std::memory_order_acquire);

        if (currentState != VERTEX_DBG_STATE_BREAKPOINT_HIT &&
            currentState != VERTEX_DBG_STATE_STEPPING &&
            currentState != VERTEX_DBG_STATE_EXCEPTION &&
            currentState != VERTEX_DBG_STATE_PAUSED)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        g_passException.store(passException != 0, std::memory_order_release);

        {
            std::scoped_lock lock{g_commandMutex};
            g_pendingCommand.store(debugger::DebugCommand::Continue, std::memory_order_release);
        }
        g_commandSignal.notify_one();

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_pause()
    {
        const ::DebuggerState currentState = g_currentState.load(std::memory_order_acquire);

        if (currentState != VERTEX_DBG_STATE_RUNNING)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        const std::uint32_t attachedPid = g_attachedProcessId.load(std::memory_order_acquire);
        if (attachedPid == 0)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        g_pauseRequested.store(true, std::memory_order_release);

        if (!DebugBreakProcess(get_native_handle()))
        {
            g_pauseRequested.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_DEBUGGER_BREAK_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_step(const StepMode mode)
    {
        const ::DebuggerState currentState = g_currentState.load(std::memory_order_acquire);

        if (currentState != VERTEX_DBG_STATE_BREAKPOINT_HIT &&
            currentState != VERTEX_DBG_STATE_STEPPING &&
            currentState != VERTEX_DBG_STATE_EXCEPTION &&
            currentState != VERTEX_DBG_STATE_PAUSED)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        debugger::DebugCommand cmd{};
        switch (mode)
        {
        case VERTEX_STEP_INTO:
            cmd = debugger::DebugCommand::StepInto;
            break;
        case VERTEX_STEP_OVER:
            cmd = debugger::DebugCommand::StepOver;
            break;
        case VERTEX_STEP_OUT:
            cmd = debugger::DebugCommand::StepOut;
            break;
        default:
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        {
            std::scoped_lock lock{g_commandMutex};
            g_pendingCommand.store(cmd, std::memory_order_release);
        }
        g_commandSignal.notify_one();

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run_to_address(const std::uint64_t address)
    {
        const DebuggerState currentState = g_currentState.load(std::memory_order_acquire);

        if (currentState != VERTEX_DBG_STATE_BREAKPOINT_HIT &&
            currentState != VERTEX_DBG_STATE_STEPPING &&
            currentState != VERTEX_DBG_STATE_EXCEPTION &&
            currentState != VERTEX_DBG_STATE_PAUSED)
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE;
        }

        g_targetAddress.store(address, std::memory_order_release);

        {
            std::scoped_lock lock{g_commandMutex};
            g_pendingCommand.store(debugger::DebugCommand::RunToAddress, std::memory_order_release);
        }
        g_commandSignal.notify_one();

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

        const bool isWow64 = g_isWow64Process.load(std::memory_order_acquire);

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

        const bool isWow64 = g_isWow64Process.load(std::memory_order_acquire);

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
