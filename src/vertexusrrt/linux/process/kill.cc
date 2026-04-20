//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <csignal>

extern native_handle& get_native_handle();

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_kill()
    {
        const native_handle handle = get_native_handle();
        if (handle == -1)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        if (kill(handle, SIGKILL) == 0)
        {
            return StatusCode::STATUS_OK;
        }

        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }
}
