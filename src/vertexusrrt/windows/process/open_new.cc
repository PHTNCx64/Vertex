//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

extern StatusCode vertex_process_open(const uint32_t process_id);

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const char* argv)
    {
        if (!process_path || std::string_view{process_path}.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto proc_path_cpp_str = ProcessInternal::utf8_to_wchar(process_path);

        auto argv_cpp_str = ProcessInternal::utf8_to_wchar(argv);

        if (!proc_path_cpp_str)
        {
            return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
        }

        STARTUPINFO startup_info{};
        startup_info.cb = sizeof(STARTUPINFO);

        PROCESS_INFORMATION process_info{};

        const BOOL result = CreateProcess(proc_path_cpp_str->c_str(), (argv_cpp_str && !argv_cpp_str->empty()) ? argv_cpp_str->data() : nullptr, nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                                          &startup_info, &process_info);
        if (!result)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);

        return vertex_process_open(process_info.dwProcessId);
    }
}
