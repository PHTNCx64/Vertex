//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>

#include <windows.h>

#include <algorithm>
#include <format>

namespace debugger
{
    TickEventResult handle_exception_general(TickState& state, const DEBUG_EVENT& event)
    {
        const auto& [ExceptionRecord, dwFirstChance] = event.u.Exception;
        const auto exceptionAddress = reinterpret_cast<std::uint64_t>(ExceptionRecord.ExceptionAddress);

        state.currentThreadId = event.dwThreadId;

        auto logMsg = std::format("[Vertex] GeneralException: code=0x{:08X} addr=0x{:016X} firstChance={}\n",
                                  ExceptionRecord.ExceptionCode, exceptionAddress, dwFirstChance);
        OutputDebugStringA(logMsg.c_str());

        if (dwFirstChance != 0)
        {
            return TickEventResult{.continueStatus = DBG_EXCEPTION_NOT_HANDLED};
        }

        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value())
            {
                const auto& cb = state.callbacks.value();
                if (cb.on_exception != nullptr)
                {
                    DebugEvent debugEvent{};
                    debugEvent.type = VERTEX_DBG_EVENT_EXCEPTION;
                    debugEvent.threadId = event.dwThreadId;
                    debugEvent.address = exceptionAddress;
                    debugEvent.exceptionCode = ExceptionRecord.ExceptionCode;
                    debugEvent.firstChance = dwFirstChance != 0 ? 1 : 0;

                    std::string desc {};
                    if (ExceptionRecord.ExceptionCode == EXCEPTION_ACCESS_VIOLATION &&
                        ExceptionRecord.NumberParameters >= 2)
                    {
                        const char* accessType = ExceptionRecord.ExceptionInformation[0] == 0 ? "reading" :
                                                 ExceptionRecord.ExceptionInformation[0] == 1 ? "writing" : "executing";
                        desc = std::format("Access violation {} address 0x{:016X}",
                                           accessType, ExceptionRecord.ExceptionInformation[1]);
                    }
                    else
                    {
                        desc = std::format("Exception 0x{:08X}", ExceptionRecord.ExceptionCode);
                    }

                    std::copy_n(desc.c_str(),
                                std::min(desc.size() + 1, static_cast<std::size_t>(VERTEX_MAX_EXCEPTION_DESC_LENGTH)),
                                debugEvent.description);

                    cb.on_exception(&debugEvent, cb.user_data);
                }
            }
        }

        return TickEventResult{
            .shouldPause = true,
            .pauseReason = PauseReason::Exception,
            .pauseAddress = exceptionAddress
        };
    }
}
