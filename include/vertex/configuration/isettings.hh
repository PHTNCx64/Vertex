//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <filesystem>
#include <nlohmann/json.hpp>
#include <sdk/statuscode.h>

namespace Vertex::Configuration
{
    class ISettings
    {
    public:
        virtual ~ISettings() = default;
        virtual StatusCode load_from_file(const std::filesystem::path& path) = 0;
        virtual StatusCode save_to_file(const std::filesystem::path& path) = 0;
        virtual void reset_to_defaults() = 0;
        [[nodiscard]] virtual bool validate() const = 0;

        [[nodiscard]] virtual nlohmann::json get_settings() const = 0;
        virtual void update_settings(const nlohmann::json& settings) = 0;

        [[nodiscard]] virtual bool get_bool(const std::string& key, bool defaultValue = false) const = 0;
        [[nodiscard]] virtual int get_int(const std::string& key, int defaultValue = 0) const = 0;
        [[nodiscard]] virtual std::string get_string(const std::string& key, const std::string& defaultValue = "") const = 0;
        [[nodiscard]] virtual std::filesystem::path get_path(const std::string& key, const std::filesystem::path& defaultValue = {}) const = 0;

        [[nodiscard]] virtual nlohmann::json get_value(const std::string& key) const = 0;
        virtual void set_value(const std::string& key, const nlohmann::json& value) = 0;
    };
}
