//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vector>
#include <string>
#include <sdk/statuscode.h>
#include <sdk/memory.h>
#include <vertex/runtime/iloader.hh>
#include <vertex/configuration/ipluginconfig.hh>

namespace Vertex::Model
{
    struct MemoryAttributeOptionData final
    {
        std::string name;
        MemoryAttributeType type;
        VertexOptionState_t stateFunction;
        bool isValid;
        bool currentState;
    };

    class MemoryAttributeModel final
    {
    public:
        explicit MemoryAttributeModel(Runtime::ILoader& loader, Configuration::IPluginConfig& pluginConfig, std::string configSection = "memoryAttributes", bool fallbackToPluginState = true);
        ~MemoryAttributeModel() = default;

        [[nodiscard]] StatusCode fetch_memory_attribute_options(std::vector<MemoryAttributeOptionData>& options) const;

        [[nodiscard]] bool has_memory_attribute_options() const;

        [[nodiscard]] StatusCode save_memory_attribute_states(const std::vector<MemoryAttributeOptionData>& options) const;

        [[nodiscard]] StatusCode load_and_apply_saved_states() const;

    private:
        [[nodiscard]] static std::string get_attribute_type_key(MemoryAttributeType type);

        Runtime::ILoader& m_loader;
        Configuration::IPluginConfig& m_pluginConfig;
        std::string m_configSection;
        bool m_fallbackToPluginState{};
    };
}
