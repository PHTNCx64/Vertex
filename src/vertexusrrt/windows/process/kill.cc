//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <Windows.h>

extern native_handle& get_native_handle();

namespace ProcessInternal
{
    StatusCode invalidate_handle();
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_kill()
    {
        const native_handle& handle = get_native_handle();
        if (handle == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        if (TerminateProcess(handle, STATUS_OK))
        {
            return ProcessInternal::invalidate_handle();
        }
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }
}
