//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <filesystem>

#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/iregistry.hh>
#include <vertex/runtime/iuiregistry.hh>

#include <sdk/statuscode.h>
#include <sdk/event.h>

namespace Vertex::Runtime
{
    class ILoader
    {
    public:
        virtual ~ILoader() = default;
        virtual StatusCode load_plugins(std::filesystem::path& path) = 0;
        virtual StatusCode load_plugin(std::filesystem::path path) = 0;
        virtual StatusCode unload_plugin(std::size_t pluginIndex) = 0;
        virtual StatusCode resolve_functions(Plugin& plugin) = 0;
        virtual StatusCode set_active_plugin(Plugin& plugin) = 0;
        virtual StatusCode set_active_plugin(std::size_t index) = 0;
        virtual StatusCode set_active_plugin(const std::filesystem::path& path) = 0;
        [[nodiscard]] virtual StatusCode has_plugin_loaded() = 0;
        virtual StatusCode get_plugins_from_fs(const std::vector<std::filesystem::path>& paths, std::vector<Plugin>& pluginStates) = 0;
        [[nodiscard]] virtual const std::vector<Plugin>& get_plugins() noexcept = 0;
        [[nodiscard]] virtual std::optional<std::reference_wrapper<Plugin>> get_active_plugin() = 0;

        [[nodiscard]] virtual IRegistry& get_registry() = 0;
        [[nodiscard]] virtual const IRegistry& get_registry() const = 0;

        [[nodiscard]] virtual IUIRegistry& get_ui_registry() = 0;
        [[nodiscard]] virtual const IUIRegistry& get_ui_registry() const = 0;

        virtual StatusCode dispatch_event(VertexEvent event, const void* data = nullptr) = 0;
    };
}
