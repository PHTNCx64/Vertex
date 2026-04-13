//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/native_handle.hh>

#include <windows.h>

#include <format>

extern ProcessArchitecture get_process_architecture();

namespace debugger
{
    namespace
    {
        [[nodiscard]] bool evaluate_hw_condition(const ::BreakpointCondition& condition, const std::uint32_t hitCount)
        {
            switch (condition.type)
            {
            case VERTEX_BP_COND_NONE:
            case VERTEX_BP_COND_EXPRESSION:
                return true;
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

        constexpr std::uint64_t DR6_STATUS_MASK = 0xFULL;

        std::uint64_t read_dr6_and_clear(const DWORD threadId)
        {
            const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
            const HANDLE cached = get_cached_thread_handle(threadId);
            const HANDLE threadHandle = cached != nullptr ? cached
                : OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, threadId);
            if (threadHandle == nullptr)
            {
                return 0;
            }

            std::uint64_t dr6Value{};

            if (isWow64)
            {
                WOW64_CONTEXT context{};
                context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;
                if (Wow64GetThreadContext(threadHandle, &context))
                {
                    dr6Value = context.Dr6;
                    if ((dr6Value & DR6_STATUS_MASK) != 0)
                    {
                        context.Dr6 = 0;
                        Wow64SetThreadContext(threadHandle, &context);
                    }
                }
            }
            else
            {
                alignas(16) CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(threadHandle, &context))
                {
                    dr6Value = context.Dr6;
                    if ((dr6Value & DR6_STATUS_MASK) != 0)
                    {
                        context.Dr6 = 0;
                        SetThreadContext(threadHandle, &context);
                    }
                }
            }

            if (cached == nullptr)
            {
                CloseHandle(threadHandle);
            }

            return dr6Value;
        }
    }

    TickEventResult handle_exception_single_step(TickState& state, const DEBUG_EVENT& event)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        state.currentThreadId = event.dwThreadId;

        auto logMsg = std::format("[Vertex] SingleStep: addr=0x{:016X}\n", exceptionAddress);
        OutputDebugStringA(logMsg.c_str());

        std::uint64_t bpAddress{};
        DebugCommand pendingCmd{DebugCommand::None};
        std::optional<std::uint64_t> precomputedTarget{};
        if (is_stepping_over_breakpoint(event.dwThreadId, &bpAddress, &pendingCmd, &precomputedTarget))
        {
            clear_breakpoint_step_over(event.dwThreadId);
            std::ignore = reapply_breakpoint_byte(bpAddress);

            if (is_temp_breakpoint_hit(exceptionAddress))
            {
                if (!remove_temp_breakpoint())
                {
                    return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
                }

                {
                    std::scoped_lock lock{state.callbackMutex};
                    if (state.callbacks.has_value())
                    {
                        const auto& cb = state.callbacks.value();
                        if (cb.on_single_step != nullptr)
                        {
                            DebugEvent debugEvent{};
                            debugEvent.type = VERTEX_DBG_EVENT_SINGLE_STEP;
                            debugEvent.threadId = event.dwThreadId;
                            debugEvent.address = exceptionAddress;
                            debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                            debugEvent.firstChance = dwFirstChance != 0;
                            cb.on_single_step(&debugEvent, cb.user_data);
                        }
                    }
                }

                return TickEventResult{
                    .shouldPause = true,
                    .pauseReason = PauseReason::TempBreakpoint,
                    .pauseAddress = exceptionAddress
                };
            }

            switch (pendingCmd)
            {
            case DebugCommand::Continue:
            case DebugCommand::RunToAddress:
            {
                if (pendingCmd == DebugCommand::RunToAddress && state.pendingTargetAddress != 0)
                {
                    std::ignore = set_temp_breakpoint(state.pendingTargetAddress);
                    state.pendingTargetAddress = 0;
                }
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
            }
            case DebugCommand::StepInto:
            {
                if (!set_trap_flag(event.dwThreadId, true))
                {
                    return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
                }
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
            }
            case DebugCommand::StepOver:
            {
                if (precomputedTarget.has_value())
                {
                    if (set_temp_breakpoint(precomputedTarget.value()))
                    {
                        return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
                    }
                }
                if (!set_trap_flag(event.dwThreadId, true))
                {
                    return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
                }
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
            }
            case DebugCommand::StepOut:
            {
                if (precomputedTarget.has_value())
                {
                    if (set_temp_breakpoint(precomputedTarget.value()))
                    {
                        return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
                    }
                }
                if (!set_trap_flag(event.dwThreadId, true))
                {
                    return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
                }
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
            }
            default:
            {
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
            }
            }
        }

