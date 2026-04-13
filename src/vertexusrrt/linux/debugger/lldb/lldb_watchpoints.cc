//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/linux/lldb_backend.hh>
#include <sdk/api.h>

extern "C"
{
    // ===============================================================================================================//
    // WATCHPOINT API                                                                                                 //
    // ===============================================================================================================//

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_watchpoint(const Watchpoint* watchpoint,
                                                                        std::uint32_t* watchpointId)
    {
        if (watchpoint == nullptr || watchpointId == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        const bool read = (watchpoint->type == VERTEX_WP_READ || watchpoint->type == VERTEX_WP_READWRITE);
        const bool write = (watchpoint->type == VERTEX_WP_WRITE || watchpoint->type == VERTEX_WP_READWRITE);

        lldb::SBError error{};
        auto wp = state.target.WatchAddress(watchpoint->address, watchpoint->size, read, write, error);
        if (error.Fail() || !wp.IsValid())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_SET_FAILED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        Debugger::WatchpointEntry entry{};
        entry.lldbId = wp.GetID();
        entry.address = watchpoint->address;
        entry.size = watchpoint->size;
        entry.type = watchpoint->type;

        const auto vertexId = state.nextWatchpointId++;
        state.watchpoints.emplace(vertexId, entry);
        *watchpointId = vertexId;

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_watchpoint(const std::uint32_t watchpointId)
    {
        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        auto it = state.watchpoints.find(watchpointId);
        if (it == state.watchpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        state.target.DeleteWatchpoint(it->second.lldbId);
        state.watchpoints.erase(it);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_watchpoint(const std::uint32_t watchpointId,
                                                                           const std::uint8_t enable)
    {
        auto& state = Debugger::get_backend_state();
        if (!state.target.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        std::scoped_lock lock{state.breakpointMutex};

        auto it = state.watchpoints.find(watchpointId);
        if (it == state.watchpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        auto wp = state.target.FindWatchpointByID(it->second.lldbId);
        if (!wp.IsValid())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        wp.SetEnabled(enable != 0);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoints(WatchpointInfo** watchpoints,
                                                                         std::uint32_t* count)
    {
        if (watchpoints == nullptr || count == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        state.cachedWatchpointInfos.clear();
        state.cachedWatchpointInfos.reserve(state.watchpoints.size());

        for (const auto& [vertexId, entry] : state.watchpoints)
        {
            WatchpointInfo info{};
            info.id = vertexId;
            info.address = entry.address;
            info.size = entry.size;
            info.type = entry.type;
            info.hwRegisterIndex = 0xFF;

            auto wp = state.target.FindWatchpointByID(entry.lldbId);
            info.enabled = (wp.IsValid() && wp.IsEnabled()) ? 1 : 0;
            info.hitCount = wp.IsValid() ? wp.GetHitCount() : 0;

            state.cachedWatchpointInfos.push_back(info);
        }

        *watchpoints = state.cachedWatchpointInfos.data();
        *count = static_cast<std::uint32_t>(state.cachedWatchpointInfos.size());
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoint_hit_count(const std::uint32_t watchpointId,
                                                                                  std::uint32_t* hitCount)
    {
        if (hitCount == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& state = Debugger::get_backend_state();
        std::scoped_lock lock{state.breakpointMutex};

        const auto it = state.watchpoints.find(watchpointId);
        if (it == state.watchpoints.end())
        {
            return StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        auto wp = state.target.FindWatchpointByID(it->second.lldbId);
        *hitCount = wp.IsValid() ? wp.GetHitCount() : 0;
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_reset_watchpoint_hit_count(
        [[maybe_unused]] const std::uint32_t watchpointId)
    {
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }
}
