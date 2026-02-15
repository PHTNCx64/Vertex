//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>
#include <sdk/api.h>

#include <Windows.h>
#include <ranges>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_breakpoint(const std::uint64_t address,
                                                                        const BreakpointType type,
                                                                        std::uint32_t* breakpointId)
    {
        if (address == 0 || breakpointId == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        if (type == VERTEX_BP_EXECUTE)
        {
            return debugger::set_software_breakpoint(address, breakpointId);
        }

        return debugger::set_hardware_breakpoint(address, type, 1, breakpointId);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_breakpoint(const std::uint32_t breakpointId)
    {
        bool isSoftware = false;
        bool isHardware = false;

        {
            auto& manager = debugger::get_breakpoint_manager();
            std::scoped_lock lock{manager.mutex};
            isSoftware = manager.softwareBreakpoints.contains(breakpointId);
            isHardware = manager.hardwareBreakpoints.contains(breakpointId);
        }

        if (isSoftware)
        {
            return debugger::remove_software_breakpoint(breakpointId);
        }

        if (isHardware)
        {
            return debugger::remove_hardware_breakpoint(breakpointId);
        }

        return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_breakpoint(const std::uint32_t breakpointId,
                                                                           const std::uint8_t enable)
    {
        bool isSoftware = false;
        bool isHardware = false;

        {
            auto& manager = debugger::get_breakpoint_manager();
            std::scoped_lock lock{manager.mutex};
            isSoftware = manager.softwareBreakpoints.contains(breakpointId);
            isHardware = manager.hardwareBreakpoints.contains(breakpointId);
        }

        if (isSoftware)
        {
            return debugger::enable_software_breakpoint(breakpointId, enable != 0);
        }

        if (isHardware)
        {
            return debugger::enable_hardware_breakpoint(breakpointId, enable != 0);
        }

        return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_watchpoint(const Watchpoint* watchpoint, std::uint32_t* watchpointId)
    {
        if (watchpoint == nullptr || watchpointId == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        return debugger::set_watchpoint(watchpoint->address, watchpoint->size, watchpoint->type, watchpointId);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_remove_watchpoint(const std::uint32_t watchpointId)
    {
        return debugger::remove_watchpoint(watchpointId);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_enable_watchpoint(const std::uint32_t watchpointId, const std::uint8_t enable)
    {
        return debugger::enable_watchpoint(watchpointId, enable != 0);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoints(WatchpointInfo** watchpoints, std::uint32_t* count)
    {
        if (watchpoints == nullptr || count == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        std::vector<debugger::WatchpointData> wpList{};
        const auto result = debugger::get_all_watchpoints(wpList);
        if (result != STATUS_OK)
        {
            return result;
        }

        if (wpList.empty())
        {
            *watchpoints = nullptr;
            *count = 0;
            return STATUS_OK;
        }

        auto* output = static_cast<WatchpointInfo*>(std::malloc(wpList.size() * sizeof(WatchpointInfo)));
        if (output == nullptr)
        {
            return STATUS_ERROR_OUT_OF_MEMORY;
        }

        for (const auto& [idx, wp] : wpList | std::views::enumerate)
        {
            WatchpointInfo info{};
            info.id = wp.id;
            info.address = wp.address;
            info.size = wp.size;
            info.type = wp.type;
            info.enabled = wp.enabled ? 1 : 0;
            info.hwRegisterIndex = wp.registerIndex;
            info.hitCount = wp.hitCount;
            output[idx] = info;
        }

        *watchpoints = output;
        *count = static_cast<std::uint32_t>(wpList.size());
        return STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoint_hit_count(const std::uint32_t watchpointId, std::uint32_t* hitCount)
    {
        if (hitCount == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        debugger::WatchpointData data{};
        const auto result = debugger::get_watchpoint_info(watchpointId, &data);
        if (result != STATUS_OK)
        {
            return result;
        }

        *hitCount = data.hitCount;
        return STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_reset_watchpoint_hit_count(const std::uint32_t watchpointId)
    {
        return debugger::reset_watchpoint_hit_count(watchpointId);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_breakpoints(BreakpointInfo** breakpoints, std::uint32_t* count)
    {
        if (breakpoints == nullptr || count == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& manager = debugger::get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const std::size_t totalCount = manager.softwareBreakpoints.size() + manager.hardwareBreakpoints.size();

        if (totalCount == 0)
        {
            *breakpoints = nullptr;
            *count = 0;
            return STATUS_OK;
        }

        auto* result = static_cast<BreakpointInfo*>(std::malloc(totalCount * sizeof(BreakpointInfo)));
        if (result == nullptr)
        {
            return STATUS_ERROR_OUT_OF_MEMORY;
        }

        std::size_t index = 0;

        for (const auto& [id, address, type, state, originalByte, hitCount, temporary] : manager.softwareBreakpoints | std::views::values)
        {
            BreakpointInfo info{};
            info.id = id;
            info.address = address;
            info.type = type;
            info.state = state;
            info.hitCount = hitCount;
            info.temporary = temporary ? 1 : 0;
            info.originalByte = originalByte;
            info.hwRegisterIndex = 0xFF;
            result[index++] = info;
        }

        for (const auto& bp : manager.hardwareBreakpoints | std::views::values)
        {
            BreakpointInfo info{};
            info.id = bp.id;
            info.address = bp.address;
            info.type = bp.type;
            info.state = bp.state;
            info.hitCount = bp.hitCount;
            info.temporary = 0;
            info.originalByte = 0;
            info.hwRegisterIndex = bp.registerIndex;
            result[index++] = info;
        }

        *breakpoints = result;
        *count = static_cast<std::uint32_t>(totalCount);

        return STATUS_OK;
    }
}
