//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pluginmemoryreader.hh>

#include <vertex/runtime/caller.hh>

namespace Vertex::Scanner
{
    StatusCode PluginMemoryReader::read_memory(const std::uint64_t address, const std::uint64_t size, void* buffer)
    {
        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = pluginOpt.value().get();
        return Runtime::get_status(Runtime::safe_call(plugin.internal_vertex_memory_read_process, address, size, static_cast<char*>(buffer)));
    }
} // namespace Vertex::Scanner
