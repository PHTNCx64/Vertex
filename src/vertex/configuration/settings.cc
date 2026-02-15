//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/configuration/settings.hh>
#include <vertex/configuration/filesystem.hh>
#include <vertex/utility.hh>
#include <fstream>
#include <thread>
#include <algorithm>
#include <sstream>
#include <string_view>

namespace Vertex::Configuration
{
    Settings::Settings(Log::ILog& log)
        : m_log(log)
    {
        const auto settingsJsonFilePath = Filesystem::get_configuration_path() / "Settings.json";

        const StatusCode status = Settings::load_from_file(settingsJsonFilePath);
        if (status != StatusCode::STATUS_OK)
        {
            log_error("Failed to load settings from file, using defaults. Path: " + settingsJsonFilePath.string());
            Settings::reset_to_defaults();
            Settings::save_to_file(settingsJsonFilePath);
        }
    }

    void Settings::log_error(std::string_view message) const
    {
        m_log.log_error(std::string{"[Settings] "} + std::string{message});
    }

    std::vector<std::string> Settings::split_key(const std::string& key) const
    {
        std::vector<std::string> parts{};
        std::string currentPart{};

        for (const char c : key)
        {
            if (c == '.')
            {
                if (!currentPart.empty())
                {
                    parts.push_back(currentPart);
                    currentPart.clear();
                }
            }
            else
            {
                currentPart += c;
            }
        }

        if (!currentPart.empty())
        {
            parts.push_back(currentPart);
        }

        return parts;
    }

