//
// Mock for SettingsModel
//

#pragma once

#include <gmock/gmock.h>
#include <vertex/model/settingsmodel.hh>
#include <sdk/statuscode.h>

namespace Vertex::Testing::Mocks
{
    class MockSettingsModel
    {
    public:
        virtual ~MockSettingsModel() = default;

        MOCK_METHOD(StatusCode, save_settings, (), (const));

        // Setters
        MOCK_METHOD(StatusCode, set_logging_status, (bool status), (const));
        MOCK_METHOD(StatusCode, set_logging_interval, (int minutes), (const));
        MOCK_METHOD(StatusCode, set_theme, (int theme), (const));
        MOCK_METHOD(StatusCode, set_gui_saving_enabled, (bool status), (const));
        MOCK_METHOD(StatusCode, set_remember_window_position, (bool status), (const));
        MOCK_METHOD(StatusCode, set_active_language, (const std::string& choice), (const));

        // Getters
        MOCK_METHOD(StatusCode, get_logging_status, (bool& status), (const));
        MOCK_METHOD(StatusCode, get_save_interval, (int& minutes), (const));
        MOCK_METHOD(const std::vector<Runtime::Plugin>&, get_plugins, (), (const));
        MOCK_METHOD(StatusCode, get_plugin_loaded, (int selectedPluginIndex), (const));
        MOCK_METHOD(StatusCode, get_plugin_is_active, (int selectedPluginIndex), (const));
        MOCK_METHOD(StatusCode, get_is_active_language, (const std::string& languageKey), (const));
        MOCK_METHOD(StatusCode, get_theme, (int& theme), (const));
        MOCK_METHOD(StatusCode, get_gui_saving_enabled, (bool& status), (const));
        MOCK_METHOD(StatusCode, get_remember_window_position, (bool& status), (const));
        MOCK_METHOD(StatusCode, load_plugin, (std::size_t index), (const));

        // Memory scanner settings
        MOCK_METHOD(StatusCode, get_reader_threads, (int& count), (const));
        MOCK_METHOD(StatusCode, set_reader_threads, (int count), (const));

        // Plugin paths management
        MOCK_METHOD(std::vector<std::filesystem::path>, get_plugin_paths, (), (const));
        MOCK_METHOD(StatusCode, add_plugin_path, (const std::filesystem::path& path), (const));
        MOCK_METHOD(StatusCode, remove_plugin_path, (const std::filesystem::path& path), (const));

        // Language management
        MOCK_METHOD((std::unordered_map<std::string, std::filesystem::path>), get_available_languages, (), (const));
        MOCK_METHOD(std::vector<std::filesystem::path>, get_language_paths, (), (const));
        MOCK_METHOD(StatusCode, add_language_path, (const std::filesystem::path& path), (const));
        MOCK_METHOD(StatusCode, remove_language_path, (const std::filesystem::path& path), (const));
    };
} // namespace Vertex::Testing::Mocks
