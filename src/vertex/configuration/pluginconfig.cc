//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/configuration/pluginconfig.hh>
#include <vertex/configuration/filesystem.hh>
#include <fstream>
#include <algorithm>
#include <cstring>

namespace Vertex::Configuration
{
    StatusCode PluginConfig::load_config(const std::string& pluginFilename)
    {
        if (pluginFilename.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (const auto dirResult = ensure_config_directory(); dirResult != StatusCode::STATUS_OK)
        {
            return dirResult;
        }

        m_currentPluginFilename = pluginFilename;
        const auto configPath = get_config_path(pluginFilename);

        if (!std::filesystem::exists(configPath))
        {
            set_default_values();
            m_isModified = true;
            return save_config();
        }

        std::ifstream file(configPath);
        if (!file.is_open())
        {
            return StatusCode::STATUS_ERROR_FS_FILE_COULD_NOT_BE_OPENED;
        }

        try
        {
            m_config = nlohmann::json::parse(file);
        }
        catch (const nlohmann::json::parse_error&)
        {
            set_default_values();
            m_isModified = true;
            return save_config();
        }

        m_isModified = false;
        return StatusCode::STATUS_OK;
    }

    StatusCode PluginConfig::save_config()
    {
        if (m_currentPluginFilename.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (const auto dirResult = ensure_config_directory(); dirResult != StatusCode::STATUS_OK)
        {
            return dirResult;
        }

        const auto configPath = get_config_path(m_currentPluginFilename);

        std::ofstream file(configPath);
        if (!file.is_open())
        {
            return StatusCode::STATUS_ERROR_FS_FILE_COULD_NOT_BE_SAVED;
        }

        try
        {
            file << m_config.dump(4);
        }
        catch (const std::exception&)
        {
            return StatusCode::STATUS_ERROR_FS_FILE_COULD_NOT_BE_SAVED;
        }

        m_isModified = false;
        return StatusCode::STATUS_OK;
    }

    std::vector<std::string> PluginConfig::get_enabled_memory_attributes(const std::string& attributeType) const
    {
        return get_enabled_memory_attributes("memoryAttributes", attributeType);
    }

    std::vector<std::string> PluginConfig::get_enabled_memory_attributes(const std::string& section, const std::string& attributeType) const
    {
        std::vector<std::string> result{};

        try
        {
            if (m_config.contains(section) &&
                m_config[section].contains(attributeType) &&
                m_config[section][attributeType].is_array())
            {
                for (const auto& attr : m_config[section][attributeType])
                {
                    if (attr.is_string())
                    {
                        result.push_back(attr.get<std::string>());
                    }
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
        }

        return result;
    }

    void PluginConfig::set_enabled_memory_attributes(const std::string& attributeType, const std::vector<std::string>& attributes)
    {
        set_enabled_memory_attributes("memoryAttributes", attributeType, attributes);
    }

    void PluginConfig::set_enabled_memory_attributes(const std::string& section, const std::string& attributeType, const std::vector<std::string>& attributes)
    {
        try
        {
            if (!m_config.contains(section))
            {
                m_config[section] = nlohmann::json::object();
            }

            m_config[section][attributeType] = nlohmann::json::array();
            for (const auto& attr : attributes)
            {
                m_config[section][attributeType].push_back(attr);
            }

            m_isModified = true;
        }
        catch (const nlohmann::json::exception&)
        {
        }
    }

    bool PluginConfig::is_memory_attribute_enabled(const std::string& attributeType, const std::string& attributeName) const
    {
        return is_memory_attribute_enabled("memoryAttributes", attributeType, attributeName);
    }

    bool PluginConfig::is_memory_attribute_enabled(const std::string& section, const std::string& attributeType, const std::string& attributeName) const
    {
        const auto attributes = get_enabled_memory_attributes(section, attributeType);
        return std::ranges::find(attributes, attributeName) != attributes.end();
    }

    std::vector<std::string> PluginConfig::get_excluded_modules() const
    {
        std::vector<std::string> result{};

        try
        {
            if (m_config.contains("pointerScanExcludedModules") &&
                m_config["pointerScanExcludedModules"].is_array())
            {
                for (const auto& module : m_config["pointerScanExcludedModules"])
                {
                    if (module.is_string())
                    {
                        result.push_back(module.get<std::string>());
                    }
                }
            }
        }
        catch (const nlohmann::json::exception&)
        {
        }

        return result;
    }

    void PluginConfig::set_excluded_modules(const std::vector<std::string>& modules)
    {
        try
        {
            m_config["pointerScanExcludedModules"] = nlohmann::json::array();
            for (const auto& module : modules)
            {
                m_config["pointerScanExcludedModules"].push_back(module);
            }
            m_isModified = true;
        }
        catch (const nlohmann::json::exception&)
        {
        }
    }

    std::string PluginConfig::get_current_plugin() const
    {
        return m_currentPluginFilename;
    }

    bool PluginConfig::is_modified() const
    {
        return m_isModified;
    }

    std::filesystem::path PluginConfig::get_config_path(const std::string& pluginFilename)
    {
        const std::filesystem::path pluginPath(pluginFilename);
        const std::string baseName = pluginPath.stem().string();

        return Filesystem::get_configuration_path() / "plugins" / (baseName + ".json");
    }

    void PluginConfig::set_default_values()
    {
        m_config = nlohmann::json::object();
        m_config["memoryAttributes"] = nlohmann::json::object();
        m_config["memoryAttributes"]["protections"] = nlohmann::json::array();
        m_config["memoryAttributes"]["states"] = nlohmann::json::array();
        m_config["memoryAttributes"]["types"] = nlohmann::json::array();
        m_config["pointerScanMemoryAttributes"] = nlohmann::json::object();
        m_config["pointerScanMemoryAttributes"]["protections"] = nlohmann::json::array();
        m_config["pointerScanMemoryAttributes"]["states"] = nlohmann::json::array();
        m_config["pointerScanMemoryAttributes"]["types"] = nlohmann::json::array();
        m_config["pointerScanExcludedModules"] = nlohmann::json::array();
    }

    void PluginConfig::set_ui_value(const std::string& panelId, const std::string& fieldId, const UIValue& value, const UIFieldType type)
    {
        try
        {
            if (!m_config.contains("uiValues"))
            {
                m_config["uiValues"] = nlohmann::json::object();
            }

            if (!m_config["uiValues"].contains(panelId))
            {
                m_config["uiValues"][panelId] = nlohmann::json::object();
            }

            switch (type)
            {
            case VERTEX_UI_FIELD_NUMBER_INT:
            case VERTEX_UI_FIELD_SLIDER_INT:
                m_config["uiValues"][panelId][fieldId] = value.intValue;
                break;
            case VERTEX_UI_FIELD_NUMBER_FLOAT:
            case VERTEX_UI_FIELD_SLIDER_FLOAT:
                m_config["uiValues"][panelId][fieldId] = value.floatValue;
                break;
            case VERTEX_UI_FIELD_CHECKBOX:
                m_config["uiValues"][panelId][fieldId] = value.boolValue != 0;
                break;
            case VERTEX_UI_FIELD_TEXT:
            case VERTEX_UI_FIELD_PATH_FILE:
            case VERTEX_UI_FIELD_PATH_DIR:
            case VERTEX_UI_FIELD_DROPDOWN:
                m_config["uiValues"][panelId][fieldId] = std::string{value.stringValue};
                break;
            default:
                return;
            }

            m_isModified = true;
        }
        catch (const nlohmann::json::exception&)
        {
        }
    }

    std::optional<UIValue> PluginConfig::get_ui_value(const std::string& panelId, const std::string& fieldId, const UIFieldType type) const
    {
        try
        {
            if (!m_config.contains("uiValues") ||
                !m_config["uiValues"].contains(panelId) ||
                !m_config["uiValues"][panelId].contains(fieldId))
            {
                return std::nullopt;
            }

            const auto& jsonVal = m_config["uiValues"][panelId][fieldId];
            UIValue result{};

            switch (type)
            {
            case VERTEX_UI_FIELD_NUMBER_INT:
            case VERTEX_UI_FIELD_SLIDER_INT:
                result.intValue = jsonVal.get<int64_t>();
                break;
            case VERTEX_UI_FIELD_NUMBER_FLOAT:
            case VERTEX_UI_FIELD_SLIDER_FLOAT:
                result.floatValue = jsonVal.get<double>();
                break;
            case VERTEX_UI_FIELD_CHECKBOX:
                result.boolValue = jsonVal.get<bool>() ? 1 : 0;
                break;
            case VERTEX_UI_FIELD_TEXT:
            case VERTEX_UI_FIELD_PATH_FILE:
            case VERTEX_UI_FIELD_PATH_DIR:
            case VERTEX_UI_FIELD_DROPDOWN:
            {
                auto str = jsonVal.get<std::string>();
                std::strncpy(result.stringValue, str.c_str(), VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1);
                result.stringValue[VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1] = '\0';
                break;
            }
            default:
                return std::nullopt;
            }

            return result;
        }
        catch (const nlohmann::json::exception&)
        {
            return std::nullopt;
        }
    }

    void PluginConfig::clear_ui_values(const std::string& panelId)
    {
        try
        {
            if (m_config.contains("uiValues") && m_config["uiValues"].contains(panelId))
            {
                m_config["uiValues"].erase(panelId);
                m_isModified = true;
            }
        }
        catch (const nlohmann::json::exception&)
        {
        }
    }

    StatusCode PluginConfig::ensure_config_directory()
    {
        const auto pluginsConfigDir = Filesystem::get_configuration_path() / "plugins";

        try
        {
            if (!std::filesystem::exists(pluginsConfigDir))
            {
                std::filesystem::create_directories(pluginsConfigDir);
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
            return StatusCode::STATUS_ERROR_FS_DIRECTORY_COULD_NOT_BE_CREATED;
        }

        return StatusCode::STATUS_OK;
    }
}
