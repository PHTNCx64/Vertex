//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/linux/lldb_backend.hh>
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/watchpoint_throttle.hh>
#include <sdk/api.h>

#include <algorithm>
#include <array>
#include <format>

#include <csignal>
#include <unistd.h>

extern native_handle& get_native_handle();
extern Runtime* g_pluginRuntime;

extern void cache_process_architecture();
extern void clear_process_architecture();
extern bool detect_wine_process(std::uint32_t pid);

std::uint32_t Debugger::find_vertex_breakpoint_id(const Debugger::LldbBackendState& state, const lldb::break_id_t lldbId)
{
    for (const auto& [vertexId, entry] : state.breakpoints)
    {
        if (!entry.isHardwareData && entry.lldbId == lldbId)
        {
            return vertexId;
        }
    }
    return 0;
}

std::uint32_t Debugger::find_vertex_watchpoint_id(const Debugger::LldbBackendState& state, const lldb::watch_id_t lldbId)
{
    for (const auto& [vertexId, entry] : state.watchpoints)
    {
        if (entry.lldbId == lldbId)
        {
            return vertexId;
        }
    }

    for (const auto& [vertexId, entry] : state.breakpoints)
    {
        if (entry.isHardwareData && entry.lldbId == lldbId)
        {
            return vertexId;
        }
    }
    return 0;
}

namespace
{
    void fire_module_loaded(Debugger::LldbBackendState& state, lldb::SBModule mod)
    {
        if (!mod.IsValid())
        {
            return;
        }

        std::scoped_lock lock{state.callbackMutex};
        if (!state.callbacks.has_value() || state.callbacks->on_module_loaded == nullptr)
        {
            return;
        }

        const auto& cb = state.callbacks.value();
        ModuleEvent moduleEvent{};
        moduleEvent.isMainModule = 0;

        const auto fileSpec = mod.GetFileSpec();
        std::string fullPath{};

        if (fileSpec.IsValid())
        {
            const char* dir = fileSpec.GetDirectory();
            const char* file = fileSpec.GetFilename();
            if (dir != nullptr && file != nullptr)
            {
                fullPath = std::format("{}/{}", dir, file);
            }
            else if (file != nullptr)
            {
                fullPath = file;
            }
        }

        std::fill_n(moduleEvent.moduleName, VERTEX_MAX_NAME_LENGTH, '\0');
        std::fill_n(moduleEvent.modulePath, VERTEX_MAX_PATH_LENGTH, '\0');

        if (!fullPath.empty())
        {
            std::copy_n(fullPath.c_str(),
                        std::min(fullPath.size() + 1, static_cast<std::size_t>(VERTEX_MAX_PATH_LENGTH)),
                        moduleEvent.modulePath);

            const auto lastSlash = fullPath.find_last_of('/');
            const auto name = (lastSlash != std::string::npos) ? fullPath.substr(lastSlash + 1) : fullPath;
            std::copy_n(name.c_str(),
                        std::min(name.size() + 1, static_cast<std::size_t>(VERTEX_MAX_NAME_LENGTH)),
                        moduleEvent.moduleName);
        }

        const auto headerAddr = mod.GetObjectFileHeaderAddress();
        moduleEvent.baseAddress = headerAddr.IsValid() ? headerAddr.GetLoadAddress(state.target) : 0;
        moduleEvent.size = 0;

        for (std::uint32_t s = 0; s < mod.GetNumSections(); ++s)
        {
            auto section = mod.GetSectionAtIndex(s);
            if (section.IsValid())
            {
                moduleEvent.size += section.GetByteSize();
            }
        }

        cb.on_module_loaded(&moduleEvent, cb.user_data);
    }

    void enumerate_initial_modules(Debugger::LldbBackendState& state)
    {
        if (!state.target.IsValid())
        {
            return;
        }

        const auto numModules = state.target.GetNumModules();
        for (std::uint32_t i = 0; i < numModules; ++i)
        {
            auto mod = state.target.GetModuleAtIndex(i);
            fire_module_loaded(state, mod);
        }
    }

