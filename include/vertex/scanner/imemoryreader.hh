//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <cstdint>

namespace Vertex::Scanner
{
    class IMemoryReader
    {
    public:
        virtual ~IMemoryReader() = default;

        virtual StatusCode read_memory(std::uint64_t address, std::uint64_t size, void* buffer) = 0;

        [[nodiscard]] virtual bool is_valid() const = 0;
    };
} // namespace Vertex::Scanner
