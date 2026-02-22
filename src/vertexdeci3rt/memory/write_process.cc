//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexdeci3rt/init.hh>
#include <vertexdeci3rt/ps3tmapi.h>
#include <sdk/api.h>

extern Runtime* g_pluginRuntime;

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process(const std::uint64_t address, const std::uint64_t size, const char* buffer)
{
    if (!buffer || size == 0)
    {
        g_pluginRuntime->vertex_log_error("Invalid parameter. Buffer or size is invalid.");
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto ctx = DECI3::context();
    if (!ctx)
    {
        g_pluginRuntime->vertex_log_error("Invalid context! The library doesn't seem to be initialized.");
        return StatusCode::STATUS_ERROR_INVALID_STATE;
    }

    constexpr int ppuUnit  = 0;
    constexpr int threadId = 0;

    const SNRESULT result = SNPS3ProcessSetMemory(ctx->module.targetNumber, ppuUnit, ctx->module.processId, threadId, address, size, reinterpret_cast<BYTE*>(const_cast<char*>(buffer)));
    if (SN_SUCCEEDED(result))
    {
        return StatusCode::STATUS_OK;
    }

    g_pluginRuntime->vertex_log_error("Failed to write memory. SNRESULT: %d", result);
    return StatusCode::STATUS_ERROR_MEMORY_WRITE;
}
