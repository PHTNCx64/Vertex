//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/utility.hh>
#include <vertex/configuration/filesystem.hh>
#include <vertex/language/language.hh>

#include <fmt/format.h>
#include <fstream>
#include <nlohmann/json.hpp>

namespace Vertex::Language
{
    Language::Language(Log::Log& loggerService) : m_loggerService(loggerService) {}

    std::string Language::build_path(const std::string_view prefix, const std::string_view key)
    {
        if (prefix.empty())
        {
            return std::string{key};
        }

        return std::string{prefix} + "." + std::string{key};
    }

    void Language::flatten_json(const nlohmann::json& json, const std::string_view prefix)
    {
        for (auto it = json.begin(); it != json.end(); ++it)
        {
            const std::string currentPath = build_path(prefix, it.key());

            if (it.value().is_object())
            {
                flatten_json(it.value(), currentPath);
            }
            else if (it.value().is_string())
            {
                m_translations[currentPath] = it.value().get<std::string>();
            }
            else
            {
                m_translations[currentPath] = it.value().dump();
            }
        }
    }

    const std::string& Language::fetch_translation(const std::string_view path)
    {
        const auto pathStr = std::string{path};
        if (const auto& it = m_translations.find(pathStr); it != m_translations.end())
        {
            return it->second;
        }

        return m_missingTranslationText;
    }

    StatusCode Language::load_translation(const std::filesystem::path& path)
    {
        if (!std::filesystem::exists(path))
        {
            m_loggerService.log_error(fmt::format("Translation file not found: {}", path.string()));
            return STATUS_ERROR_FILE_NOT_FOUND;
        }

        m_translations.clear();
        m_fileTranslationContent.clear();

        try
        {
            std::ifstream file(path, std::ios::binary | std::ios::ate);
            if (!file.is_open())
            {
                m_loggerService.log_error(fmt::format("Failed to open translation file: {}", path.string()));
                return STATUS_ERROR_FS_FILE_OPEN_FAILED;
            }

            const std::streamsize fileSize = file.tellg();
            file.seekg(0, std::ios::beg);

            m_fileTranslationContent.reserve(fileSize);
            m_fileTranslationContent.assign(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
            file.close();

            if (m_fileTranslationContent.size() >= 3 &&
                static_cast<unsigned char>(m_fileTranslationContent[0]) == 0xEF &&
                static_cast<unsigned char>(m_fileTranslationContent[1]) == 0xBB &&
                static_cast<unsigned char>(m_fileTranslationContent[2]) == 0xBF)
            {
                m_fileTranslationContent.erase(0, 3);
            }

            const nlohmann::json parsedContent = nlohmann::json::parse(m_fileTranslationContent);
            flatten_json(parsedContent, EMPTY_STRING);

            m_activeLanguagePath = path;
            m_loggerService.log_info(fmt::format("Successfully loaded translation: {}", path.string()));

            return STATUS_OK;
        }
        catch (const nlohmann::json::parse_error& e)
        {
            m_loggerService.log_error(fmt::format("JSON parse error in translation file '{}': {}", path.string(), e.what()));
            return STATUS_ERROR_FS_FILE_READ_FAILED;
        }
        catch (const std::exception& e)
        {
            m_loggerService.log_error(fmt::format("Error reading translation file '{}': {}", path.string(), e.what()));
            return STATUS_ERROR_FS_FILE_READ_FAILED;
        }
    }

    StatusCode Language::is_active_language(const std::filesystem::path& path) const noexcept
    {
        return path == m_activeLanguagePath ? STATUS_OK : STATUS_ERROR_GENERAL;
    }

    std::unordered_map<std::string, std::filesystem::path> Language::fetch_all_languages()
    {
        static std::unordered_map<std::string, std::filesystem::path> languages{};
        static bool initialized{};

        if (initialized)
        {
            return languages;
        }

        try
        {
            const auto languagePath = Configuration::Filesystem::get_language_path();

            std::error_code ec{};
            if (!std::filesystem::exists(languagePath, ec) || ec)
            {
                m_loggerService.log_warn(fmt::format("Language directory does not exist: {}", languagePath.string()));
                initialized = true;
                return languages;
            }

            if (!std::filesystem::is_directory(languagePath, ec) || ec)
            {
                m_loggerService.log_error(fmt::format("Language path is not a directory: {}", languagePath.string()));
                initialized = true;
                return languages;
            }

            for (const auto& entry : std::filesystem::directory_iterator(languagePath, ec))
            {
                if (ec)
                {
                    m_loggerService.log_warn(fmt::format("Error iterating language directory: {}", ec.message()));
                    continue;
                }

                if (!entry.is_regular_file(ec) || ec)
                {
                    continue;
                }

                const auto& filePath = entry.path();

                if (filePath.extension() != FileTypes::CONFIGURATION_EXTENSION)
                {
                    continue;
                }

                const auto stem = filePath.stem();
                const std::string languageName = stem.string();
                languages[languageName] = std::filesystem::absolute(filePath);
            }

            m_loggerService.log_info(fmt::format("Found {} language file(s)", languages.size()));
            initialized = true;
        }
        catch (const std::exception& e)
        {
            m_loggerService.log_error(fmt::format("Exception while fetching languages: {}", e.what()));
            initialized = true;
        }

        return languages;
    }
}
