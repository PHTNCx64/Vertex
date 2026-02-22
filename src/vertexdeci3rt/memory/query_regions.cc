//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <sdk/api.h>
#include <vertexdeci3rt/init.hh>
#include <vertexdeci3rt/ps3tmapi.h>

#include <cstdlib>
#include <span>
#include <ranges>

extern Runtime* g_pluginRuntime;

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, std::uint64_t* size)
{
    if (!regions || !size)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    auto* ctx = DECI3::context();
    if (!ctx)
    {
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    std::uint32_t areaCount {};
    std::uint32_t bufferSize {};

    SNRESULT result = SNPS3GetVirtualMemoryInfo(ctx->module.targetNumber, ctx->module.processId, TRUE, &areaCount, &bufferSize, nullptr);
    if (SN_FAILED(result))
    {
        g_pluginRuntime->vertex_log_error("Memory region buffer size query failed! SNRESULT: %d", result);
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    if (areaCount == 0 || bufferSize == 0)
    {
        *regions = nullptr;
        *size    = 0;
        return StatusCode::STATUS_OK;
    }

    auto buffer = std::make_unique<std::uint8_t[]>(bufferSize);

    result = SNPS3GetVirtualMemoryInfo(ctx->module.targetNumber, ctx->module.processId, TRUE, &areaCount, &bufferSize, buffer.get());
    if (SN_FAILED(result))
    {
        g_pluginRuntime->vertex_log_error("Memory region retrieval failed! SNRESULT: %d", result);
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    const auto* areas = reinterpret_cast<const SNPS3VirtualMemoryArea*>(buffer.get());

    *size    = areaCount;
    *regions = static_cast<MemoryRegion*>(std::malloc(sizeof(MemoryRegion) * areaCount));
    if (!*regions)
    {
        return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    std::ranges::transform(std::span { areas, areaCount }, *regions, [](const SNPS3VirtualMemoryArea& area) {
        return MemoryRegion { .baseModuleName = nullptr, .baseAddress = area.uAddress, .regionSize = area.uVSize };
    });

    return StatusCode::STATUS_OK;
}
