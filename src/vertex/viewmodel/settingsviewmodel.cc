//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/settingsviewmodel.hh>

#include <utility>

#include <fmt/format.h>

#include <vertex/event/types/viewevent.hh>

namespace Vertex::ViewModel
{
    SettingsViewModel::SettingsViewModel(std::unique_ptr<Model::ISettingsModel> model, Event::EventBus& eventBus, Log::ILog& logService, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_logService{logService}
    {
        subscribe_to_events();
    }

    SettingsViewModel::~SettingsViewModel()
    {
        unsubscribe_from_events();
    }

    void SettingsViewModel::subscribe_to_events() const
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   if (m_eventCallback)
                                                   {
                                                       m_eventCallback(Event::VIEW_EVENT, event);
                                                   }
                                               });
    }

    void SettingsViewModel::unsubscribe_from_events() const
    {
        m_eventBus.unsubscribe(m_viewModelName, Event::VIEW_EVENT);
    }

    void SettingsViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> eventCallback)
    {
        m_eventCallback = std::move(eventCallback);
    }

    void SettingsViewModel::save_settings() const
    {
        if (const auto status = m_model->save_settings(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to save settings (status={})", static_cast<int>(status)));
        }
    }

    void SettingsViewModel::reset_to_defaults()
    {
    }

    void SettingsViewModel::apply_settings() const
    {
        if (const auto status = m_model->save_settings(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to apply settings (status={})", static_cast<int>(status)));
        }
    }

    bool SettingsViewModel::has_pending_changes() const
    {
        return m_model->has_pending_changes();
    }

    bool SettingsViewModel::get_logging_status() const
    {
        bool status{};
        if (const auto result = m_model->get_logging_status(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get logging status (status={})", static_cast<int>(result)));
        }
        return status;
    }

    int SettingsViewModel::get_save_interval() const
    {
        int minutes{};
        if (const auto status = m_model->get_save_interval(minutes); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get save interval (status={})", static_cast<int>(status)));
        }
        return minutes;
    }

    int SettingsViewModel::get_theme() const
    {
        int theme{};
        if (const auto status = m_model->get_theme(theme); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get theme (status={})", static_cast<int>(status)));
        }
        return theme;
    }

    bool SettingsViewModel::get_gui_saving_enabled() const
    {
        bool status{};
        if (const auto result = m_model->get_gui_saving_enabled(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get GUI saving status (status={})", static_cast<int>(result)));
        }
        return status;
    }

    bool SettingsViewModel::get_remember_window_position() const
    {
        bool status{};
        if (const auto result = m_model->get_remember_window_position(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get remember window position (status={})", static_cast<int>(result)));
        }
        return status;
    }

    const std::vector<Runtime::Plugin>& SettingsViewModel::get_plugins() const
    {
        return m_model->get_plugins();
    }

    bool SettingsViewModel::is_plugin_loaded(const int selectedPluginIndex) const
    {
        return m_model->get_plugin_loaded(selectedPluginIndex) == StatusCode::STATUS_OK;
    }

    bool SettingsViewModel::is_plugin_active(const int selectedPluginIndex) const
    {
        return m_model->get_plugin_is_active(selectedPluginIndex) == StatusCode::STATUS_OK;
    }

    bool SettingsViewModel::is_active_language(const std::string_view languageKey) const
    {
        return m_model->get_is_active_language(languageKey) == StatusCode::STATUS_OK;
    }

    void SettingsViewModel::set_logging_status(const bool status) const
    {
        if (const auto result = m_model->set_logging_status(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set logging status (status={})", static_cast<int>(result)));
        }
    }

    void SettingsViewModel::set_logging_interval(const int minutes) const
    {
        if (const auto status = m_model->set_logging_interval(minutes); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set logging interval (status={})", static_cast<int>(status)));
        }
    }

    void SettingsViewModel::set_save_interval(const int minutes) const
    {
        if (const auto status = m_model->set_save_interval(minutes); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set save interval (status={})", static_cast<int>(status)));
        }
    }

    void SettingsViewModel::set_theme(const int theme) const
    {
        if (const auto status = m_model->set_theme(theme); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set theme (status={})", static_cast<int>(status)));
        }
    }

    void SettingsViewModel::set_gui_saving_enabled(const bool status) const
    {
        if (const auto result = m_model->set_gui_saving_enabled(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set GUI saving enabled (status={})", static_cast<int>(result)));
        }
    }

    void SettingsViewModel::set_remember_window_position(const bool status) const
    {
        if (const auto result = m_model->set_remember_window_position(status); result != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set remember window position (status={})", static_cast<int>(result)));
        }
    }

    void SettingsViewModel::set_active_language(const std::string_view choice) const
    {
        if (const auto status = m_model->set_active_language(choice); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set active language (status={})", static_cast<int>(status)));
        }
    }

    void SettingsViewModel::load_plugin(const std::size_t index) const
    {
        if (const auto status = m_model->load_plugin(index); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to load plugin at index {} (status={})", index, static_cast<int>(status)));
        }
    }

    void SettingsViewModel::unload_plugin(const std::size_t index) const
    {
        if (const auto status = m_model->unload_plugin(index); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to unload plugin at index {} (status={})", index, static_cast<int>(status)));
        }
    }

    void SettingsViewModel::set_active_plugin(const std::size_t index) const
    {
        if (const auto status = m_model->set_active_plugin(index); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set active plugin at index {} (status={})", index, static_cast<int>(status)));
        }
    }

    int SettingsViewModel::get_reader_threads() const
    {
        int count{};
        if (const auto status = m_model->get_reader_threads(count); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get reader threads (status={})", static_cast<int>(status)));
        }
        return count;
    }

    void SettingsViewModel::set_reader_threads(const int count) const
    {
        if (const auto status = m_model->set_reader_threads(count); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set reader threads (status={})", static_cast<int>(status)));
        }
    }

    int SettingsViewModel::get_thread_buffer_size() const
    {
        int sizeMB{};
        if (const auto status = m_model->get_thread_buffer_size(sizeMB); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to get thread buffer size (status={})", static_cast<int>(status)));
        }
        return sizeMB;
    }

    void SettingsViewModel::set_thread_buffer_size(const int sizeMB) const
    {
        if (const auto status = m_model->set_thread_buffer_size(sizeMB); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("SettingsViewModel: failed to set thread buffer size (status={})", static_cast<int>(status)));
        }
    }

    std::vector<std::filesystem::path> SettingsViewModel::get_plugin_paths() const
    {
        return m_model->get_plugin_paths();
    }

    bool SettingsViewModel::add_plugin_path(const std::filesystem::path& path) const
    {
        return m_model->add_plugin_path(path) == StatusCode::STATUS_OK;
    }

    bool SettingsViewModel::remove_plugin_path(const std::filesystem::path& path) const
    {
        return m_model->remove_plugin_path(path) == StatusCode::STATUS_OK;
    }

    std::unordered_map<std::string, std::filesystem::path> SettingsViewModel::get_available_languages() const
    {
        return m_model->get_available_languages();
    }

    std::vector<std::filesystem::path> SettingsViewModel::get_language_paths() const
    {
        return m_model->get_language_paths();
    }

    bool SettingsViewModel::add_language_path(const std::filesystem::path& path) const
    {
        return m_model->add_language_path(path) == StatusCode::STATUS_OK;
    }

    bool SettingsViewModel::remove_language_path(const std::filesystem::path& path) const
    {
        return m_model->remove_language_path(path) == StatusCode::STATUS_OK;
    }

    int SettingsViewModel::get_last_tab_index() const
    {
        return m_model->get_ui_state_int("uiState.settingsView.lastTabIndex", 0);
    }

    void SettingsViewModel::set_last_tab_index(const int index) const
    {
        m_model->set_ui_state_int("uiState.settingsView.lastTabIndex", index);
    }
}
