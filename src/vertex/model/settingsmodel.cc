//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/utility.hh>
#include <vertex/configuration/filesystem.hh>
#include <vertex/model/settingsmodel.hh>
#include <ranges>
#include <algorithm>

namespace Vertex::Model
{
    StatusCode SettingsModel::set_logging_status(const bool status) const
    {
        m_settingsService.set_value("general.enableLogging", status);
        m_hasUnsavedChanges = true;
        return m_loggerService.set_logging_status(status);
    }

    StatusCode SettingsModel::set_logging_interval(const int minutes) const
    {
        return m_loggerService.set_logging_interval(minutes);
    }

    StatusCode SettingsModel::set_save_interval(const int minutes) const
    {
        m_settingsService.set_value("general.autoSaveInterval", minutes);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::save_settings() const
    {
        const auto result = m_settingsService.save_to_file(Configuration::Filesystem::get_configuration_path() / "Settings.json");
        m_hasUnsavedChanges = false;
        return result;
    }

    bool SettingsModel::has_pending_changes() const
    {
        return m_hasUnsavedChanges;
    }

    StatusCode SettingsModel::set_theme(const int theme) const
    {
        m_settingsService.set_value("general.theme", theme);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::set_gui_saving_enabled(const bool status) const
    {
        m_settingsService.set_value("general.guiSavingEnabled", status);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::set_remember_window_position(const bool status) const
    {
        m_settingsService.set_value("general.rememberWindowPos", status);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::set_active_language(const std::string_view choice) const
    {
        const std::string language = fmt::format("{}{}", choice, FileTypes::CONFIGURATION_EXTENSION);
        m_settingsService.set_value("language.activeLanguage", language);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_plugin_loaded(const int selectedPluginIndex) const
    {
        return m_loaderService.get_plugins()[selectedPluginIndex].is_loaded() ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
    }

    StatusCode SettingsModel::get_plugin_is_active(const int selectedPluginIndex) const
    {
        const auto activePlugin = m_loaderService.get_active_plugin();
        if (!activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& selectedPlugin = m_loaderService.get_plugins()[selectedPluginIndex];
        return &activePlugin->get() == &selectedPlugin ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
    }

    StatusCode SettingsModel::get_theme(int& theme) const
    {
        theme = m_settingsService.get_int("general.theme");
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_gui_saving_enabled(bool& status) const
    {
        status = m_settingsService.get_bool("general.guiSavingEnabled");
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_remember_window_position(bool& status) const
    {
        status = m_settingsService.get_bool("general.rememberWindowPos");
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_save_interval(int& minutes) const
    {
        minutes = m_settingsService.get_int("general.autoSaveInterval");
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_logging_status(bool& status) const
    {
        status = m_settingsService.get_bool("general.enableLogging");
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_is_active_language(const std::string_view languageKey) const
    {
        const auto& languages = m_languageService.fetch_all_languages();
        const std::string keyStr{languageKey};
        const auto& it = languages.find(keyStr);
        return m_languageService.is_active_language(it->second);
    }

    const std::vector<Runtime::Plugin>& SettingsModel::get_plugins() const
    {
        return m_loaderService.get_plugins();
    }

    StatusCode SettingsModel::get_reader_threads(int& count) const
    {
        count = m_settingsService.get_int("memoryScan.readerThreads", 1);
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::set_reader_threads(const int count) const
    {
        m_settingsService.set_value("memoryScan.readerThreads", count);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::get_thread_buffer_size(int& sizeMB) const
    {
        sizeMB = m_settingsService.get_int("memoryScan.threadBufferSizeMB", 32);
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::set_thread_buffer_size(const int sizeMB) const
    {
        m_settingsService.set_value("memoryScan.threadBufferSizeMB", sizeMB);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    std::vector<std::filesystem::path> SettingsModel::get_plugin_paths() const
    {
        const auto pluginPathsJson = m_settingsService.get_value("plugins.pluginPaths");

        if (!pluginPathsJson.is_array())
        {
            return {};
        }

        return pluginPathsJson
            | std::views::filter([](const auto& item) { return item.is_string(); })
            | std::views::transform([](const auto& item) { return std::filesystem::path{item.template get<std::string>()}; })
            | std::ranges::to<std::vector>();
    }

    StatusCode SettingsModel::add_plugin_path(const std::filesystem::path& path) const
    {
        auto pluginPathsJson = m_settingsService.get_value("plugins.pluginPaths");

        if (!pluginPathsJson.is_array())
        {
            pluginPathsJson = nlohmann::json::array();
        }

        const std::string pathStr = path.string();
        const auto exists = std::ranges::any_of(pluginPathsJson, [&pathStr](const auto& item) {
            return item.is_string() && item.template get<std::string>() == pathStr;
        });

        if (exists)
        {
            return StatusCode::STATUS_ERROR_GENERAL_ALREADY_EXISTS;
        }

        pluginPathsJson.push_back(pathStr);
        m_settingsService.set_value("plugins.pluginPaths", pluginPathsJson);
        m_hasUnsavedChanges = true;

        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::remove_plugin_path(const std::filesystem::path& path) const
    {
        auto pluginPathsJson = m_settingsService.get_value("plugins.pluginPaths");

        if (!pluginPathsJson.is_array())
        {
            return StatusCode::STATUS_ERROR_FS_JSON_TYPE_MISMATCH;
        }

        const std::string pathStr = path.string();

        const auto found = std::ranges::any_of(pluginPathsJson, [&pathStr](const auto& item) {
            return item.is_string() && item.template get<std::string>() == pathStr;
        });

        if (!found)
        {
            return StatusCode::STATUS_ERROR_FS_JSON_KEY_NOT_FOUND;
        }

        nlohmann::json newPaths = nlohmann::json::array();
        std::ranges::for_each(
            pluginPathsJson | std::views::filter([&pathStr](const auto& item) {
                return item.is_string() && item.template get<std::string>() != pathStr;
            }),
            [&newPaths](const auto& item) { newPaths.push_back(item); }
        );

        m_settingsService.set_value("plugins.pluginPaths", newPaths);
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    std::unordered_map<std::string, std::filesystem::path> SettingsModel::get_available_languages() const
    {
        return m_languageService.fetch_all_languages();
    }

    std::vector<std::filesystem::path> SettingsModel::get_language_paths() const
    {
        std::vector<std::filesystem::path> paths{};
        const auto languagePathJson = m_settingsService.get_value("language.languagePath");

        if (languagePathJson.is_string())
        {
            paths.emplace_back(languagePathJson.get<std::string>());
        }

        return paths;
    }

    StatusCode SettingsModel::add_language_path(const std::filesystem::path& path) const
    {
        m_settingsService.set_value("language.languagePath", path.string());
        m_hasUnsavedChanges = true;
        return StatusCode::STATUS_OK;
    }

    StatusCode SettingsModel::remove_language_path(const std::filesystem::path& path) const
    {
        const auto currentPath = m_settingsService.get_string("language.languagePath");

        if (currentPath == path.string())
        {
            m_settingsService.set_value("language.languagePath", EMPTY_STRING);
            m_hasUnsavedChanges = true;
            return StatusCode::STATUS_OK;
        }

        return StatusCode::STATUS_ERROR_FS_JSON_KEY_NOT_FOUND;
    }

    StatusCode SettingsModel::load_plugin(const std::size_t index) const
    {
        auto& plugins = m_loaderService.get_plugins();
        if (index >= plugins.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }
        if (plugins[index].is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_ALREADY_LOADED;
        }
        return m_loaderService.load_plugin(plugins[index].get_path());
    }

    StatusCode SettingsModel::unload_plugin(const std::size_t index) const
    {
        auto& plugins = m_loaderService.get_plugins();
        if (index >= plugins.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!plugins[index].is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto activePlugin = m_loaderService.get_active_plugin();
        if (activePlugin.has_value() && &activePlugin->get() == &plugins[index])
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return m_loaderService.unload_plugin(index);
    }

    StatusCode SettingsModel::set_active_plugin(const std::size_t index) const
    {
        auto& plugins = m_loaderService.get_plugins();
        if (index >= plugins.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!plugins[index].is_loaded())
        {
            const auto loadResult = m_loaderService.load_plugin(plugins[index].get_path());
            if (loadResult != StatusCode::STATUS_OK)
            {
                return loadResult;
            }
        }

        const auto result = m_loaderService.set_active_plugin(index);
        if (result == StatusCode::STATUS_OK)
        {
            m_settingsService.set_value("plugins.activePluginPath", plugins[index].get_path());
            m_hasUnsavedChanges = true;
        }
        return result;
    }

    int SettingsModel::get_ui_state_int(const std::string_view key, const int defaultValue) const
    {
        bool guiSavingEnabled{};
        std::ignore = get_gui_saving_enabled(guiSavingEnabled);
        if (!guiSavingEnabled)
        {
            return defaultValue;
        }
        const std::string keyStr{key};
        return m_settingsService.get_int(keyStr, defaultValue);
    }

    void SettingsModel::set_ui_state_int(const std::string_view key, const int value) const
    {
        bool guiSavingEnabled{};
        std::ignore = get_gui_saving_enabled(guiSavingEnabled);
        if (guiSavingEnabled)
        {
            const std::string keyStr{key};
            m_settingsService.set_value(keyStr, value);
        }
    }
}
