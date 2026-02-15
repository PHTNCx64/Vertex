//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <filesystem>
#include <string>
#include <vector>
#include <sdk/statuscode.h>

namespace Vertex::Configuration
{
    class IPluginConfig
    {
    public:
        virtual ~IPluginConfig() = default;

        virtual StatusCode load_config(const std::string& pluginFilename) = 0;

        virtual StatusCode save_config() = 0;

        [[nodiscard]] virtual std::vector<std::string> get_enabled_memory_attributes(const std::string& attributeType) const = 0;
        [[nodiscard]] virtual std::vector<std::string> get_enabled_memory_attributes(const std::string& section, const std::string& attributeType) const = 0;

        virtual void set_enabled_memory_attributes(const std::string& attributeType, const std::vector<std::string>& attributes) = 0;
        virtual void set_enabled_memory_attributes(const std::string& section, const std::string& attributeType, const std::vector<std::string>& attributes) = 0;

        [[nodiscard]] virtual bool is_memory_attribute_enabled(const std::string& attributeType, const std::string& attributeName) const = 0;
        [[nodiscard]] virtual bool is_memory_attribute_enabled(const std::string& section, const std::string& attributeType, const std::string& attributeName) const = 0;

        [[nodiscard]] virtual std::vector<std::string> get_excluded_modules() const = 0;
        virtual void set_excluded_modules(const std::vector<std::string>& modules) = 0;

        [[nodiscard]] virtual std::string get_current_plugin() const = 0;

        [[nodiscard]] virtual bool is_modified() const = 0;
    };
}
