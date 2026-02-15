//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <ranges>
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>

#include <vector>

extern ProcessArchitecture get_process_architecture();

namespace debugger
{
    namespace
    {
        [[nodiscard]] std::optional<std::uint8_t> allocate_hw_register_for_watchpoint()
        {
            auto& manager = get_breakpoint_manager();

            for (std::uint8_t i = 0; i < 4; ++i)
            {
                if (!manager.hwRegisterUsed[i])
                {
                    manager.hwRegisterUsed[i] = true;
                    return i;
                }
            }

            return std::nullopt;
        }

        void free_hw_register_for_watchpoint(const std::uint8_t index)
        {
            auto& manager = get_breakpoint_manager();

            if (index < 4)
            {
                manager.hwRegisterUsed[index] = false;
            }
        }
    }

    StatusCode set_watchpoint(const std::uint64_t address, const std::uint32_t size, const WatchpointType type, std::uint32_t* watchpointId)
    {
        if (address == 0 || watchpointId == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        if (size != 1 && size != 2 && size != 4 && size != 8)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto alignmentMask = static_cast<std::uint64_t>(size - 1);
        if ((address & alignmentMask) != 0)
        {
            return STATUS_ERROR_BREAKPOINT_ADDRESS_MISALIGNED;
        }

        auto& manager = get_breakpoint_manager();

        std::uint32_t id = 0;
        {
            std::scoped_lock lock{manager.mutex};

            const auto registerIndex = allocate_hw_register_for_watchpoint();
            if (!registerIndex.has_value())
            {
                return STATUS_ERROR_BREAKPOINT_LIMIT_REACHED;
            }

            id = manager.nextWatchpointId.fetch_add(1, std::memory_order_relaxed);

            WatchpointData wp{};
            wp.id = id;
            wp.address = address;
            wp.size = size;
            wp.type = type;
            wp.enabled = true;
            wp.temporarilyDisabled = false;
            wp.registerIndex = registerIndex.value();
            wp.hitCount = 0;

            manager.watchpoints.emplace(id, wp);
        }

        *watchpointId = id;

        std::ignore = apply_watchpoint_to_all_threads(id);

        return STATUS_OK;
    }

    StatusCode remove_watchpoint(const std::uint32_t watchpointId)
    {
        auto& manager = get_breakpoint_manager();

        std::uint8_t registerIndex{};
        {
            std::scoped_lock lock{manager.mutex};

            const auto it = manager.watchpoints.find(watchpointId);
            if (it == manager.watchpoints.end())
            {
                return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
            }

            registerIndex = it->second.registerIndex;
            free_hw_register_for_watchpoint(registerIndex);
            manager.watchpoints.erase(it);
        }

        std::ignore = clear_hw_register_on_all_threads(registerIndex);

        return STATUS_OK;
    }

    StatusCode enable_watchpoint(const std::uint32_t watchpointId, const bool enable)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        it->second.enabled = enable;
        return STATUS_OK;
    }

    bool is_watchpoint_hit(const std::uint64_t dr6Value, std::uint32_t* watchpointId, WatchpointType* type, std::uint64_t* watchedAddress, std::uint32_t* watchedSize)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (std::uint8_t regIndex = 0; regIndex < 4; ++regIndex)
        {
            if (dr6Value & (1ULL << regIndex))
            {
                for (auto& [id, wp] : manager.watchpoints)
                {
                    if (wp.registerIndex == regIndex && wp.enabled && !wp.temporarilyDisabled)
                    {
                        wp.hitCount++;

                        if (watchpointId != nullptr)
                        {
                            *watchpointId = id;
                        }
                        if (type != nullptr)
                        {
                            *type = wp.type;
                        }
                        if (watchedAddress != nullptr)
                        {
                            *watchedAddress = wp.address;
                        }
                        if (watchedSize != nullptr)
                        {
                            *watchedSize = wp.size;
                        }
                        return true;
                    }
                }
            }
        }

        return false;
    }

    StatusCode get_watchpoint_info(const std::uint32_t watchpointId, WatchpointData* outData)
    {
        if (outData == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        *outData = it->second;
        return STATUS_OK;
    }

    StatusCode get_all_watchpoints(std::vector<WatchpointData>& outWatchpoints)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        outWatchpoints.clear();
        outWatchpoints.reserve(manager.watchpoints.size());

        for (const auto& wp : manager.watchpoints | std::views::values)
        {
            outWatchpoints.push_back(wp);
        }

        return STATUS_OK;
    }

    StatusCode reset_watchpoint_hit_count(const std::uint32_t watchpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        it->second.hitCount = 0;
        return STATUS_OK;
    }
}
