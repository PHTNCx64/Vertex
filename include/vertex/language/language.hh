//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <nlohmann/json.hpp>

#include <unordered_map>
#include <string_view>
#include <vertex/language/ilanguage.hh>
#include <vertex/log/log.hh>

namespace Vertex::Language
{
    class Language final : public ILanguage
    {
    public:
        explicit Language(Log::Log& loggerService);

        StatusCode load_translation(const std::filesystem::path& path) override;
        [[nodiscard]] const std::string& fetch_translation(std::string_view path) override;
        [[nodiscard]] std::unordered_map<std::string, std::filesystem::path> fetch_all_languages() override;
        [[nodiscard]] StatusCode is_active_language(const std::filesystem::path& path) const noexcept override;

    private:
        void flatten_json(const nlohmann::json& json, std::string_view prefix);
        [[nodiscard]] static std::string build_path(std::string_view prefix, std::string_view key);

        Log::Log& m_loggerService;

        std::unordered_map<std::string, std::string> m_translations {};
        std::string m_fileTranslationContent {};
        std::string m_missingTranslationText {"[NO_TRANSLATION_PROVIDED]"};
        std::filesystem::path m_activeLanguagePath {};
    };
}
