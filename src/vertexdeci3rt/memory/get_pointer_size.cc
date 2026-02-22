//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>

extern "C" VERTEX_EXPORT StatusCode vertex_memory_get_process_pointer_size(std::uint64_t* size)
{
    // Processes normally have a pointer size of 4.
    // However, the LV2 kernel has a pointer size of 8, LV1 too?

    // For now, we just return 4 since we right now will only deal with regular processes
    // But for later stages when we add support for the LV1 HV and LV2 Kernel, we should implement this properly.
    *size = 4;

    return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
}
