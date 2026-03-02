//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <Windows.h>
#include <algorithm>
namespace debugger
{
    TickEventResult handle_output_string(TickState& state, const DEBUG_EVENT& event)
    {
        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value() && state.callbacks.value().on_output_string != nullptr)
            {
                const auto& cb = state.callbacks.value();
                OutputStringEvent outputEvent{};
                outputEvent.threadId = event.dwThreadId;

                const auto& [lpDebugStringData, fUnicode, nDebugStringLength] = event.u.DebugString;
                const auto remotePtr = reinterpret_cast<std::uint64_t>(lpDebugStringData);
                constexpr std::size_t maxChars = VERTEX_MAX_EXCEPTION_DESC_LENGTH - 1;

                bool readOk = false;

                if (fUnicode)
                {
                    const std::size_t wcharCount = std::min<std::size_t>(nDebugStringLength, maxChars);
                    std::array<wchar_t, VERTEX_MAX_EXCEPTION_DESC_LENGTH> wbuf{};
                    if (read_process_memory(remotePtr, wbuf.data(), wcharCount * sizeof(wchar_t)))
                    {
                        const int written = WideCharToMultiByte(CP_UTF8, 0, wbuf.data(),
                            static_cast<int>(wcharCount), outputEvent.message,
                            static_cast<int>(maxChars), nullptr, nullptr);
                        outputEvent.message[written > 0 ? written : 0] = '\0';
                        readOk = true;
                    }
                }
                else
                {
                    const std::size_t byteCount = std::min<std::size_t>(nDebugStringLength, maxChars);
                    if (read_process_memory(remotePtr, outputEvent.message, byteCount))
                    {
                        outputEvent.message[byteCount] = '\0';
                        readOk = true;
                    }
                }

                if (readOk)
                {
                    cb.on_output_string(&outputEvent, cb.user_data);
                }
            }
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE};
    }
}
