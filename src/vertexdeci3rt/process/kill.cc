//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexdeci3rt/init.hh>
#include <vertexdeci3rt/ps3tmapi.h>

#include <sdk/api.h>

extern Runtime* g_pluginRuntime;

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_kill()
    {
        const auto* ctx = DECI3::context();
        if (!ctx)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        const SNRESULT result = SNPS3ProcessKill(ctx->module.targetNumber, ctx->module.processId);
        if (SN_SUCCEEDED(result))
        {
            return StatusCode::STATUS_OK;
        }

        g_pluginRuntime->vertex_log_error("Failed to kill process with id %u. SNRESULT: %d", ctx->module.processId, result);
        return StatusCode::STATUS_ERROR_GENERAL;
    }
}
