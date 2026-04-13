//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/imemoryreader.hh>
#include <vertex/runtime/iloader.hh>

namespace Vertex::Scanner
{
    class PluginMemoryReader final : public IMemoryReader
    {
    public:
        explicit PluginMemoryReader(Runtime::ILoader& loaderService)
            : m_loaderService{loaderService}
        {
        }

        StatusCode read_memory(std::uint64_t address, std::uint64_t size, void* buffer) override;
        [[nodiscard]] bool supports_bulk_read() const noexcept override;
        [[nodiscard]] std::uint32_t bulk_request_limit() const noexcept override;
        StatusCode read_memory_bulk(std::span<const BulkReadRequest> requests, std::span<BulkReadResult> results) override;

    private:
        Runtime::ILoader& m_loaderService;
    };
} // namespace Vertex::Scanner
