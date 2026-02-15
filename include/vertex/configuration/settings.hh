//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/configuration/isettings.hh>
#include <vertex/log/ilog.hh>
#include <optional>

namespace Vertex::Configuration
{
    class Settings final : public ISettings
    {
    public:
        explicit Settings(Log::ILog& log);

        StatusCode load_from_file(const std::filesystem::path& path) override;
        StatusCode save_to_file(const std::filesystem::path& path) override;
        void reset_to_defaults() override;
        [[nodiscard]] bool validate() const override;
        void set_default_language();

        [[nodiscard]] nlohmann::json get_settings() const override
        {
            return m_settings;
        }

        void update_settings(const nlohmann::json& settings) override
        {
            m_settings = settings;
        }

        [[nodiscard]] bool get_bool(const std::string& key, bool defaultValue = false) const override;
        [[nodiscard]] int get_int(const std::string& key, int defaultValue = 0) const override;
        [[nodiscard]] std::string get_string(const std::string& key, const std::string& defaultValue = "") const override;
        [[nodiscard]] std::filesystem::path get_path(const std::string& key, const std::filesystem::path& defaultValue = {}) const override;

        [[nodiscard]] nlohmann::json get_value(const std::string& key) const override;
        void set_value(const std::string& key, const nlohmann::json& value) override;

    private:
        nlohmann::json m_settings{};
        Log::ILog& m_log;

        void set_default_values();
        [[nodiscard]] std::optional<nlohmann::json> navigate_to_key(const std::string& key) const;
        void set_nested_value(const std::string& key, const nlohmann::json& value);
        [[nodiscard]] std::vector<std::string> split_key(const std::string& key) const;

        void log_error(std::string_view message) const;
    };
}
