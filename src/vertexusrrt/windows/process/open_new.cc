//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

namespace
{
    std::wstring quote_command_line_argument(const std::wstring& argument)
    {
        if (argument.empty())
        {
            return L"\"\"";
        }

        if (argument.find_first_of(L" \t\n\v\"") == std::wstring::npos)
        {
            return argument;
        }

        std::wstring quoted{};
        quoted.reserve(argument.size() + 2);
        quoted.push_back(L'"');

        std::size_t consecutiveBackslashes = 0;
        for (const wchar_t ch : argument)
        {
            if (ch == L'\\')
            {
                ++consecutiveBackslashes;
                continue;
            }

            if (ch == L'"')
            {
                quoted.append((consecutiveBackslashes * 2) + 1, L'\\');
                quoted.push_back(L'"');
                consecutiveBackslashes = 0;
                continue;
            }

            quoted.append(consecutiveBackslashes, L'\\');
            consecutiveBackslashes = 0;
            quoted.push_back(ch);
        }

        quoted.append(consecutiveBackslashes * 2, L'\\');
        quoted.push_back(L'"');

        return quoted;
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const int argc, const char** argv)
    {
        if (!process_path || std::string_view{process_path}.empty() || argc < 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto procPathCppStr = ProcessInternal::utf8_to_wchar(process_path);

        if (!procPathCppStr)
        {
            return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
        }

        std::wstring commandLine = quote_command_line_argument(*procPathCppStr);

        if (argv)
        {
            for (int i = 0; i < argc; ++i)
            {
                if (!argv[i])
                {
                    continue;
                }

                const auto arg = ProcessInternal::utf8_to_wchar(argv[i]);
                if (!arg)
                {
                    return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
                }

                commandLine.push_back(L' ');
                commandLine += quote_command_line_argument(*arg);
            }
        }

        STARTUPINFOW startup_info{};
        startup_info.cb = sizeof(STARTUPINFOW);

        PROCESS_INFORMATION process_info{};

        const BOOL result = CreateProcessW(procPathCppStr->c_str(), commandLine.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr,
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