    [[nodiscard]] bool evaluate_hit_count_condition(const BreakpointCondition& condition, const std::uint32_t hitCount)
    {
        if (condition.type == VERTEX_BP_COND_NONE || condition.enabled == 0)
        {
            return true;
        }

        switch (condition.type)
        {
        case VERTEX_BP_COND_HIT_COUNT_EQUAL:
            return hitCount == condition.hitCountTarget;
        case VERTEX_BP_COND_HIT_COUNT_GREATER:
            return hitCount > condition.hitCountTarget;
        case VERTEX_BP_COND_HIT_COUNT_MULTIPLE:
            return condition.hitCountTarget > 0 && (hitCount % condition.hitCountTarget) == 0;
        default:
            return true;
        }
    }

    StatusCode dispatch_stop_event(Debugger::LldbBackendState& state)
    {
        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_DEBUG_TICK_ERROR;
        }

        const auto numThreads = state.process.GetNumThreads();
        for (std::uint32_t i = 0; i < numThreads; ++i)
        {
            auto thread = state.process.GetThreadAtIndex(i);
            if (!thread.IsValid())
            {
                continue;
            }

            const auto reason = thread.GetStopReason();
            if (reason == lldb::eStopReasonNone || reason == lldb::eStopReasonInvalid)
            {
                continue;
            }

            state.process.SetSelectedThread(thread);

            const auto frame = thread.GetFrameAtIndex(0);
            const std::uint64_t address = frame.IsValid() ? frame.GetPC() : 0;
            const auto tid = static_cast<std::uint32_t>(thread.GetThreadID());

            switch (reason)
            {
            case lldb::eStopReasonBreakpoint:
            {
                const auto lldbBpId = static_cast<lldb::break_id_t>(thread.GetStopReasonDataAtIndex(0));
                std::uint32_t vertexId{};
                bool shouldPause{true};

                {
                    std::scoped_lock bpLock{state.breakpointMutex};
                    vertexId = Debugger::find_vertex_breakpoint_id(state, lldbBpId);

                    if (vertexId != 0)
                    {
                        if (auto it = state.breakpoints.find(vertexId); it != state.breakpoints.end())
                        {
                            auto bp = state.target.FindBreakpointByID(lldbBpId);
                            const auto hitCount = bp.IsValid() ? bp.GetHitCount() : 0;
                            shouldPause = evaluate_hit_count_condition(it->second.condition, hitCount);
                        }
                    }
                }

                if (!shouldPause)
                {
                    state.process.Continue();
                    return StatusCode::STATUS_DEBUG_TICK_PROCESSED;
                }

                std::scoped_lock cbLock{state.callbackMutex};
                if (state.callbacks.has_value() && state.callbacks->on_breakpoint_hit != nullptr)
                {
                    const auto& cb = state.callbacks.value();
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_BREAKPOINT;
                    debugEvent.threadId = tid;
                    debugEvent.address = address;
                    debugEvent.breakpointId = vertexId;
                    debugEvent.firstChance = 1;
                    cb.on_breakpoint_hit(&debugEvent, cb.user_data);
                }
                return StatusCode::STATUS_DEBUG_TICK_PAUSED;
            }
            case lldb::eStopReasonWatchpoint:
            {
                const auto lldbWpId = static_cast<lldb::watch_id_t>(thread.GetStopReasonDataAtIndex(0));
                std::uint32_t vertexId{};

                {
                    std::scoped_lock bpLock{state.breakpointMutex};
                    vertexId = Debugger::find_vertex_watchpoint_id(state, lldbWpId);
                }

                if (vertexId != 0 && debugger::should_throttle_watchpoint(vertexId))
                {
                    state.process.Continue();
                    return StatusCode::STATUS_DEBUG_TICK_PROCESSED;
                }

                {
                    std::scoped_lock cbLock{state.callbackMutex};
                    if (state.callbacks.has_value() && state.callbacks->on_watchpoint_hit != nullptr)
                    {
                        const auto& cb = state.callbacks.value();
                        WatchpointEvent wpEvent{};
                        wpEvent.breakpointId = vertexId;
                        wpEvent.threadId = tid;
                        wpEvent.address = address;
                        wpEvent.accessAddress = address;
                        cb.on_watchpoint_hit(&wpEvent, cb.user_data);
                    }
                }

                if (state.continueOnWatchpointHit.load(std::memory_order_relaxed))
                {
                    state.process.Continue();
                    return StatusCode::STATUS_DEBUG_TICK_PROCESSED;
                }

                return StatusCode::STATUS_DEBUG_TICK_PAUSED;
            }
            case lldb::eStopReasonPlanComplete:
            case lldb::eStopReasonTrace:
            {
                std::scoped_lock lock{state.callbackMutex};
                if (state.callbacks.has_value() && state.callbacks->on_single_step != nullptr)
                {
                    const auto& cb = state.callbacks.value();
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_SINGLE_STEP;
                    debugEvent.threadId = tid;
                    debugEvent.address = address;
                    debugEvent.firstChance = 1;
                    cb.on_single_step(&debugEvent, cb.user_data);
                }
                return StatusCode::STATUS_DEBUG_TICK_PAUSED;
            }
            case lldb::eStopReasonSignal:
            {
                const auto signo = static_cast<int>(thread.GetStopReasonDataAtIndex(0));

                std::scoped_lock lock{state.callbackMutex};
                if (state.callbacks.has_value() && state.callbacks->on_exception != nullptr)
                {
                    const auto& cb = state.callbacks.value();
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_EXCEPTION;
                    debugEvent.threadId = tid;
                    debugEvent.address = address;
                    debugEvent.exceptionCode = static_cast<std::uint32_t>(signo);
                    debugEvent.firstChance = 1;

                    thread.GetStopDescription(debugEvent.description, sizeof(debugEvent.description));

                    cb.on_exception(&debugEvent, cb.user_data);
                }
                return StatusCode::STATUS_DEBUG_TICK_PAUSED;
            }
            default:
                break;
            }
        }

