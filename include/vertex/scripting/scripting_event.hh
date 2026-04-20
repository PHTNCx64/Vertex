//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>

#include <cstdint>
#include <filesystem>
#include <string>
#include <type_traits>
#include <variant>

namespace Vertex::Scripting
{
    enum class ScriptingEventKind : std::uint32_t
    {
        None           = 0,
        ScriptOutput   = 1u << 0,
        ScriptError    = 1u << 1,
        ScriptComplete = 1u << 2,
        ModuleLoaded   = 1u << 3,
        ModuleUnloaded = 1u << 4,
    };

    using ScriptingEventKindMask = std::underlying_type_t<ScriptingEventKind>;

    [[nodiscard]] inline constexpr ScriptingEventKindMask operator|(ScriptingEventKind a, ScriptingEventKind b) noexcept
    {
        return static_cast<ScriptingEventKindMask>(a) | static_cast<ScriptingEventKindMask>(b);
    }

    [[nodiscard]] inline constexpr ScriptingEventKindMask operator|(ScriptingEventKindMask a, ScriptingEventKind b) noexcept
    {
        return a | static_cast<ScriptingEventKindMask>(b);
    }

    [[nodiscard]] inline constexpr ScriptingEventKindMask operator&(ScriptingEventKindMask a, ScriptingEventKind b) noexcept
    {
        return a & static_cast<ScriptingEventKindMask>(b);
    }

    struct ScriptOutputInfo final
    {
        std::string moduleName{};
        std::string text{};
    };

    struct ScriptErrorInfo final
    {
        std::string moduleName{};
        StatusCode code{};
        std::string message{};
    };

    struct ScriptCompleteInfo final
    {
        std::string moduleName{};
        StatusCode code{};
    };

    struct ModuleInfo final
    {
        std::string moduleName{};
        std::filesystem::path path{};
    };

    struct ScriptingEvent final
    {
        ScriptingEventKind kind{ScriptingEventKind::None};
        std::variant<std::monostate,
                     ScriptOutputInfo,
                     ScriptErrorInfo,
                     ScriptCompleteInfo,
                     ModuleInfo>
            detail{};
    };
}
