//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>
#include <vertexdeci3rt/init.hh>

extern Runtime* g_pluginRuntime;

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open(const uint32_t processId)
    {
        auto* ctx = DECI3::context();
        if (!ctx)
        {
            g_pluginRuntime->vertex_log_error("Couldn't get DECI3 context. Is the plugin initialized correctly?");
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        const SNRESULT result = SNPS3ProcessAttach(ctx->module.targetNumber, 0, processId);
        if (SN_SUCCEEDED(result))
        {
            ctx->module.processId = processId;
            return StatusCode::STATUS_OK;
        }

        g_pluginRuntime->vertex_log_error("Failed to attach to process with id %u. SNRESULT: %d", processId, result);
        return StatusCode::STATUS_ERROR_GENERAL;
    }
}
