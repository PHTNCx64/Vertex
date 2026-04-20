//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <variant>

namespace Vertex::Runtime::PluginRuntime
{
    enum class PluginEventKind : std::uint32_t
    {
        None                = 0,
        PluginStateChanged  = 1u << 0,
        PluginLoadError     = 1u << 1,
        ActivePluginChanged = 1u << 2,
    };

    using PluginEventKindMask = std::underlying_type_t<PluginEventKind>;

    [[nodiscard]] inline constexpr PluginEventKindMask operator|(PluginEventKind a, PluginEventKind b) noexcept
    {
        return static_cast<PluginEventKindMask>(a) | static_cast<PluginEventKindMask>(b);
    }

    [[nodiscard]] inline constexpr PluginEventKindMask operator|(PluginEventKindMask a, PluginEventKind b) noexcept
    {
        return a | static_cast<PluginEventKindMask>(b);
    }

    [[nodiscard]] inline constexpr PluginEventKindMask operator&(PluginEventKindMask a, PluginEventKind b) noexcept
    {
        return a & static_cast<PluginEventKindMask>(b);
    }

    struct PluginStateInfo final
    {
        std::size_t index{};
        bool loaded{};
        std::string filename{};
    };

    struct PluginLoadErrorInfo final
    {
        std::filesystem::path path{};
        StatusCode code{};
        std::string description{};
    };

    struct ActivePluginChangedInfo final
    {
        std::size_t index{};
        std::string filename{};
    };

    struct PluginEvent final
    {
        PluginEventKind kind{PluginEventKind::None};
        std::variant<std::monostate,
                     PluginStateInfo,
                     PluginLoadErrorInfo,
                     ActivePluginChangedInfo>
            detail{};
    };
}
