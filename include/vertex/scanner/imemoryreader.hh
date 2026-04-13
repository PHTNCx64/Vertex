//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <cstdint>
#include <span>

namespace Vertex::Scanner
{
    struct BulkReadRequest final
    {
        std::uint64_t address{};
        std::uint64_t size{};
        void* buffer{};
    };

    struct BulkReadResult final
    {
        StatusCode status{StatusCode::STATUS_OK};
    };

    class IMemoryReader
    {
    public:
        virtual ~IMemoryReader() = default;

        virtual StatusCode read_memory(std::uint64_t address, std::uint64_t size, void* buffer) = 0;

        [[nodiscard]] virtual bool supports_bulk_read() const noexcept
        {
            return false;
        }

        [[nodiscard]] virtual std::uint32_t bulk_request_limit() const noexcept
        {
            return 0;
        }

        virtual StatusCode read_memory_bulk(std::span<const BulkReadRequest> requests, std::span<BulkReadResult> results)
        {
            static_cast<void>(requests);
            static_cast<void>(results);
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }
    };
} // namespace Vertex::Scanner
