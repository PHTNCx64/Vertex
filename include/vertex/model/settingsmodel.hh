//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/loader.hh>
#include <vertex/log/log.hh>
#include <vertex/language/language.hh>

#include <string>
#include <string_view>

namespace Vertex::Model
{
    class SettingsModel final
    {
    public:
        explicit SettingsModel(Runtime::ILoader& loaderService,
                               Log::ILog& loggerService,
                               Language::ILanguage& languageService,
                               Configuration::ISettings& settingsService)
            : m_loaderService(loaderService),
              m_loggerService(loggerService),
              m_settingsService(settingsService),
              m_languageService(languageService)
        {
        }

        [[nodiscard]] StatusCode save_settings() const;
        [[nodiscard]] bool has_pending_changes() const;

        [[nodiscard]] StatusCode set_logging_status(bool status) const;
        [[nodiscard]] StatusCode set_logging_interval(int minutes) const;
        [[nodiscard]] StatusCode set_save_interval(int minutes) const;
        [[nodiscard]] StatusCode set_theme(int theme) const;
        [[nodiscard]] StatusCode set_gui_saving_enabled(bool status) const;
        [[nodiscard]] StatusCode set_remember_window_position(bool status) const;
        [[nodiscard]] StatusCode set_active_language(std::string_view choice) const;

        [[nodiscard]] StatusCode get_logging_status(bool& status) const;
        [[nodiscard]] StatusCode get_save_interval(int& minutes) const;
        [[nodiscard]] const std::vector<Runtime::Plugin>& get_plugins() const;
        [[nodiscard]] StatusCode get_plugin_loaded(int selectedPluginIndex) const;
        [[nodiscard]] StatusCode get_plugin_is_active(int selectedPluginIndex) const;
        [[nodiscard]] StatusCode get_is_active_language(std::string_view languageKey) const;
        [[nodiscard]] StatusCode get_theme(int& theme) const;
        [[nodiscard]] StatusCode get_gui_saving_enabled(bool& status) const;
        [[nodiscard]] StatusCode get_remember_window_position(bool& status) const;
        [[nodiscard]] StatusCode load_plugin(std::size_t index) const;
        [[nodiscard]] StatusCode set_active_plugin(std::size_t index) const;

        [[nodiscard]] StatusCode get_reader_threads(int& count) const;
        [[nodiscard]] StatusCode get_thread_buffer_size(int& sizeMB) const;
        [[nodiscard]] StatusCode set_reader_threads(int count) const;
        [[nodiscard]] StatusCode set_thread_buffer_size(int sizeMB) const;

        [[nodiscard]] std::vector<std::filesystem::path> get_plugin_paths() const;
        [[nodiscard]] StatusCode add_plugin_path(const std::filesystem::path& path) const;
        [[nodiscard]] StatusCode remove_plugin_path(const std::filesystem::path& path) const;

        [[nodiscard]] std::unordered_map<std::string, std::filesystem::path> get_available_languages() const;
        [[nodiscard]] std::vector<std::filesystem::path> get_language_paths() const;
        [[nodiscard]] StatusCode add_language_path(const std::filesystem::path& path) const;
        [[nodiscard]] StatusCode remove_language_path(const std::filesystem::path& path) const;

        [[nodiscard]] int get_ui_state_int(std::string_view key, int defaultValue) const;
        void set_ui_state_int(std::string_view key, int value) const;

    private:
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Configuration::ISettings& m_settingsService;
        Language::ILanguage& m_languageService;
        mutable bool m_hasUnsavedChanges{};
    };
}
