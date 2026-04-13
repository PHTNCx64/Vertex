//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <sdk/statuscode.h>

#include <chrono>
#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace Vertex::Scripting
{
    using ContextId = uint32_t;

    enum class ScriptState : int32_t
    {
        Ready,
        Running,
        Executing,
        Suspended,
        Sleeping,
        Finished,
        Error
    };

    struct PinnedPlugin final
    {
        std::filesystem::path pluginPath;
    };

    struct UseActive final {};

    using BindingPolicy = std::variant<UseActive, PinnedPlugin>;

    struct ContextInfo final
    {
        ContextId id{};
        std::string name{};
        ScriptState state{ScriptState::Ready};
    };

    struct ContextVariable final
    {
        std::string name{};
        std::string type{};
        std::string value{};
    };

    class IAngelScript
    {
    public:
        virtual ~IAngelScript() = default;

        [[nodiscard]] virtual StatusCode start() = 0;
        [[nodiscard]] virtual StatusCode stop() = 0;
        [[nodiscard]] virtual bool is_running() const noexcept = 0;

        [[nodiscard]] virtual std::expected<ContextId, StatusCode> create_context(
            std::string_view moduleName,
            const std::filesystem::path& scriptPath,
            BindingPolicy policy) = 0;

        [[nodiscard]] virtual StatusCode remove_context(ContextId id) = 0;
        [[nodiscard]] virtual StatusCode suspend_context(ContextId id) = 0;
        [[nodiscard]] virtual StatusCode resume_context(ContextId id) = 0;
        [[nodiscard]] virtual StatusCode set_breakpoint(ContextId id, int line) = 0;
        [[nodiscard]] virtual StatusCode remove_breakpoint(ContextId id, int line) = 0;

        [[nodiscard]] virtual std::expected<ScriptState, StatusCode> get_context_state(ContextId id) const = 0;
        [[nodiscard]] virtual std::vector<ContextInfo> get_context_list() const = 0;
        [[nodiscard]] virtual std::expected<std::vector<ContextVariable>, StatusCode> get_context_variables(ContextId id) const = 0;
    };
}
