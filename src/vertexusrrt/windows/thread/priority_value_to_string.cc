//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <Windows.h>
#include <array>
#include <cstdint>
#include <ranges>
#include <string_view>

namespace
{
    struct PriorityEntry final
    {
        std::int32_t priority;
        std::string_view text;
        std::size_t size;
    };

    consteval PriorityEntry make_priority_entry(const int32_t priority, const std::string_view text)
    {
        return PriorityEntry{ priority, text, text.size() + 1U };
    }

    constexpr auto k_priority_entries = std::array{
        make_priority_entry(THREAD_PRIORITY_LOWEST, "Lowest"),
        make_priority_entry(THREAD_PRIORITY_BELOW_NORMAL, "Below Normal"),
        make_priority_entry(THREAD_PRIORITY_NORMAL, "Normal"),
        make_priority_entry(THREAD_PRIORITY_ABOVE_NORMAL, "Above Normal"),
        make_priority_entry(THREAD_PRIORITY_HIGHEST, "Highest"),
        make_priority_entry(THREAD_PRIORITY_TIME_CRITICAL, "Time Critical"),
        make_priority_entry(THREAD_PRIORITY_IDLE, "Idle")
    };

    consteval PriorityEntry make_special_entry(const std::string_view text)
    {
        return PriorityEntry{ 0, text, text.size() + 1U };
    }

    constexpr auto k_custom_priority = make_special_entry("Custom");
    constexpr auto k_invalid_priority = make_special_entry("Invalid Priority");
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_thread_priority_value_to_string(const std::int32_t priority, char** out, std::size_t* outSize)
    {
        if (out == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto entryIt = std::ranges::find_if(k_priority_entries, [priority](const PriorityEntry& entry)
            {
                return entry.priority == priority;
            });

        const PriorityEntry* selected = nullptr;
        if (entryIt != k_priority_entries.end())
        {
            selected = &(*entryIt);
        }
        else if (priority >= -15 && priority <= 15)
        {
            selected = &k_custom_priority;
        }
        else
        {
            selected = &k_invalid_priority;
        }

        *out = const_cast<char*>(selected->text.data());
        if (outSize != nullptr)
        {
            *outSize = selected->size;
        }

        return StatusCode::STATUS_OK;
    }
}
