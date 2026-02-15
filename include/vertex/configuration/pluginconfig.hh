//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/configuration/ipluginconfig.hh>
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>
#include <vector>

namespace Vertex::Configuration
{
    class PluginConfig final : public IPluginConfig
    {
    public:
        PluginConfig() = default;
        ~PluginConfig() override = default;

        StatusCode load_config(const std::string& pluginFilename) override;
        StatusCode save_config() override;

        [[nodiscard]] std::vector<std::string> get_enabled_memory_attributes(const std::string& attributeType) const override;
        [[nodiscard]] std::vector<std::string> get_enabled_memory_attributes(const std::string& section, const std::string& attributeType) const override;
        void set_enabled_memory_attributes(const std::string& attributeType, const std::vector<std::string>& attributes) override;
        void set_enabled_memory_attributes(const std::string& section, const std::string& attributeType, const std::vector<std::string>& attributes) override;
        [[nodiscard]] bool is_memory_attribute_enabled(const std::string& attributeType, const std::string& attributeName) const override;
        [[nodiscard]] bool is_memory_attribute_enabled(const std::string& section, const std::string& attributeType, const std::string& attributeName) const override;

        [[nodiscard]] std::vector<std::string> get_excluded_modules() const override;
        void set_excluded_modules(const std::vector<std::string>& modules) override;

        [[nodiscard]] std::string get_current_plugin() const override;
        [[nodiscard]] bool is_modified() const override;

    private:
        [[nodiscard]] static std::filesystem::path get_config_path(const std::string& pluginFilename);

        void set_default_values();

        [[nodiscard]] static StatusCode ensure_config_directory();

        nlohmann::json m_config{};
        std::string m_currentPluginFilename{};
        bool m_isModified{};
    };
}
