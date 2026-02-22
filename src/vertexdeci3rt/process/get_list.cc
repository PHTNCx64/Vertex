//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <vertexdeci3rt/init.hh>

#include <algorithm>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

extern Runtime* g_pluginRuntime;

namespace
{
    constexpr void copy_to(std::span<char> dest, std::string_view src) noexcept
    {
        const auto count = std::min(src.size(), dest.size() - 1);
        std::ranges::copy_n(src.data(), count, dest.data());
        dest[count] = '\0';
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto* ctx = DECI3::context();
        if (!ctx)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        std::uint32_t processCount {};
        const SNRESULT result = SNPS3ProcessList(ctx->module.targetNumber, &processCount, nullptr);
        if (SN_FAILED(result))
        {
            g_pluginRuntime->vertex_log_error("Process List count retrieval failed! %d", result);
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (!list)
        {
            *count = processCount;
            return StatusCode::STATUS_OK;
        }

        if (!*list)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t bufferSize = *count;
        if (bufferSize == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::vector<std::uint32_t> listBuffer(processCount);
        std::uint32_t listCount = processCount;
        if (SN_FAILED(SNPS3ProcessList(ctx->module.targetNumber, &listCount, listBuffer.data())))
        {
            g_pluginRuntime->vertex_log_error("Process List retrieval failed!");
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        const std::uint32_t copyCount = std::min(bufferSize, listCount);
        ProcessInformation* buffer    = *list;
        std::uint32_t written {};

        for (const auto i : std::views::iota(0u, copyCount))
        {
            const std::uint32_t pid = listBuffer[i];
            std::uint32_t infoSize {};
            if (SN_FAILED(SNPS3ProcessInfo(ctx->module.targetNumber, pid, &infoSize, nullptr)))
            {
                continue;
            }

            std::vector<std::uint8_t> infoBuffer(infoSize);
            auto* processInfo = reinterpret_cast<SNPS3PROCESSINFO*>(infoBuffer.data());
            if (SN_FAILED(SNPS3ProcessInfo(ctx->module.targetNumber, pid, &infoSize, processInfo)))
            {
                continue;
            }

            auto& [processName, processOwner, processId, parentProcessId] = buffer[written];
            processId       = pid;
            parentProcessId = processInfo->Hdr.uParentProcessID;
            copy_to(processName, processInfo->Hdr.szPath);
            copy_to(processOwner, "N/A");
            ++written;
        }

        *count = written;

        if (listCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }
}
