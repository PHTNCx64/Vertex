//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/imemoryreader.hh>
#include <vertex/runtime/iloader.hh>
#include <functional>

namespace Vertex::Scanner
{
    class PluginMemoryReader final : public IMemoryReader
    {
    public:
        explicit PluginMemoryReader(Runtime::ILoader& loaderService)
            : m_loaderService(loaderService)
        {
        }

        StatusCode read_memory(std::uint64_t address, std::uint64_t size, void* buffer) override
        {
            auto pluginOpt = m_loaderService.get_active_plugin();
            if (!pluginOpt.has_value())
            {
                return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
            }

            const auto& plugin = pluginOpt.value().get();
            if (!plugin.internal_vertex_memory_read_process)
            {
                return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
            }

            return plugin.internal_vertex_memory_read_process(address, size, static_cast<char*>(buffer));
        }

        [[nodiscard]] bool is_valid() const override
        {
            const auto pluginOpt = m_loaderService.get_active_plugin();
            if (!pluginOpt.has_value())
            {
                return false;
            }

            const auto& plugin = pluginOpt.value().get();
            return plugin.is_loaded() && plugin.internal_vertex_memory_read_process != nullptr;
        }

    private:
        Runtime::ILoader& m_loaderService;
    };
} // namespace Vertex::Scanner
