//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/log/log.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/runtime/registry.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/configuration/isettings.hh>

namespace Vertex::Runtime
{
    class Loader final : public ILoader
    {
    public:
        Loader(Configuration::ISettings& settingsService, Log::ILog& loggerService); // NOLINT
        ~Loader() override;

        StatusCode load_plugins(std::filesystem::path& path) override;
        StatusCode load_plugin(std::filesystem::path path) override;
        StatusCode unload_plugin(std::size_t pluginIndex) override;
        StatusCode resolve_functions(Plugin& plugin) override;
        StatusCode set_active_plugin(Plugin& plugin) override;
        StatusCode set_active_plugin(std::size_t index) override;
        StatusCode set_active_plugin(const std::filesystem::path& path) override;
        [[nodiscard]] StatusCode has_plugin_loaded() override;
        StatusCode get_plugins_from_fs(const std::vector<std::filesystem::path>& paths, std::vector<Plugin>& pluginStates) override;
        [[nodiscard]] std::optional<std::reference_wrapper<Plugin>> get_active_plugin() override;
        [[nodiscard]] const std::vector<Plugin>& get_plugins() noexcept override;

        [[nodiscard]] IRegistry& get_registry() override;
        [[nodiscard]] const IRegistry& get_registry() const override;

        StatusCode dispatch_event(VertexEvent event, const void* data = nullptr) override;

    private:
        [[nodiscard]] std::string status_code_to_string(StatusCode code) const;

        std::vector<Plugin> m_plugins{};
        std::optional<std::reference_wrapper<Plugin>> m_activePlugin;
        Configuration::ISettings& m_settingsService;
        Log::ILog& m_loggerService;
        Registry m_registry;
    };
}
