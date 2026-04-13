//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <tuple>
#include <sdk/api.h>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_executable_extensions(
        char** extensions, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        *count = 0;
        std::ignore = extensions;

        return StatusCode::STATUS_OK;
    }
}
