//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/runtime/command.hh>

#include <sdk/statuscode.h>

#include <cstdint>
#include <functional>

namespace Vertex::Model
{
    struct StartTrackingResult final
    {
        StatusCode status {StatusCode::STATUS_OK};
        std::uint32_t watchpointId {};
    };

    class AccessTrackerModel final
    {
    public:
        using StartCompletion = std::move_only_function<void(StartTrackingResult) const>;
        using StopCompletion = std::move_only_function<void(StatusCode) const>;

        explicit AccessTrackerModel(Debugger::IDebuggerRuntimeService& runtime);
        ~AccessTrackerModel() = default;

        AccessTrackerModel(const AccessTrackerModel&) = delete;
        AccessTrackerModel& operator=(const AccessTrackerModel&) = delete;
        AccessTrackerModel(AccessTrackerModel&&) = delete;
        AccessTrackerModel& operator=(AccessTrackerModel&&) = delete;

        [[nodiscard]] Runtime::CommandId
        start_tracking(std::uint64_t address, std::uint32_t size, StartCompletion onComplete);

        [[nodiscard]] Runtime::CommandId
        stop_tracking(std::uint32_t watchpointId, StopCompletion onComplete);

        [[nodiscard]] Debugger::IDebuggerRuntimeService& runtime() noexcept { return m_runtime; }

    private:
        Debugger::IDebuggerRuntimeService& m_runtime;
    };
}
