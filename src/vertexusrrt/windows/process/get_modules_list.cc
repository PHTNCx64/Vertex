//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>
#include <sdk/process.h>

#include <Windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern native_handle& get_native_handle();

namespace ProcessInternal
{
    ProcessInformation* opened_process_info();
    std::optional<std::string> wchar_to_utf8(const WCHAR* str) noexcept;
    void vertex_cpy(char* dst, std::string_view src, std::size_t max_len);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_modules_list(ModuleInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const native_handle& processHandle = get_native_handle();
        if (processHandle == INVALID_HANDLE_VALUE || processHandle == nullptr)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        DWORD processId = ProcessInternal::opened_process_info()->processId;
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        std::vector<ModuleInformation> modules{};
        MODULEENTRY32W moduleEntry{};
        moduleEntry.dwSize = sizeof(MODULEENTRY32W);

        if (Module32FirstW(snapshot, &moduleEntry))
        {
            do
            {
                ModuleInformation info{};

                if (moduleEntry.szModule[0] != L'\0')
                {
                    auto nameOpt = ProcessInternal::wchar_to_utf8(moduleEntry.szModule);
                    if (nameOpt && !nameOpt->empty())
                    {
                        ProcessInternal::vertex_cpy(info.moduleName, *nameOpt, VERTEX_MAX_NAME_LENGTH);
                    }
                    else
                    {
                        ProcessInternal::vertex_cpy(info.moduleName, "Unknown", VERTEX_MAX_NAME_LENGTH);
                    }
                }
                else
                {
                    ProcessInternal::vertex_cpy(info.moduleName, "Unknown", VERTEX_MAX_NAME_LENGTH);
                }

                if (moduleEntry.szExePath[0] != L'\0')
                {
                    auto pathOpt = ProcessInternal::wchar_to_utf8(moduleEntry.szExePath);
                    if (pathOpt && !pathOpt->empty())
                    {
                        ProcessInternal::vertex_cpy(info.modulePath, *pathOpt, VERTEX_MAX_PATH_LENGTH);
                    }
                    else
                    {
                        info.modulePath[0] = '\0';
                    }
                }
                else
                {
                    info.modulePath[0] = '\0';
                }

                info.baseAddress = reinterpret_cast<uint64_t>(moduleEntry.modBaseAddr);
                info.size = static_cast<uint64_t>(moduleEntry.modBaseSize);

                modules.push_back(info);

                moduleEntry.dwSize = sizeof(MODULEENTRY32W);

            } while (Module32NextW(snapshot, &moduleEntry));
        }

        CloseHandle(snapshot);

        if (!list)
        {
            *count = static_cast<uint32_t>(modules.size());
            return StatusCode::STATUS_OK;
        }

        if (!*list)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (*count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t bufferSize = *count;
        const auto actualCount = static_cast<uint32_t>(modules.size());
        const uint32_t copyCount = std::min(bufferSize, actualCount);

        ModuleInformation* buffer = *list;
        std::copy_n(modules.begin(), copyCount, buffer);

        *count = copyCount;

        if (actualCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }
}
