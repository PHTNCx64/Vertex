//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <cstdint>

#include <Windows.h>

extern native_handle& get_native_handle();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_read_process(const std::uint64_t address, const std::uint64_t size, char* buffer)
{
    if (!buffer || size == 0)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return STATUS_ERROR_PROCESS_INVALID;
    }

    SIZE_T numberOfBytesRead{};
    const BOOL status = ReadProcessMemory(nativeHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &numberOfBytesRead);
    return status && numberOfBytesRead == size ? STATUS_OK : STATUS_ERROR_MEMORY_READ;
}
