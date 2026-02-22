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

    private:
        Runtime::ILoader& m_loaderService;
    };
} // namespace Vertex::Scanner