        return StatusCode::STATUS_DEBUG_TICK_PAUSED;
    }
}

std::uint32_t get_current_debug_thread_id()
{
    auto& state = Debugger::get_backend_state();
    if (!state.process.IsValid())
    {
        return 0;
    }
    auto thread = state.process.GetSelectedThread();
    return thread.IsValid() ? static_cast<std::uint32_t>(thread.GetThreadID()) : 0;
}

std::uint32_t get_attached_process_id()
{
    auto& state = Debugger::get_backend_state();
    if (!state.process.IsValid())
    {
        return 0;
    }
    return static_cast<std::uint32_t>(state.process.GetProcessID());
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_attach()
    {
        auto& state = Debugger::get_backend_state();

        const native_handle pid = get_native_handle();
        if (pid == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        if (state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ALREADY_ATTACHED;
        }

        if (!state.debugger.IsValid())
        {
            Debugger::initialize_lldb();
        }

        const auto exeLink = std::format("/proc/{}/exe", pid);
        std::array<char, 4096> resolvedPath{};
        const auto len = readlink(exeLink.c_str(), resolvedPath.data(), resolvedPath.size() - 1);

        state.target = state.debugger.CreateTarget(len > 0 ? resolvedPath.data() : nullptr);
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ATTACH_FAILED;
        }

        lldb::SBAttachInfo attachInfo{};
        attachInfo.SetProcessID(static_cast<lldb::pid_t>(pid));
        attachInfo.SetWaitForLaunch(false);

        lldb::SBError error{};
        state.process = state.target.Attach(attachInfo, error);

        if (error.Fail() || !state.process.IsValid())
        {
            if (g_pluginRuntime != nullptr)
            {
                g_pluginRuntime->vertex_log_error("LLDB attach failed: %s",
                    error.GetCString() != nullptr ? error.GetCString() : "unknown error");
            }
            state.debugger.DeleteTarget(state.target);
            state.target = lldb::SBTarget{};
            return StatusCode::STATUS_ERROR_DEBUGGER_ATTACH_FAILED;
        }

        cache_process_architecture();

        // TODO: Add some option in the ui to still allow handling those exceptions for users that might want
        // to have it for whatever reason, personally, i don't recommend it since LLDB (and other debuggers in general)
        // halt a lot on wine / proton processes because of these signals.
        //
        // might as well add an option to allow users to specify what signals they'd like to ignore.
        state.isWineProcess = detect_wine_process(static_cast<std::uint32_t>(pid));
        if (state.isWineProcess)
        {
            auto signals = state.process.GetUnixSignals();
            if (signals.IsValid())
            {
                signals.SetShouldStop(SIGALRM, false);
                signals.SetShouldNotify(SIGALRM, false);
                signals.SetShouldStop(SIGUSR1, false);
                signals.SetShouldNotify(SIGUSR1, false);
            }

            if (g_pluginRuntime != nullptr)
            {
                g_pluginRuntime->vertex_log_info("Wine/Proton process detected (PID %u), suppressing periodic signals",
                    static_cast<unsigned>(pid));
            }
        }

        enumerate_initial_modules(state);

        lldb::SBError resumeError = state.process.Continue();
        if (resumeError.Fail())
        {
            if (g_pluginRuntime != nullptr)
            {
                g_pluginRuntime->vertex_log_error("LLDB failed to resume process after attach: %s",
                    resumeError.GetCString() != nullptr ? resumeError.GetCString() : "unknown error");
            }

            state.process.Detach();
            state.debugger.DeleteTarget(state.target);
            state.process = lldb::SBProcess{};
            state.target = lldb::SBTarget{};
            clear_process_architecture();

            return StatusCode::STATUS_ERROR_DEBUGGER_ATTACH_FAILED;
        }

        if (g_pluginRuntime != nullptr)
        {
            g_pluginRuntime->vertex_log_info("LLDB attached to process %llu",
                static_cast<unsigned long long>(state.process.GetProcessID()));
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_detach()
    {
        auto& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        const lldb::SBError error = state.process.Detach();

        {
            std::scoped_lock lock{state.breakpointMutex};
            state.breakpoints.clear();
            state.nextBreakpointId = 1;
            state.watchpoints.clear();
            state.nextWatchpointId = 1;
        }

        debugger::clear_watchpoint_throttle_state();

        if (state.target.IsValid())
        {
            state.debugger.DeleteTarget(state.target);
        }

        state.process = lldb::SBProcess{};
        state.target = lldb::SBTarget{};
        state.isWineProcess = false;

        clear_process_architecture();

        if (g_pluginRuntime != nullptr)
        {
            g_pluginRuntime->vertex_log_info("LLDB detached from process");
        }

        return error.Fail()
            ? StatusCode::STATUS_ERROR_DEBUGGER_DETACH_FAILED
            : StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_callbacks(const DebuggerCallbacks* callbacks)
    {
        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.callbackMutex};

        if (callbacks != nullptr)
        {
            state.callbacks = *callbacks;
        }
        else
        {
            state.callbacks.reset();
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_tick(const std::uint32_t timeout_ms)
    {
        auto&& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        const auto processState = state.process.GetState();
        if (processState == lldb::eStateExited || processState == lldb::eStateCrashed)
        {
            const auto exitCode = state.process.GetExitStatus();

            {
                std::scoped_lock lock{state.callbackMutex};
                if (state.callbacks.has_value() && state.callbacks->on_process_exited != nullptr)
                {
                    const auto& cb = state.callbacks.value();
                    cb.on_process_exited(static_cast<std::int32_t>(exitCode), cb.user_data);
                }
            }

            return StatusCode::STATUS_DEBUG_TICK_PROCESS_EXITED;
        }

        lldb::SBEvent event{};
        const auto timeoutSec = (timeout_ms == VERTEX_INFINITE_WAIT)
            ? std::numeric_limits<std::uint32_t>::max()
            : timeout_ms;

        const bool gotEvent = state.listener.WaitForEvent(static_cast<std::uint32_t>(timeoutSec) / 1000, event);

        if (!gotEvent)
        {
            return StatusCode::STATUS_DEBUG_TICK_NO_EVENT;
        }

        if (lldb::SBProcess::EventIsProcessEvent(event))
        {
            const auto newState = lldb::SBProcess::GetStateFromEvent(event);

            switch (newState)
            {
            case lldb::eStateStopped:
                return dispatch_stop_event(state);

            case lldb::eStateExited:
            {
                const auto exitCode = state.process.GetExitStatus();

                {
                    std::scoped_lock lock{state.callbackMutex};
                    if (state.callbacks.has_value() && state.callbacks->on_process_exited != nullptr)
                    {
                        const auto& cb = state.callbacks.value();
                        cb.on_process_exited(static_cast<std::int32_t>(exitCode), cb.user_data);
                    }
                }

                return StatusCode::STATUS_DEBUG_TICK_PROCESS_EXITED;
            }
            case lldb::eStateRunning:
            case lldb::eStateStepping:
                return StatusCode::STATUS_DEBUG_TICK_PROCESSED;

            case lldb::eStateCrashed:
                return dispatch_stop_event(state);

            case lldb::eStateDetached:
                return StatusCode::STATUS_DEBUG_TICK_DETACHED;

            default:
                return StatusCode::STATUS_DEBUG_TICK_PROCESSED;
            }
        }

        return StatusCode::STATUS_DEBUG_TICK_PROCESSED;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_continue(const std::uint8_t passException)
    {
        auto& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        if (passException != 0)
        {
            auto thread = state.process.GetSelectedThread();
            if (thread.IsValid() && thread.GetStopReason() == lldb::eStopReasonSignal)
            {
                const auto signo = static_cast<int>(thread.GetStopReasonDataAtIndex(0));
                if (signo > 0)
                {
                    state.process.Signal(signo);
                }
            }
        }

        const lldb::SBError error = state.process.Continue();
        if (error.Fail())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_CONTINUE_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_pause()
    {
        auto& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        const lldb::SBError error = state.process.Stop();
        if (error.Fail())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_BREAK_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_step(const StepMode mode)
    {
        const auto& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        auto thread = state.process.GetSelectedThread();
        if (!thread.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        switch (mode)
        {
        case VertexStepMode::VERTEX_STEP_INTO:
            thread.StepInstruction(false);
            break;
        case VertexStepMode::VERTEX_STEP_OVER:
            thread.StepInstruction(true);
            break;
        case VertexStepMode::VERTEX_STEP_OUT:
            thread.StepOut();
            break;
        default:
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_run_to_address(const std::uint64_t address)
    {
        const auto& state = Debugger::get_backend_state();

        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        auto thread = state.process.GetSelectedThread();
        if (!thread.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        thread.RunToAddress(address);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_instruction_pointer(
        const std::uint32_t threadId, std::uint64_t* address)
    {
        if (address == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        auto thread = state.process.GetThreadByID(static_cast<lldb::tid_t>(threadId));
        if (!thread.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        const auto frame = thread.GetFrameAtIndex(0);
        if (!frame.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
        }

        *address = frame.GetPC();
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_instruction_pointer(
        const std::uint32_t threadId, const std::uint64_t address)
    {
        auto& state = Debugger::get_backend_state();
        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        auto thread = state.process.GetThreadByID(static_cast<lldb::tid_t>(threadId));
        if (!thread.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        auto frame = thread.GetFrameAtIndex(0);
        if (!frame.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
        }

        if (!frame.SetPC(address))
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_CONTEXT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }
}
