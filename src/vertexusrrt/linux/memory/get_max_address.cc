//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <cstdint>

extern ProcessArchitecture get_process_architecture();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_max_process_address(std::uint64_t* address)
{
    if (!address)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto arch = get_process_architecture();

    switch (arch)
    {
    case ProcessArchitecture::X86:
        *address = 0xFFFFFFFF;
        break;
    case ProcessArchitecture::X86_64:
    case ProcessArchitecture::ARM64:
    default:
        *address = 0x7FFFFFFFFFFF;
        break;
    }

    return StatusCode::STATUS_OK;
}
