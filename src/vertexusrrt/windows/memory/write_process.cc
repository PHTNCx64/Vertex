//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <cstdint>

#include <Windows.h>

extern native_handle& get_native_handle();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process(const std::uint64_t address, const std::uint64_t size, const char* buffer)
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

    SIZE_T numberOfBytesWritten{};
    BOOL status = WriteProcessMemory(nativeHandle, reinterpret_cast<LPVOID>(address), buffer, size, &numberOfBytesWritten);

    if (status && numberOfBytesWritten == size)
    {
        return STATUS_OK;
    }

    DWORD oldProtect = 0;
    if (!VirtualProtectEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
    {
        return STATUS_ERROR_MEMORY_WRITE;
    }

    numberOfBytesWritten = 0;
    status = WriteProcessMemory(nativeHandle, reinterpret_cast<LPVOID>(address), buffer, size, &numberOfBytesWritten);

    DWORD tempProtect = 0;
    VirtualProtectEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, oldProtect, &tempProtect);

    return status && numberOfBytesWritten == size ? STATUS_OK : STATUS_ERROR_MEMORY_WRITE;
}
