//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/plugin.hh>

#include <vertex/runtime/caller.hh>

#include <fmt/format.h>

namespace Vertex::Runtime
{
    void Plugin::release_library()
    {
        if (m_library.has_value())
        {
            const auto result = safe_call(internal_vertex_exit);
            if (!status_ok(result))
            {
                m_logService.get().log_error(fmt::format("[Plugin Unload] vertex_exit failed for: {}", get_filename()));
            }
            m_library.reset();
        }

        reset_function_pointers();
        m_pluginInfo = {};
        m_runtime = {};
    }

    Plugin::~Plugin()
    {
        release_library();
    }
}
