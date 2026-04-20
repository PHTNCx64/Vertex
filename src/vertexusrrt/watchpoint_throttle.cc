//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/watchpoint_throttle.hh>
#include <sdk/api.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <mutex>
#include <string_view>
#include <unordered_map>

extern Runtime* g_pluginRuntime;

namespace
{
    using SteadyClock = std::chrono::steady_clock;

    std::atomic<std::uint32_t> g_throttleMs{debugger::DEFAULT_WATCHPOINT_THROTTLE_MS};

    std::mutex g_lastHitMutex{};
    std::unordered_map<std::uint32_t, SteadyClock::time_point> g_lastHitByWatchpoint{};

    constexpr auto WATCHPOINT_THROTTLE_PANEL_ID = "watchpoint_throttle";
    constexpr auto THROTTLE_MS_FIELD_ID = "throttle_ms";

    template <std::size_t N>
    void fill_c_string(char (&dest)[N], std::string_view src) noexcept
    {
        const auto count = std::min(src.size(), N - 1);
        std::copy_n(src.data(), count, dest);
        dest[count] = '\0';
    }

    void VERTEX_API on_throttle_apply(const char* fieldId, const UIValue* value, [[maybe_unused]] void* userData)
    {
        if (fieldId == nullptr || value == nullptr)
        {
            return;
        }

        const std::string_view field{fieldId};
        if (field != THROTTLE_MS_FIELD_ID)
        {
            return;
        }

        const auto clamped = std::clamp<std::int64_t>(value->intValue, 0,
            static_cast<std::int64_t>(debugger::MAX_WATCHPOINT_THROTTLE_MS));

        debugger::set_watchpoint_throttle_ms(static_cast<std::uint32_t>(clamped));

        if (g_pluginRuntime != nullptr)
        {
            g_pluginRuntime->vertex_log_info("Debugger: watchpoint throttle set to %u ms",
                static_cast<unsigned>(clamped));
        }
    }

    void VERTEX_API on_throttle_reset([[maybe_unused]] void* userData)
    {
        debugger::set_watchpoint_throttle_ms(debugger::DEFAULT_WATCHPOINT_THROTTLE_MS);
    }
}

namespace debugger
{
    std::uint32_t get_watchpoint_throttle_ms() noexcept
    {
        return g_throttleMs.load(std::memory_order_relaxed);
    }

    void set_watchpoint_throttle_ms(const std::uint32_t throttleMs) noexcept
    {
        const auto clamped = std::min(throttleMs, MAX_WATCHPOINT_THROTTLE_MS);
        g_throttleMs.store(clamped, std::memory_order_relaxed);
    }

    bool should_throttle_watchpoint(const std::uint32_t watchpointId) noexcept
    {
        const auto throttleMs = g_throttleMs.load(std::memory_order_relaxed);
        if (throttleMs == 0)
        {
            return false;
        }

        const auto now = SteadyClock::now();
        const auto window = std::chrono::milliseconds{throttleMs};

        std::scoped_lock lock{g_lastHitMutex};
        const auto it = g_lastHitByWatchpoint.find(watchpointId);
        if (it != g_lastHitByWatchpoint.end() && (now - it->second) < window)
        {
            return true;
        }

        g_lastHitByWatchpoint.insert_or_assign(watchpointId, now);
        return false;
    }

    void clear_watchpoint_throttle_entry(const std::uint32_t watchpointId) noexcept
    {
        std::scoped_lock lock{g_lastHitMutex};
        g_lastHitByWatchpoint.erase(watchpointId);
    }

    void clear_watchpoint_throttle_state() noexcept
    {
        std::scoped_lock lock{g_lastHitMutex};
        g_lastHitByWatchpoint.clear();
    }

    void register_watchpoint_throttle_ui()
    {
        if (g_pluginRuntime == nullptr || g_pluginRuntime->vertex_register_ui_panel == nullptr)
        {
            return;
        }

        static std::array<UIField, 1> fields{};

        auto& throttleField = fields[0];
        fill_c_string(throttleField.fieldId, THROTTLE_MS_FIELD_ID);
        fill_c_string(throttleField.label, "Watchpoint Throttle (ms)");
        fill_c_string(throttleField.tooltip,
            "Minimum time between successive watchpoint hits dispatched to the debugger. "
            "Prevents overloading the target when a watchpoint fires very frequently. "
            "Set to 0 to disable throttling. Default: 250 ms.");
        throttleField.type = VERTEX_UI_FIELD_NUMBER_INT;
        throttleField.defaultValue.intValue = static_cast<std::int64_t>(DEFAULT_WATCHPOINT_THROTTLE_MS);
        throttleField.minValue.intValue = 0;
        throttleField.maxValue.intValue = static_cast<std::int64_t>(MAX_WATCHPOINT_THROTTLE_MS);
        throttleField.layoutOrientation = VERTEX_UI_LAYOUT_HORIZONTAL;
        throttleField.layoutBorder = 5;
        throttleField.layoutProportion = 0;

        static UISection section{};
        fill_c_string(section.title, "Watchpoint Throttling");
        section.fields = fields.data();
        section.fieldCount = fields.size();

        static UIPanel panel{};
        fill_c_string(panel.panelId, WATCHPOINT_THROTTLE_PANEL_ID);
        fill_c_string(panel.title, "Debugger");
        panel.sections = &section;
        panel.sectionCount = 1;
        panel.onApply = on_throttle_apply;
        panel.onReset = on_throttle_reset;
        panel.userData = nullptr;

        std::ignore = g_pluginRuntime->vertex_register_ui_panel(&panel);
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_set_watchpoint_throttle_ms(const std::uint32_t throttleMs)
    {
        if (throttleMs > debugger::MAX_WATCHPOINT_THROTTLE_MS)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        debugger::set_watchpoint_throttle_ms(throttleMs);
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_watchpoint_throttle_ms(std::uint32_t* throttleMs)
    {
        if (throttleMs == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        *throttleMs = debugger::get_watchpoint_throttle_ms();
        return StatusCode::STATUS_OK;
    }
}
