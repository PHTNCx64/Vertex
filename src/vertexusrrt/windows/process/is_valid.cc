//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <Windows.h>

extern native_handle& get_native_handle();

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_is_valid()
    {
        const native_handle& handle = get_native_handle();

        if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(handle, &exit_code))
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        return exit_code == STILL_ACTIVE ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
    }
}
