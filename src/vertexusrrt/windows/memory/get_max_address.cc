//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>

#include <Windows.h>

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_max_process_address(std::uint64_t* address)
{
    if (!address)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    SYSTEM_INFO sys_info{};
    GetSystemInfo(&sys_info);
    *address = reinterpret_cast<std::uint64_t>(sys_info.lpMaximumApplicationAddress);

    return STATUS_OK;
}
