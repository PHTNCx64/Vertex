//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/libraryloader.hh>
#include <vertex/runtime/caller.hh>

namespace Vertex::Runtime
{
    Plugin::~Plugin()
    {
        const auto result = Runtime::safe_call(internal_vertex_exit);
        if (!Runtime::status_ok(result))
        {
            // TODO: add logging
        }

        if (m_pluginHandle)
        {
            // TODO: Graceful handling of plugin cleanup and unloading.
            Vertex::Runtime::LibraryLoader::unload_library(m_pluginHandle);
            m_pluginHandle = nullptr;
        }
    }
}
