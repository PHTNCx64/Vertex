//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/libraryloader.hh>
#include <vertex/runtime/caller.hh>

namespace Vertex::Runtime
{
    void Plugin::unload()
    {
        const auto result = Runtime::safe_call(internal_vertex_exit);
        if (!Runtime::status_ok(result))
        {
            // TODO: add logging
        }

        if (m_pluginHandle)
        {
            Vertex::Runtime::LibraryLoader::unload_library(m_pluginHandle);
            m_pluginHandle = nullptr;
        }

        m_pluginInfo = {};
        m_runtime = {};
    }

    Plugin::~Plugin()
    {
        unload();
    }
}
