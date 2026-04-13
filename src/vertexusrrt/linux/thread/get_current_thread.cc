//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>

extern std::uint32_t get_current_debug_thread_id();

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_current_thread(uint32_t* threadId)
    {
        if (threadId == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t currentId = get_current_debug_thread_id();
        if (currentId == 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        *threadId = currentId;
        return StatusCode::STATUS_OK;
    }
}
