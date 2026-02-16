//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <Windows.h>
#include <cstdint>

extern native_handle& get_native_handle();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_free(const std::uint64_t address, const std::uint64_t size)
{
    if (!address || !size)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    BOOL status = VirtualFreeEx(get_native_handle(), reinterpret_cast<LPVOID>(address), 0, MEM_RELEASE);
    return status ? STATUS_OK : STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
}
