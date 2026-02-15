//
// Mock for ISettings interface
//

#pragma once

#include <gmock/gmock.h>
#include <vertex/configuration/isettings.hh>

namespace Vertex::Testing::Mocks
{
    class MockISettings : public Configuration::ISettings
    {
    public:
        ~MockISettings() override = default;

        MOCK_METHOD(StatusCode, load_from_file, (const std::filesystem::path& path), (override));
        MOCK_METHOD(StatusCode, save_to_file, (const std::filesystem::path& path), (override));
        MOCK_METHOD(void, reset_to_defaults, (), (override));
        MOCK_METHOD(bool, validate, (), (const, override));

        MOCK_METHOD(nlohmann::json, get_settings, (), (const, override));
        MOCK_METHOD(void, update_settings, (const nlohmann::json& settings), (override));

        // Convenience methods
        MOCK_METHOD(bool, get_bool, (const std::string& key, bool defaultValue), (const, override));
        MOCK_METHOD(int, get_int, (const std::string& key, int defaultValue), (const, override));
        MOCK_METHOD(std::string, get_string, (const std::string& key, const std::string& defaultValue), (const, override));
        MOCK_METHOD(std::filesystem::path, get_path, (const std::string& key, const std::filesystem::path& defaultValue), (const, override));

        MOCK_METHOD(nlohmann::json, get_value, (const std::string& key), (const, override));
        MOCK_METHOD(void, set_value, (const std::string& key, const nlohmann::json& value), (override));
    };
} // namespace Vertex::Testing::Mocks
