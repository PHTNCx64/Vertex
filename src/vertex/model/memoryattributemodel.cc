//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/model/memoryattributemodel.hh>
#include <vertex/runtime/caller.hh>
#include <ranges>
#include <algorithm>

namespace Vertex::Model
{
    MemoryAttributeModel::MemoryAttributeModel(Runtime::ILoader& loader, Configuration::IPluginConfig& pluginConfig, std::string configSection, const bool fallbackToPluginState)
        : m_loader{loader},
          m_pluginConfig{pluginConfig},
          m_configSection{std::move(configSection)},
          m_fallbackToPluginState{fallbackToPluginState}
    {
    }

    StatusCode MemoryAttributeModel::fetch_memory_attribute_options(std::vector<MemoryAttributeOptionData>& options) const
    {
        options.clear();

        auto activePlugin = m_loader.get_active_plugin();
        if (!activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NO_PLUGIN_ACTIVE;
        }

        MemoryAttributeOption* rawOptions{};
        std::uint32_t count{};

        const auto& plugin = activePlugin.value().get();
        const auto result = Runtime::safe_call(plugin.internal_vertex_memory_construct_attribute_filters, &rawOptions, &count);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result) || !rawOptions || count == 0)
        {
            return Runtime::status_ok(result) ? StatusCode::STATUS_ERROR_PLUGIN_NO_MEMORY_ATTRIBUTE_OPTIONS : status;
        }

        const std::string pluginFilename = plugin.get_filename();
        if (m_pluginConfig.get_current_plugin() != pluginFilename)
        {
            m_pluginConfig.load_config(pluginFilename);
        }

        for (const auto& option : std::span{rawOptions, count})
        {
            MemoryAttributeOptionData data{};
            data.isValid = true;
            data.currentState = false;

            if (option.memoryAttributeName && std::strlen(option.memoryAttributeName) > 0)
            {
                data.name = option.memoryAttributeName;
            }
            else
            {
                data.name = "Unknown Memory Attribute";
                data.isValid = false;
            }

            if (!option.stateFunction)
            {
                data.isValid = false;
            }
            else
            {
                data.stateFunction = option.stateFunction;
            }

            data.type = option.memoryAttributeType;

            const std::string typeKey = get_attribute_type_key(data.type);
            const bool savedState = m_pluginConfig.is_memory_attribute_enabled(m_configSection, typeKey, data.name);

            if (savedState)
            {
                data.currentState = true;
                if (option.stateFunction)
                {
                    option.stateFunction(true);
                }
            }
            else if (m_fallbackToPluginState && option.currentState)
            {
                data.currentState = *option.currentState;
            }

            if (data.isValid && (data.type == VERTEX_PROTECTION || data.type == VERTEX_STATE || data.type == VERTEX_TYPE))
            {
                options.push_back(data);
            }
        }

        return options.empty() ? StatusCode::STATUS_ERROR_PLUGIN_NO_MEMORY_ATTRIBUTE_OPTIONS : StatusCode::STATUS_OK;
    }

    bool MemoryAttributeModel::has_memory_attribute_options() const
    {
        std::vector<MemoryAttributeOptionData> options;
        return fetch_memory_attribute_options(options) == StatusCode::STATUS_OK;
    }

    StatusCode MemoryAttributeModel::save_memory_attribute_states(const std::vector<MemoryAttributeOptionData>& options) const
    {
        const auto activePlugin = m_loader.get_active_plugin();
        if (!activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NO_PLUGIN_ACTIVE;
        }

        const std::string pluginFilename = activePlugin.value().get().get_filename();
        if (m_pluginConfig.get_current_plugin() != pluginFilename)
        {
            m_pluginConfig.load_config(pluginFilename);
        }

        auto enabledOptions = options | std::views::filter([](const auto& opt) { return opt.currentState; });

        auto enabledProtections = enabledOptions
            | std::views::filter([](const auto& opt) { return opt.type == VERTEX_PROTECTION; })
            | std::views::transform([](const auto& opt) { return opt.name; })
            | std::ranges::to<std::vector>();

        auto enabledStates = enabledOptions
            | std::views::filter([](const auto& opt) { return opt.type == VERTEX_STATE; })
            | std::views::transform([](const auto& opt) { return opt.name; })
            | std::ranges::to<std::vector>();

        auto enabledTypes = enabledOptions
            | std::views::filter([](const auto& opt) { return opt.type == VERTEX_TYPE; })
            | std::views::transform([](const auto& opt) { return opt.name; })
            | std::ranges::to<std::vector>();

        m_pluginConfig.set_enabled_memory_attributes(m_configSection, "protections", enabledProtections);
        m_pluginConfig.set_enabled_memory_attributes(m_configSection, "states", enabledStates);
        m_pluginConfig.set_enabled_memory_attributes(m_configSection, "types", enabledTypes);

        return m_pluginConfig.save_config();
    }

    StatusCode MemoryAttributeModel::load_and_apply_saved_states() const
    {
        const auto activePlugin = m_loader.get_active_plugin();
        if (!activePlugin.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NO_PLUGIN_ACTIVE;
        }

        const std::string pluginFilename = activePlugin.value().get().get_filename();
        return m_pluginConfig.load_config(pluginFilename);
    }

    std::string MemoryAttributeModel::get_attribute_type_key(const MemoryAttributeType type)
    {
        constexpr std::array typeKeys = std::to_array<std::string_view>({
            "protections",
            "states",
            "types",
            "other"
        });

        switch (type)
        {
        case VERTEX_PROTECTION:
            return std::string{typeKeys[0]};
        case VERTEX_STATE:
            return std::string{typeKeys[1]};
        case VERTEX_TYPE:
            return std::string{typeKeys[2]};
        default:
            return std::string{typeKeys[3]};
        }
    }
}
