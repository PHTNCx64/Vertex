//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>
#include <tuple>

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_change_protection(const std::uint64_t address, const std::uint64_t size, MemoryAttributeOption option)
{
    std::ignore = address;
    std::ignore = size;
    std::ignore = option;
    return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
}
