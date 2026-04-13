//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <windows.h>

#include <format>

namespace debugger
{
    namespace
    {
        [[nodiscard]] bool evaluate_condition(const ::BreakpointCondition& condition, const std::uint32_t hitCount)
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
    }

    TickEventResult handle_exception_breakpoint(TickState& state, const DEBUG_EVENT& event)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        state.currentThreadId = event.dwThreadId;

        auto logMsg = std::format("[Vertex] Breakpoint: addr=0x{:016X}\n", exceptionAddress);
        OutputDebugStringA(logMsg.c_str());

        if (state.initialBreakpointPending)
        {
            state.initialBreakpointPending = false;
            OutputDebugStringA("[Vertex] Initial attach breakpoint consumed\n");
            return TickEventResult{.continueStatus = DBG_CONTINUE};
        }

        if (is_temp_breakpoint_hit(exceptionAddress))
        {
            if (!remove_temp_breakpoint())
            {
                return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
            }

            if (!decrement_instruction_pointer(event.dwThreadId))
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

        std::uint32_t userBreakpointId{};
        ::BreakpointCondition bpCondition{};
        std::uint32_t bpHitCount{};
        const bool isUserBreakpoint = is_user_breakpoint_hit(exceptionAddress, &userBreakpointId, &bpCondition, &bpHitCount);

        if (isUserBreakpoint)
        {
            const auto restoreResult = restore_breakpoint_byte(exceptionAddress);
            if (restoreResult != STATUS_OK)
            {
                return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
            }

            if (!decrement_instruction_pointer(event.dwThreadId))
            {
                return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
            }

            if (!evaluate_condition(bpCondition, bpHitCount))
            {
                set_breakpoint_step_over(exceptionAddress, event.dwThreadId);
                std::ignore = set_trap_flag(event.dwThreadId, true);
                return TickEventResult{.continueStatus = DBG_CONTINUE, .isInternal = true};
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
                        debugEvent.address = exceptionAddress;
                        debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                        debugEvent.firstChance = dwFirstChance ? 1 : 0;
                        debugEvent.breakpointId = userBreakpointId;
                        cb.on_breakpoint_hit(&debugEvent, cb.user_data);
                    }
                }
            }

            return TickEventResult{
                .shouldPause = true,
                .pauseReason = PauseReason::UserBreakpoint,
                .pauseAddress = exceptionAddress,
                .pauseBreakpointId = userBreakpointId
            };
        }

        const bool isPauseRequested = state.pauseRequested;
        state.pauseRequested = false;

        if (!isPauseRequested)
        {
            OutputDebugStringA("[Vertex] Breakpoint: not user BP, not pause requested, returning DBG_CONTINUE\n");
            return TickEventResult{.continueStatus = DBG_CONTINUE};
        }

        return TickEventResult{
            .shouldPause = true,
            .pauseReason = PauseReason::PauseRequested,
            .pauseAddress = exceptionAddress
        };
    }
}
