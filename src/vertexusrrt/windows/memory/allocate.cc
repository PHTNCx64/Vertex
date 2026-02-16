//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <cstdint>
#include <tuple>

#include <Windows.h>

extern native_handle& get_native_handle();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_allocate(const std::uint64_t address, const std::uint64_t size, const MemoryAttributeOption** protection, const std::size_t attributeSize, std::uint64_t* targetAddress)
{
    if (!targetAddress)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    std::ignore = protection;
    std::ignore = attributeSize;

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return STATUS_ERROR_PROCESS_INVALID;
    }

    LPVOID target = VirtualAllocEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
    *targetAddress = reinterpret_cast<std::uint64_t>(target);

    return target ? STATUS_OK : STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
}
