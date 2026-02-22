#pragma once

#include <gmock/gmock.h>
#include <vertex/model/isettingsmodel.hh>

namespace Vertex::Testing::Mocks
{
    class MockSettingsModel : public Model::ISettingsModel
    {
    public:
        ~MockSettingsModel() override = default;

        MOCK_METHOD(StatusCode, save_settings, (), (const, override));
        MOCK_METHOD(bool, has_pending_changes, (), (const, override));

        MOCK_METHOD(StatusCode, set_logging_status, (bool status), (const, override));
        MOCK_METHOD(StatusCode, set_logging_interval, (int minutes), (const, override));
        MOCK_METHOD(StatusCode, set_save_interval, (int minutes), (const, override));
        MOCK_METHOD(StatusCode, set_theme, (int theme), (const, override));
        MOCK_METHOD(StatusCode, set_gui_saving_enabled, (bool status), (const, override));
        MOCK_METHOD(StatusCode, set_remember_window_position, (bool status), (const, override));
        MOCK_METHOD(StatusCode, set_active_language, (std::string_view choice), (const, override));

        MOCK_METHOD(StatusCode, get_logging_status, (bool& status), (const, override));
        MOCK_METHOD(StatusCode, get_save_interval, (int& minutes), (const, override));
        MOCK_METHOD(const std::vector<Runtime::Plugin>&, get_plugins, (), (const, override));
        MOCK_METHOD(StatusCode, get_plugin_loaded, (int selectedPluginIndex), (const, override));
        MOCK_METHOD(StatusCode, get_plugin_is_active, (int selectedPluginIndex), (const, override));
        MOCK_METHOD(StatusCode, get_is_active_language, (std::string_view languageKey), (const, override));
        MOCK_METHOD(StatusCode, get_theme, (int& theme), (const, override));
        MOCK_METHOD(StatusCode, get_gui_saving_enabled, (bool& status), (const, override));
        MOCK_METHOD(StatusCode, get_remember_window_position, (bool& status), (const, override));
        MOCK_METHOD(StatusCode, load_plugin, (std::size_t index), (const, override));
        MOCK_METHOD(StatusCode, unload_plugin, (std::size_t index), (const, override));
        MOCK_METHOD(StatusCode, set_active_plugin, (std::size_t index), (const, override));

        MOCK_METHOD(StatusCode, get_reader_threads, (int& count), (const, override));
        MOCK_METHOD(StatusCode, get_thread_buffer_size, (int& sizeMB), (const, override));
        MOCK_METHOD(StatusCode, set_reader_threads, (int count), (const, override));
        MOCK_METHOD(StatusCode, set_thread_buffer_size, (int sizeMB), (const, override));

        MOCK_METHOD(std::vector<std::filesystem::path>, get_plugin_paths, (), (const, override));
        MOCK_METHOD(StatusCode, add_plugin_path, (const std::filesystem::path& path), (const, override));
        MOCK_METHOD(StatusCode, remove_plugin_path, (const std::filesystem::path& path), (const, override));

        MOCK_METHOD((std::unordered_map<std::string, std::filesystem::path>), get_available_languages, (), (const, override));
        MOCK_METHOD(std::vector<std::filesystem::path>, get_language_paths, (), (const, override));
        MOCK_METHOD(StatusCode, add_language_path, (const std::filesystem::path& path), (const, override));
        MOCK_METHOD(StatusCode, remove_language_path, (const std::filesystem::path& path), (const, override));

        MOCK_METHOD(int, get_ui_state_int, (std::string_view key, int defaultValue), (const, override));
        MOCK_METHOD(void, set_ui_state_int, (std::string_view key, int value), (const, override));
    };
}
