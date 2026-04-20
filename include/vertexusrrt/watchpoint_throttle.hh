//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <cstdint>

namespace debugger
{
    inline constexpr std::uint32_t DEFAULT_WATCHPOINT_THROTTLE_MS = 250;
    inline constexpr std::uint32_t MAX_WATCHPOINT_THROTTLE_MS = 60000;

    [[nodiscard]] std::uint32_t get_watchpoint_throttle_ms() noexcept;
    void set_watchpoint_throttle_ms(std::uint32_t throttleMs) noexcept;

    [[nodiscard]] bool should_throttle_watchpoint(std::uint32_t watchpointId) noexcept;
    void clear_watchpoint_throttle_entry(std::uint32_t watchpointId) noexcept;
    void clear_watchpoint_throttle_state() noexcept;

    void register_watchpoint_throttle_ui();
}
