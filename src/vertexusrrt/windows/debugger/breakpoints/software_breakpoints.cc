//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <ranges>
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>
#include <vertexusrrt/native_handle.hh>

extern native_handle& get_native_handle();

namespace debugger
{
    StatusCode set_software_breakpoint(const std::uint64_t address, std::uint32_t* breakpointId)
    {
        if (address == 0 || breakpointId == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (const auto& [id, bp] : manager.softwareBreakpoints)
        {
            if (bp.address == address)
            {
                *breakpointId = id;
                return STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS;
            }
        }

        std::uint8_t originalByte = 0;
        if (!read_process_memory(address, &originalByte, 1))
        {
            return STATUS_ERROR_MEMORY_READ_FAILED;
        }

        if (!write_process_memory(address, &INT3_OPCODE, 1))
        {
            return STATUS_ERROR_MEMORY_WRITE_FAILED;
        }

        const std::uint32_t id = manager.nextBreakpointId.fetch_add(1, std::memory_order_relaxed);

        SoftwareBreakpointData bp{};
        bp.id = id;
        bp.address = address;
        bp.type = VERTEX_BP_EXECUTE;
        bp.state = VERTEX_BP_STATE_ENABLED;
        bp.originalByte = originalByte;
        bp.hitCount = 0;
        bp.temporary = false;

        manager.softwareBreakpoints.emplace(id, bp);
        *breakpointId = id;

        return STATUS_OK;
    }

    StatusCode remove_software_breakpoint(const std::uint32_t breakpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.softwareBreakpoints.find(breakpointId);
        if (it == manager.softwareBreakpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        const auto& bp = it->second;

        if (bp.state == VERTEX_BP_STATE_ENABLED)
        {
            if (!write_process_memory(bp.address, &bp.originalByte, 1))
            {
                return STATUS_ERROR_MEMORY_WRITE_FAILED;
            }
        }

        manager.softwareBreakpoints.erase(it);
        return STATUS_OK;
    }

    StatusCode enable_software_breakpoint(const std::uint32_t breakpointId, const bool enable)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.softwareBreakpoints.find(breakpointId);
        if (it == manager.softwareBreakpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        auto& bp = it->second;

        if (enable && bp.state == VERTEX_BP_STATE_DISABLED)
        {
            if (!write_process_memory(bp.address, &INT3_OPCODE, 1))
            {
                return STATUS_ERROR_MEMORY_WRITE_FAILED;
            }
            bp.state = VERTEX_BP_STATE_ENABLED;
        }
        else if (!enable && bp.state == VERTEX_BP_STATE_ENABLED)
        {
            if (!write_process_memory(bp.address, &bp.originalByte, 1))
            {
                return STATUS_ERROR_MEMORY_WRITE_FAILED;
            }
            bp.state = VERTEX_BP_STATE_DISABLED;
        }

        return STATUS_OK;
    }

    std::optional<SoftwareBreakpointData*> find_software_breakpoint_by_address(const std::uint64_t address)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (auto& bp : manager.softwareBreakpoints | std::views::values)
        {
            if (bp.address == address && bp.state == VERTEX_BP_STATE_ENABLED)
            {
                return &bp;
            }
        }

        return std::nullopt;
    }

    bool is_user_breakpoint_hit(const std::uint64_t address, std::uint32_t* breakpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (auto& [id, bp] : manager.softwareBreakpoints)
        {
            if (bp.address == address && bp.state == VERTEX_BP_STATE_ENABLED)
            {
                bp.hitCount++;
                if (breakpointId != nullptr)
                {
                    *breakpointId = id;
                }
                return true;
            }
        }

        return false;
    }

    StatusCode restore_breakpoint_byte(const std::uint64_t address)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (const auto& bp : manager.softwareBreakpoints | std::views::values)
        {
            if (bp.address == address && bp.state == VERTEX_BP_STATE_ENABLED)
            {
                if (!write_process_memory(address, &bp.originalByte, 1))
                {
                    return STATUS_ERROR_MEMORY_WRITE_FAILED;
                }
                return STATUS_OK;
            }
        }

        return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
    }

    StatusCode reapply_breakpoint_byte(const std::uint64_t address)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (const auto& bp : manager.softwareBreakpoints | std::views::values)
        {
            if (bp.address == address && bp.state == VERTEX_BP_STATE_ENABLED)
            {
                if (!write_process_memory(address, &INT3_OPCODE, 1))
                {
                    return STATUS_ERROR_MEMORY_WRITE_FAILED;
                }
                return STATUS_OK;
            }
        }

        return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
    }
}