    StatusCode Settings::load_from_file(const std::filesystem::path& path)
    {
        std::error_code ec{};
        if (!std::filesystem::exists(path, ec) || ec)
        {
            log_error("Settings file not found: " + path.string());
            return StatusCode::STATUS_ERROR_FILE_NOT_FOUND;
        }

        std::ifstream fs(path);
        if (!fs.is_open())
        {
            log_error("Failed to open settings file: " + path.string());
            return StatusCode::STATUS_ERROR_FS_FILE_OPEN_FAILED;
        }

        std::stringstream buffer{};
        buffer << fs.rdbuf();
        fs.close();

        const std::string content = buffer.str();
        if (content.empty())
        {
            log_error("Settings file is empty: " + path.string());
            return StatusCode::STATUS_ERROR_FS_JSON_PARSE_FAILED;
        }

        m_settings = nlohmann::json::parse(content, nullptr, false);

        if (m_settings.is_discarded())
        {
            log_error("Failed to parse settings JSON: " + path.string());
            m_settings = nlohmann::json{};
            return StatusCode::STATUS_ERROR_FS_JSON_PARSE_FAILED;
        }

        if (!m_settings.is_object())
        {
            log_error("Settings JSON is not an object: " + path.string());
            m_settings = nlohmann::json{};
            return StatusCode::STATUS_ERROR_FS_JSON_TYPE_MISMATCH;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode Settings::save_to_file(const std::filesystem::path& path)
    {
        std::filesystem::path absolutePath = path;

        std::error_code ec{};
        if (path.is_relative())
        {
            absolutePath = std::filesystem::current_path(ec) / path;
            if (ec)
            {
                log_error("Failed to get current path: " + ec.message());
                return StatusCode::STATUS_ERROR_GENERAL;
            }
        }

        const auto parentPath = absolutePath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath, ec))
        {
            if (ec)
            {
                log_error("Failed to check parent directory existence: " + ec.message());
            }

            const bool created = std::filesystem::create_directories(parentPath, ec);
            if (!created || ec)
            {
                log_error("Failed to create directories for settings: " + parentPath.string() +
                          (ec ? " Error: " + ec.message() : EMPTY_STRING));
                return StatusCode::STATUS_ERROR_FS_DIR_CREATION_FAILED;
            }
        }

        std::ofstream file(absolutePath, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            log_error("Failed to open settings file for writing: " + absolutePath.string());
            return StatusCode::STATUS_ERROR_FS_FILE_OPEN_FAILED;
        }

        const std::string jsonStr = m_settings.dump(4);
        file << jsonStr;
        file.close();

        if (file.fail())
        {
            log_error("Failed to write settings to file: " + absolutePath.string());
            return StatusCode::STATUS_ERROR_FS_FILE_WRITE_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    void Settings::reset_to_defaults()
    {
        set_default_values();
    }

    bool Settings::validate() const
    {
        const int autoSaveInterval = get_int("general.autoSaveInterval", 5);
        if (autoSaveInterval < 1 || autoSaveInterval > 3600)
        {
            return false;
        }

        const int readerThreads = get_int("memoryScan.readerThreads", 1);
        if (readerThreads < 1 || readerThreads > 64)
        {
            return false;
        }

        const int threadBufferSizeMB = get_int("memoryScan.threadBufferSizeMB", 32);

        return threadBufferSizeMB >= 1 && threadBufferSizeMB <= 1024;
    }

    bool Settings::get_bool(const std::string& key, bool defaultValue) const
    {
        const auto value = navigate_to_key(key);
        if (!value.has_value())
        {
            return defaultValue;
        }

        if (!value->is_boolean())
        {
            return defaultValue;
        }

        return value->get<bool>();
    }

    int Settings::get_int(const std::string& key, int defaultValue) const
    {
        const auto value = navigate_to_key(key);
        if (!value.has_value())
        {
            return defaultValue;
        }

        if (!value->is_number_integer())
        {
            return defaultValue;
        }

        return value->get<int>();
    }

    std::string Settings::get_string(const std::string& key, const std::string& defaultValue) const
    {
        const auto value = navigate_to_key(key);
        if (!value.has_value())
        {
            return defaultValue;
        }

        if (!value->is_string())
        {
            return defaultValue;
        }

        return value->get<std::string>();
    }

    std::filesystem::path Settings::get_path(const std::string& key, const std::filesystem::path& defaultValue) const
    {
        const auto value = navigate_to_key(key);
        if (!value.has_value())
        {
            return defaultValue;
        }

        if (!value->is_string())
        {
            return defaultValue;
        }

        return std::filesystem::path{value->get<std::string>()};
    }

    void Settings::set_value(const std::string& key, const nlohmann::json& value)
    {
        set_nested_value(key, value);
    }

    nlohmann::json Settings::get_value(const std::string& key) const
    {
        const auto value = navigate_to_key(key);
        if (!value.has_value())
        {
            return {};
        }
        return *value;
    }

    void Settings::set_default_values()
    {
        m_settings = nlohmann::json::object();

        m_settings["general"]["autoSaveEnabled"] = true;
        m_settings["general"]["autoSaveInterval"] = 5;
        m_settings["general"]["guiSavingEnabled"] = true;
        m_settings["general"]["rememberWindowPos"] = true;
        m_settings["general"]["enableLogging"] = true;
        m_settings["general"]["theme"] = 0;

        const int hardwareConcurrency = std::max(1, static_cast<int>(std::thread::hardware_concurrency()));
        m_settings["memoryScan"]["readerThreads"] = std::max(1, hardwareConcurrency / 2);
        m_settings["memoryScan"]["threadBufferSizeMB"] = 32;

        set_default_language();

        m_settings["plugins"]["activePluginPath"] = EMPTY_STRING;
        m_settings["plugins"]["pluginPaths"] = nlohmann::json::array();
        m_settings["plugins"]["pluginPaths"].push_back(Filesystem::get_plugin_path().string());

        m_settings["uiState"]["mainView"]["valueTypeIndex"] = 2;
        m_settings["uiState"]["mainView"]["scanTypeIndex"] = 0;
        m_settings["uiState"]["mainView"]["endiannessTypeIndex"] = 0;
        m_settings["uiState"]["mainView"]["hexadecimalEnabled"] = false;
        m_settings["uiState"]["mainView"]["alignmentEnabled"] = true;
        m_settings["uiState"]["mainView"]["alignmentValue"] = 4;

        m_settings["uiState"]["settingsView"]["lastTabIndex"] = 0;

        m_settings["uiState"]["debuggerView"]["breakpointsPanelExpanded"] = true;
        m_settings["uiState"]["debuggerView"]["registersPanelExpanded"] = true;
        m_settings["uiState"]["debuggerView"]["stackPanelExpanded"] = true;

        m_settings["uiState"]["memoryAttributeView"]["lastSelectedProtections"] = nlohmann::json::array();
        m_settings["uiState"]["memoryAttributeView"]["lastSelectedStates"] = nlohmann::json::array();
        m_settings["uiState"]["memoryAttributeView"]["lastSelectedTypes"] = nlohmann::json::array();

        m_settings["uiState"]["analyticsView"]["refreshInterval"] = 1000;
        m_settings["uiState"]["analyticsView"]["autoRefreshEnabled"] = true;

        m_settings["uiState"]["processListView"]["filterTypeIndex"] = 1;
    }

    void Settings::set_default_language()
    {
        std::error_code ec{};
        const auto languageDir = Filesystem::get_language_path();
        const auto englishPath = languageDir / "English_US.json";

        if (std::filesystem::exists(englishPath, ec) && !ec)
        {
            m_settings["language"]["languagePath"] = languageDir.string();
            m_settings["language"]["activeLanguage"] = "English_US.json";
            return;
        }

        if (!std::filesystem::exists(languageDir, ec) || ec)
        {
            log_error("Language directory does not exist: " + languageDir.string());
            m_settings["language"]["languagePath"] = EMPTY_STRING;
            m_settings["language"]["activeLanguage"] = EMPTY_STRING;
            return;
        }

        auto dirIter = std::filesystem::directory_iterator(languageDir, ec);
        if (ec)
        {
            log_error("Failed to iterate language directory: " + ec.message());
            m_settings["language"]["languagePath"] = EMPTY_STRING;
            m_settings["language"]["activeLanguage"] = EMPTY_STRING;
            return;
        }

        for (const auto& entry : dirIter)
        {
            if (entry.is_regular_file(ec) && !ec && entry.path().extension() == ".json")
            {
                m_settings["language"]["languagePath"] = languageDir.string();
                m_settings["language"]["activeLanguage"] = entry.path().filename().string();
                return;
            }
        }

        log_error("No language files found in: " + languageDir.string());
        m_settings["language"]["languagePath"] = EMPTY_STRING;
        m_settings["language"]["activeLanguage"] = EMPTY_STRING;
    }

    std::optional<nlohmann::json> Settings::navigate_to_key(const std::string& key) const
    {
        const auto keyParts = split_key(key);
        if (keyParts.empty())
        {
            return std::nullopt;
        }

        const nlohmann::json* current = &m_settings;

        for (const auto& part : keyParts)
        {
            if (!current->is_object())
            {
                return std::nullopt;
            }

            if (!current->contains(part))
            {
                return std::nullopt;
            }

            current = &(*current)[part];
        }

        return *current;
    }

    void Settings::set_nested_value(const std::string& key, const nlohmann::json& value)
    {
        const auto keyParts = split_key(key);
        if (keyParts.empty())
        {
            log_error("Cannot set value for empty key");
            return;
        }

        nlohmann::json* current = &m_settings;

        for (std::size_t i = 0; i < keyParts.size() - 1; ++i)
        {
            const auto& part = keyParts[i];

            if (!current->is_object())
            {
                *current = nlohmann::json::object();
            }

            if (!current->contains(part) || !(*current)[part].is_object())
            {
                (*current)[part] = nlohmann::json::object();
            }

            current = &(*current)[part];
        }

        if (!current->is_object())
        {
            *current = nlohmann::json::object();
        }

        (*current)[keyParts.back()] = value;
    }

}
