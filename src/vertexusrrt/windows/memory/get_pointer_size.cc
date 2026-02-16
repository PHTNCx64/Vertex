//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <cstdint>

extern ProcessArchitecture get_process_architecture();

extern "C" VERTEX_EXPORT StatusCode vertex_memory_get_process_pointer_size(std::uint64_t* size)
{
    const auto arch = get_process_architecture();
    if (arch == ProcessArchitecture::X86)
    {
        *size = sizeof(std::uint32_t);
        return StatusCode::STATUS_OK;
    }
    if (arch == ProcessArchitecture::X86_64 || arch == ProcessArchitecture::ARM64)
    {
        *size = sizeof(std::uint64_t);
        return StatusCode::STATUS_OK;
    }
    return StatusCode::STATUS_ERROR_PROCESS_INVALID;
}
