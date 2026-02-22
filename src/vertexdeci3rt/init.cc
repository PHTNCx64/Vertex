//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0 with Plugin Interface exceptions.
//

#include <vertexdeci3rt/init.hh>
#include <vertexdeci3rt/config.hh>
#include <sdk/api.h>

#include <memory>

extern Runtime* g_pluginRuntime;

namespace DECI3
{
    namespace
    {
        std::unique_ptr<Deci3Context> g_context {};
    }

    Deci3Context* context() noexcept
    {
        return g_context.get();
    }

    void destroy_context() noexcept
    {
        g_context.reset();
    }

    SNRESULT initialize_communications()
    {
        const auto config = Config::load();
        if (!config)
        {
            g_pluginRuntime->vertex_log_error("Failed to load or parse deci3config.ini.");
            return -1;
        }

        const SNRESULT result = SNPS3InitTargetComms();
        if (result != SN_S_OK)
        {
            g_pluginRuntime->vertex_log_error("Failed to initialize DECI3 target communications.");
            return result;
        }

        g_context = std::make_unique<Deci3Context>(std::move(*config));
        return SN_S_OK;
    }
} // namespace DECI3
