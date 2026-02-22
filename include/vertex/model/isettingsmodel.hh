//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <vertex/runtime/plugin.hh>

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Vertex::Model
{
    class ISettingsModel
    {
    public:
        virtual ~ISettingsModel() = default;

        [[nodiscard]] virtual StatusCode save_settings() const = 0;
        [[nodiscard]] virtual bool has_pending_changes() const = 0;

        [[nodiscard]] virtual StatusCode set_logging_status(bool status) const = 0;
        [[nodiscard]] virtual StatusCode set_logging_interval(int minutes) const = 0;
        [[nodiscard]] virtual StatusCode set_save_interval(int minutes) const = 0;
        [[nodiscard]] virtual StatusCode set_theme(int theme) const = 0;
        [[nodiscard]] virtual StatusCode set_gui_saving_enabled(bool status) const = 0;
        [[nodiscard]] virtual StatusCode set_remember_window_position(bool status) const = 0;
        [[nodiscard]] virtual StatusCode set_active_language(std::string_view choice) const = 0;

        [[nodiscard]] virtual StatusCode get_logging_status(bool& status) const = 0;
        [[nodiscard]] virtual StatusCode get_save_interval(int& minutes) const = 0;
        [[nodiscard]] virtual const std::vector<Runtime::Plugin>& get_plugins() const = 0;
        [[nodiscard]] virtual StatusCode get_plugin_loaded(int selectedPluginIndex) const = 0;
        [[nodiscard]] virtual StatusCode get_plugin_is_active(int selectedPluginIndex) const = 0;
        [[nodiscard]] virtual StatusCode get_is_active_language(std::string_view languageKey) const = 0;
        [[nodiscard]] virtual StatusCode get_theme(int& theme) const = 0;
        [[nodiscard]] virtual StatusCode get_gui_saving_enabled(bool& status) const = 0;
        [[nodiscard]] virtual StatusCode get_remember_window_position(bool& status) const = 0;
        [[nodiscard]] virtual StatusCode load_plugin(std::size_t index) const = 0;
        [[nodiscard]] virtual StatusCode unload_plugin(std::size_t index) const = 0;
        [[nodiscard]] virtual StatusCode set_active_plugin(std::size_t index) const = 0;

        [[nodiscard]] virtual StatusCode get_reader_threads(int& count) const = 0;
        [[nodiscard]] virtual StatusCode get_thread_buffer_size(int& sizeMB) const = 0;
        [[nodiscard]] virtual StatusCode set_reader_threads(int count) const = 0;
        [[nodiscard]] virtual StatusCode set_thread_buffer_size(int sizeMB) const = 0;

        [[nodiscard]] virtual std::vector<std::filesystem::path> get_plugin_paths() const = 0;
        [[nodiscard]] virtual StatusCode add_plugin_path(const std::filesystem::path& path) const = 0;
        [[nodiscard]] virtual StatusCode remove_plugin_path(const std::filesystem::path& path) const = 0;

        [[nodiscard]] virtual std::unordered_map<std::string, std::filesystem::path> get_available_languages() const = 0;
        [[nodiscard]] virtual std::vector<std::filesystem::path> get_language_paths() const = 0;
        [[nodiscard]] virtual StatusCode add_language_path(const std::filesystem::path& path) const = 0;
        [[nodiscard]] virtual StatusCode remove_language_path(const std::filesystem::path& path) const = 0;

        [[nodiscard]] virtual int get_ui_state_int(std::string_view key, int defaultValue) const = 0;
        virtual void set_ui_state_int(std::string_view key, int value) const = 0;
    };
}
