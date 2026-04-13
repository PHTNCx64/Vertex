//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/linux/lldb_backend.hh>
#include <sdk/api.h>

extern "C"
{
    // ===============================================================================================================//
    // BREAKPOINT API                                                                                                 //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint(const std::uint64_t address,
                                                                        const BreakpointType type,
                                                                        std::uint32_t* breakpointId)
    {
        if (address == 0 || breakpointId == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        Debugger::BreakpointEntry entry{};
        entry.address = address;
        entry.type = type;

        if (type == VertexBreakpointType::VERTEX_BP_EXECUTE)
        {
            const auto bp = state.target.BreakpointCreateByAddress(address);
            if (!bp.IsValid())
            {
                return StatusCode::STATUS_ERROR_BREAKPOINT_SET_FAILED;
            }
            entry.lldbId = bp.GetID();
            entry.isHardwareData = false;
        }
        else
        {
            const bool read = (type == VertexBreakpointType::VERTEX_BP_READ || type == VertexBreakpointType::VERTEX_BP_READWRITE);
            const bool write = (type == VertexBreakpointType::VERTEX_BP_WRITE || type == VertexBreakpointType::VERTEX_BP_READWRITE);

            lldb::SBError error{};
            auto wp = state.target.WatchAddress(address, 1, read, write, error);
            if (error.Fail() || !wp.IsValid())
            {
                return StatusCode::STATUS_ERROR_BREAKPOINT_SET_FAILED;
            }
            entry.lldbId = wp.GetID();
            entry.isHardwareData = true;
        }

        const auto vertexId = state.nextBreakpointId++;
        state.breakpoints.emplace(vertexId, entry);
        *breakpointId = vertexId;

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_breakpoint(const std::uint32_t breakpointId)
    {
        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        auto it = state.breakpoints.find(breakpointId);
        if (it == state.breakpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        const auto& entry = it->second;

        if (entry.isHardwareData)
        {
            auto wp = state.target.FindWatchpointByID(static_cast<lldb::watch_id_t>(entry.lldbId));
            if (wp.IsValid())
            {
                state.target.DeleteWatchpoint(wp.GetID());
            }
        }
        else
        {
            state.target.BreakpointDelete(entry.lldbId);
        }

        state.breakpoints.erase(it);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_breakpoint(const std::uint32_t breakpointId,
                                                                           const std::uint8_t enable)
    {
        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        const auto it = state.breakpoints.find(breakpointId);
        if (it == state.breakpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        const auto& entry = it->second;
        const bool enabled = enable != 0;

        if (entry.isHardwareData)
        {
            auto wp = state.target.FindWatchpointByID(static_cast<lldb::watch_id_t>(entry.lldbId));
            if (!wp.IsValid())
            {
                return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
            }
            wp.SetEnabled(enabled);
        }
        else
        {
            auto bp = state.target.FindBreakpointByID(entry.lldbId);
            if (!bp.IsValid())
            {
                return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
            }
            bp.SetEnabled(enabled);
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoints(BreakpointInfo** breakpoints,
                                                                         std::uint32_t* count)
    {
        if (breakpoints == nullptr || count == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        state.cachedBreakpointInfos.clear();
        state.cachedBreakpointInfos.reserve(state.breakpoints.size());

        for (const auto& [vertexId, entry] : state.breakpoints)
        {
            BreakpointInfo info{};
            info.id = vertexId;
            info.address = entry.address;
            info.type = entry.type;
            info.temporary = 0;
            info.originalByte = 0;

            if (entry.isHardwareData)
            {
                auto wp = state.target.FindWatchpointByID(static_cast<lldb::watch_id_t>(entry.lldbId));
                info.state = (wp.IsValid() && wp.IsEnabled())
                    ? VertexBreakpointState::VERTEX_BP_STATE_ENABLED
                    : VertexBreakpointState::VERTEX_BP_STATE_DISABLED;
                info.hitCount = wp.IsValid() ? wp.GetHitCount() : 0;
            }
            else
            {
                auto bp = state.target.FindBreakpointByID(entry.lldbId);
                info.state = (bp.IsValid() && bp.IsEnabled())
                    ? VertexBreakpointState::VERTEX_BP_STATE_ENABLED
                    : VertexBreakpointState::VERTEX_BP_STATE_DISABLED;
                info.hitCount = bp.IsValid() ? bp.GetHitCount() : 0;
            }

            state.cachedBreakpointInfos.push_back(info);
        }

        *breakpoints = state.cachedBreakpointInfos.data();
        *count = static_cast<std::uint32_t>(state.cachedBreakpointInfos.size());
        return StatusCode::STATUS_OK;
    }

    // ===============================================================================================================//
    // CONDITIONAL BREAKPOINT API                                                                                     //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint_condition(const std::uint32_t breakpointId,
                                                                                  const BreakpointCondition* condition)
    {
        if (condition == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        const auto it = state.breakpoints.find(breakpointId);
        if (it == state.breakpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        it->second.condition = *condition;

        if (condition->type == VERTEX_BP_COND_EXPRESSION && !it->second.isHardwareData)
        {
            auto bp = state.target.FindBreakpointByID(it->second.lldbId);
            if (bp.IsValid())
            {
                bp.SetCondition(condition->expression);
            }
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoint_condition(const std::uint32_t breakpointId,
                                                                                  BreakpointCondition* condition)
    {
        if (condition == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        const auto it = state.breakpoints.find(breakpointId);
        if (it == state.breakpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        *condition = it->second.condition;
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_clear_breakpoint_condition(const std::uint32_t breakpointId)
    {
        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        const auto it = state.breakpoints.find(breakpointId);
        if (it == state.breakpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        it->second.condition = {};

        if (!it->second.isHardwareData)
        {
            auto bp = state.target.FindBreakpointByID(it->second.lldbId);
            if (bp.IsValid())
            {
                bp.SetCondition(nullptr);
            }
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint_action(
        [[maybe_unused]] const std::uint32_t breakpointId,
        [[maybe_unused]] const BreakpointAction* action)
    {
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoint_action(
        [[maybe_unused]] const std::uint32_t breakpointId,
        BreakpointAction* action)
    {
        if (action == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        *action = {};
        return StatusCode::STATUS_OK;
    }
}
