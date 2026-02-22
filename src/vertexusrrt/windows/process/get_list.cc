//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <tlhelp32.h>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        std::vector<ProcessInformation> processes {};

        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &pe32))
        {
            do
            {
                ProcessInformation info{};

                info.processId = pe32.th32ProcessID;
                info.parentProcessId = pe32.th32ParentProcessID;

                const auto procNameOpt = ProcessInternal::wchar_to_utf8(pe32.szExeFile);
                if (procNameOpt && !procNameOpt->empty())
                {
                    ProcessInternal::vertex_cpy(info.processName, *procNameOpt, VERTEX_MAX_NAME_LENGTH);
                }
                else
                {
                    ProcessInternal::vertex_cpy(info.processName, "Unknown Process", VERTEX_MAX_NAME_LENGTH);
                }

                ProcessInternal::vertex_cpy(info.processOwner, "N/A", VERTEX_MAX_NAME_LENGTH);

                processes.push_back(info);

            } while (Process32NextW(snapshot, &pe32));
        }

        CloseHandle(snapshot);

        *count = static_cast<uint32_t>(processes.size());
        if (!list)
        {
            return StatusCode::STATUS_OK;
        }

        if (!*list)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t bufferSize = *count;
        const std::uint32_t actualCount = processes.size();

        if (bufferSize == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t copyCount = std::min(bufferSize, actualCount);

        ProcessInformation* buffer = *list;
        std::copy_n(processes.begin(), copyCount, buffer);

        *count = copyCount;

        if (actualCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }
}
