#pragma once

#include <gmock/gmock.h>
#include <string_view>
#include <vertex/language/ilanguage.hh>

namespace Vertex::Testing::Mocks
{
    class MockILanguage : public Language::ILanguage
    {
    public:
        ~MockILanguage() override = default;

        MOCK_METHOD(StatusCode, load_translation, (const std::filesystem::path& path), (override));
        MOCK_METHOD(const std::string&, fetch_translation, (std::string_view path), (override));
        MOCK_METHOD((std::unordered_map<std::string, std::filesystem::path>), fetch_all_languages, (), (override));
        MOCK_METHOD(StatusCode, is_active_language, (const std::filesystem::path& path), (const, noexcept, override));
    };
}
