//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>

#include <vertex/event/eventbus.hh>
#include <vertex/model/settingsmodel.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

namespace Vertex::ViewModel
{
    class SettingsViewModel final
    {
    public:
        SettingsViewModel(
            std::unique_ptr<Model::ISettingsModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService,
            std::string name = ViewModelName::SETTINGS
        );

        ~SettingsViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback);

        void save_settings() const;
        void reset_to_defaults();
        void apply_settings() const;
        [[nodiscard]] bool has_pending_changes() const;

        [[nodiscard]] bool get_logging_status() const;
        [[nodiscard]] int get_save_interval() const;
        [[nodiscard]] int get_theme() const;
        [[nodiscard]] bool get_gui_saving_enabled() const;
        [[nodiscard]] bool get_remember_window_position() const;
        [[nodiscard]] const std::vector<Runtime::Plugin>& get_plugins() const;
        [[nodiscard]] bool is_plugin_loaded(int selectedPluginIndex) const;
        [[nodiscard]] bool is_plugin_active(int selectedPluginIndex) const;
        [[nodiscard]] bool is_active_language(std::string_view languageKey) const;
        [[nodiscard]] int get_reader_threads() const;
        [[nodiscard]] int get_thread_buffer_size() const;

        void set_logging_status(bool status) const;
        void set_logging_interval(int minutes) const;
        void set_save_interval(int minutes) const;
        void set_theme(int theme) const;
        void set_gui_saving_enabled(bool status) const;
        void set_remember_window_position(bool status) const;
        void set_active_language(std::string_view choice) const;
        void set_reader_threads(int count) const;
        void set_thread_buffer_size(int sizeMB) const;

        void load_plugin(std::size_t index) const;
        void unload_plugin(std::size_t index) const;
        void set_active_plugin(std::size_t index) const;

        [[nodiscard]] std::vector<std::filesystem::path> get_plugin_paths() const;
        [[nodiscard]] bool add_plugin_path(const std::filesystem::path& path) const;
        [[nodiscard]] bool remove_plugin_path(const std::filesystem::path& path) const;

        [[nodiscard]] std::unordered_map<std::string, std::filesystem::path> get_available_languages() const;
        [[nodiscard]] std::vector<std::filesystem::path> get_language_paths() const;
        [[nodiscard]] bool add_language_path(const std::filesystem::path& path) const;
        [[nodiscard]] bool remove_language_path(const std::filesystem::path& path) const;

        [[nodiscard]] int get_last_tab_index() const;
        void set_last_tab_index(int index) const;

    private:
        void subscribe_to_events() const;
        void unsubscribe_from_events() const;

        std::string m_viewModelName {};
        std::unique_ptr<Model::ISettingsModel> m_model {};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback {};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;
    };
}
