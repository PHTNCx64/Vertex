//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>
#include <fstream>
#include <string>

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_min_process_address(std::uint64_t* address)
{
    if (!address)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    std::ifstream procFile{"/proc/sys/vm/mmap_min_addr"};
    if (!procFile.is_open())
    {
        *address = 0x10000;
        return StatusCode::STATUS_OK;
    }

    std::string value{};
    std::getline(procFile, value);

    *address = value.empty() ? 0x10000 : std::stoull(value);
    return StatusCode::STATUS_OK;
}
