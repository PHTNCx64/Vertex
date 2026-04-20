//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/accesstrackermodel.hh>

#include <vertex/debugger/engine_command.hh>

#include <utility>
#include <variant>

namespace Vertex::Model
{
    namespace service = Vertex::Debugger::service;

    AccessTrackerModel::AccessTrackerModel(Debugger::IDebuggerRuntimeService& runtime)
        : m_runtime {runtime}
    {
    }

    Runtime::CommandId
    AccessTrackerModel::start_tracking(std::uint64_t address,
                                        std::uint32_t size,
                                        StartCompletion onComplete)
    {
        const auto id = m_runtime.send_command(
            service::CmdAddWatchpoint {
                .address = address,
                .size = size,
                .type = Debugger::WatchpointType::ReadWrite
            });

        if (id == Runtime::INVALID_COMMAND_ID)
        {
            if (onComplete)
            {
                onComplete(StartTrackingResult {
                    .status = StatusCode::STATUS_SHUTDOWN,
                    .watchpointId = 0,
                });
            }
            return Runtime::INVALID_COMMAND_ID;
        }

        m_runtime.subscribe_result(id,
            [cb = std::move(onComplete)](const service::CommandResult& result)
            {
                if (!cb)
                {
                    return;
                }
                if (result.code != StatusCode::STATUS_OK)
                {
                    cb(StartTrackingResult {.status = result.code, .watchpointId = 0});
                    return;
                }
                if (const auto* payload = std::get_if<service::AddWatchpointResultPayload>(&result.payload))
                {
                    cb(StartTrackingResult {
                        .status = StatusCode::STATUS_OK,
                        .watchpointId = payload->watchpointId,
                    });
                }
                else
                {
                    cb(StartTrackingResult {
                        .status = StatusCode::STATUS_ERROR_GENERAL,
                        .watchpointId = 0,
                    });
                }
            });

        return id;
    }

    Runtime::CommandId
    AccessTrackerModel::stop_tracking(std::uint32_t watchpointId, StopCompletion onComplete)
    {
        const auto id = m_runtime.send_command(
            service::CmdRemoveWatchpoint {.id = watchpointId});

        if (id == Runtime::INVALID_COMMAND_ID)
        {
            if (onComplete)
            {
                onComplete(StatusCode::STATUS_SHUTDOWN);
            }
            return Runtime::INVALID_COMMAND_ID;
        }

        m_runtime.subscribe_result(id,
            [cb = std::move(onComplete)](const service::CommandResult& result)
            {
                if (cb)
                {
                    cb(result.code);
                }
            });

        return id;
    }

}