        std::uint32_t watchpointId{};
        if (is_stepping_over_watchpoint(event.dwThreadId, &watchpointId))
        {
            clear_watchpoint_step_over(event.dwThreadId);
            std::ignore = re_enable_watchpoint_on_all_threads(watchpointId);
            return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
        }

        const std::uint64_t dr6Value = read_dr6_and_clear(event.dwThreadId);
        if ((dr6Value & DR6_STATUS_MASK) != 0)
        {
            std::uint32_t hitHwBreakpointId{};
            std::uint64_t hwBreakpointAddress{};
            ::BreakpointCondition hwCondition{};
            std::uint32_t hwHitCount{};

            if (is_hardware_breakpoint_hit_by_dr6(dr6Value, &hitHwBreakpointId, &hwBreakpointAddress, &hwCondition, &hwHitCount))
            {
                if (!evaluate_hw_condition(hwCondition, hwHitCount))
                {
                    return TickEventResult{.continueStatus = DBG_CONTINUE};
                }

                {
                    std::scoped_lock lock{state.callbackMutex};
                    if (state.callbacks.has_value())
                    {
                        const auto& cb = state.callbacks.value();
                        if (cb.on_breakpoint_hit != nullptr)
                        {
                            DebugEvent debugEvent{};
                            debugEvent.type = VERTEX_DBG_EVENT_BREAKPOINT;
                            debugEvent.threadId = event.dwThreadId;
                            debugEvent.address = hwBreakpointAddress;
                            debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                            debugEvent.firstChance = dwFirstChance != 0;
                            debugEvent.breakpointId = hitHwBreakpointId;
                            cb.on_breakpoint_hit(&debugEvent, cb.user_data);
                        }
                    }
                }

                return TickEventResult{
                    .shouldPause = true,
                    .pauseReason = PauseReason::HardwareBreakpoint,
                    .pauseAddress = hwBreakpointAddress,
                    .pauseBreakpointId = hitHwBreakpointId
                };
            }

            std::uint32_t hitWatchpointId{};
            WatchpointType watchType{};
            std::uint64_t watchedAddress{};
            std::uint32_t watchedSize{};

            if (is_watchpoint_hit(dr6Value, &hitWatchpointId, &watchType, &watchedAddress, &watchedSize))
            {
                {
                    std::scoped_lock lock{state.callbackMutex};
                    if (state.callbacks.has_value())
                    {
                        const auto& cb = state.callbacks.value();
                        if (cb.on_watchpoint_hit != nullptr)
                        {
                            WatchpointEvent wpEvent{};
                            wpEvent.breakpointId = hitWatchpointId;
                            wpEvent.threadId = event.dwThreadId;
                            wpEvent.address = watchedAddress;
                            wpEvent.accessAddress = exceptionAddress;
                            wpEvent.type = watchType;
                            wpEvent.size = static_cast<std::uint8_t>(watchedSize);
                            cb.on_watchpoint_hit(&wpEvent, cb.user_data);
                        }
                    }
                }

                return TickEventResult{
                    .shouldPause = true,
                    .pauseReason = PauseReason::Watchpoint,
                    .pauseAddress = exceptionAddress,
                    .pauseWatchpointId = hitWatchpointId
                };
            }
        }

        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value())
            {
                const auto& cb = state.callbacks.value();
                if (cb.on_single_step != nullptr)
                {
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_SINGLE_STEP;
                    debugEvent.threadId = event.dwThreadId;
                    debugEvent.address = exceptionAddress;
                    debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                    debugEvent.firstChance = dwFirstChance != 0;
                    cb.on_single_step(&debugEvent, cb.user_data);
                }
            }
        }

        return TickEventResult{
            .shouldPause = true,
            .pauseReason = PauseReason::SingleStep,
            .pauseAddress = exceptionAddress
        };
    }
}
