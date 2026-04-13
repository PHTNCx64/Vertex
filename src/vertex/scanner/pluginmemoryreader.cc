//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/pluginmemoryreader.hh>

#include <vertex/runtime/caller.hh>

#include <algorithm>
#include <limits>
#include <sdk/memory.h>
#include <vector>

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

    bool PluginMemoryReader::supports_bulk_read() const noexcept
    {
        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return false;
        }

        return pluginOpt.value().get().internal_vertex_memory_read_process_bulk != nullptr;
    }

    std::uint32_t PluginMemoryReader::bulk_request_limit() const noexcept
    {
        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return 0;
        }

        const auto& plugin = pluginOpt.value().get();
        if (plugin.internal_vertex_memory_read_process_bulk == nullptr ||
            plugin.internal_vertex_memory_get_bulk_request_limit == nullptr)
        {
            return 0;
        }

        std::uint32_t maxRequestCount{};
        const auto result = Runtime::safe_call(plugin.internal_vertex_memory_get_bulk_request_limit, &maxRequestCount);
        if (!Runtime::status_ok(result) || maxRequestCount == 0)
        {
            return 0;
        }

        return maxRequestCount;
    }

    StatusCode PluginMemoryReader::read_memory_bulk(
        const std::span<const BulkReadRequest> requests,
        std::span<BulkReadResult> results)
    {
        if (requests.empty())
        {
            return StatusCode::STATUS_OK;
        }

        if (requests.size() != results.size())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (requests.size() > std::numeric_limits<std::uint32_t>::max())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const bool hasNullBuffer = std::ranges::any_of(requests, [](const auto& request)
        {
            return request.buffer == nullptr && request.size > 0;
        });
        if (hasNullBuffer)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto& plugin = pluginOpt.value().get();
        if (plugin.internal_vertex_memory_read_process_bulk == nullptr)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
        }

        std::vector<::BulkReadRequest> sdkRequests(requests.size());
        std::vector<::BulkReadResult> sdkResults(results.size());
        for (std::size_t index{}; index < requests.size(); ++index)
        {
            sdkRequests[index] = {requests[index].address, requests[index].size, requests[index].buffer};
        }

        std::uint32_t maxPerBulkCall = static_cast<std::uint32_t>(sdkRequests.size());
        if (plugin.internal_vertex_memory_get_bulk_request_limit != nullptr)
        {
            std::uint32_t queriedLimit{};
            const auto limitResult = Runtime::safe_call(
                plugin.internal_vertex_memory_get_bulk_request_limit,
                &queriedLimit);
            if (Runtime::status_ok(limitResult) && queriedLimit > 0)
            {
                maxPerBulkCall = std::min(maxPerBulkCall, queriedLimit);
            }
        }
        maxPerBulkCall = std::max<std::uint32_t>(1, maxPerBulkCall);

        std::size_t offset{};
        while (offset < sdkRequests.size())
        {
            const auto remaining = sdkRequests.size() - offset;
            const auto chunkCount = static_cast<std::uint32_t>(std::min<std::size_t>(remaining, maxPerBulkCall));

            const auto callResult = Runtime::safe_call(
                plugin.internal_vertex_memory_read_process_bulk,
                sdkRequests.data() + offset,
                sdkResults.data() + offset,
                chunkCount);
            const auto status = Runtime::get_status(callResult);
            if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
            {
                return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
            }
            if (status != StatusCode::STATUS_OK)
            {
                return status;
            }

            offset += chunkCount;
        }

        for (std::size_t index{}; index < results.size(); ++index)
        {
            results[index].status = sdkResults[index].status;
        }

        return StatusCode::STATUS_OK;
    }
} // namespace Vertex::Scanner
