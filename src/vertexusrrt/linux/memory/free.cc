//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>

extern Runtime* g_pluginRuntime;

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_free(
    [[maybe_unused]] const std::uint64_t address,
    [[maybe_unused]] const std::uint64_t size)
{
    if (g_pluginRuntime)
    {
        g_pluginRuntime->vertex_log_error("[MemFree] Remote memory free is not supported on Linux in the user-mode runtime.");
    }

    return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
}
