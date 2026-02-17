//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

extern void cache_process_architecture();
extern StatusCode vertex_process_close();

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open(const uint32_t processId)
    {
        native_handle& handle = get_native_handle();
        handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, processId);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        WCHAR full_proc_path[MAX_PATH];
        DWORD size = MAX_PATH;

        if (!QueryFullProcessImageName(handle, 0, full_proc_path, &size))
        {
            vertex_process_close();
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        const WCHAR* proc_name = wcsrchr(full_proc_path, L'\\');
        if (proc_name)
        {
            ++proc_name;
        }
        else
        {
            proc_name = full_proc_path;
        }

        cache_process_architecture();

        ProcessInformation* info = ProcessInternal::opened_process_info();

        info->processId = processId;
        const auto proc_name_cpp_str = ProcessInternal::wchar_to_utf8(proc_name);
        if (!proc_name_cpp_str)
        {
            vertex_process_close();
            return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
        }
        ProcessInternal::vertex_cpy(info->processName, *proc_name_cpp_str, VERTEX_MAX_NAME_LENGTH);

        return StatusCode::STATUS_OK;
    }
}
