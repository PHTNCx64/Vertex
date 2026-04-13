//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <unistd.h>

#include <array>
#include <format>
#include <fstream>
#include <string>
#include <string_view>

extern void cache_process_architecture();

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open(const uint32_t processId)
    {
        const auto procPath = std::format("/proc/{}", processId);

        if (access(procPath.c_str(), F_OK) != 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        native_handle& handle = get_native_handle();
        handle = static_cast<int>(processId);

        const auto linkPath = std::format("/proc/{}/exe", processId);

        std::array<char, VERTEX_MAX_NAME_LENGTH + 1> buf{};
        const ssize_t len = readlink(linkPath.c_str(), buf.data(), VERTEX_MAX_NAME_LENGTH);

        std::string exeName{};
        if (len > 0)
        {
            std::string_view sv{buf.data(), static_cast<std::size_t>(len)};
            if (const auto pos = sv.rfind('/'); pos != std::string_view::npos)
            {
                sv = sv.substr(pos + 1);
            }
            exeName = std::string{sv};
        }

        if (exeName.empty())
        {
            const auto statusPath = std::format("/proc/{}/status", processId);
            std::ifstream statusFile{statusPath};
            if (statusFile)
            {
                std::string line{};
                while (std::getline(statusFile, line))
                {
                    std::string_view sv{line};
                    if (sv.starts_with("Name:"))
                    {
                        sv.remove_prefix(5);
                        while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
                        {
                            sv.remove_prefix(1);
                        }
                        exeName = std::string{sv};
                        break;
                    }
                }
            }
        }

        if (exeName.empty())
        {
            handle = -1;
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        cache_process_architecture();

        ProcessInformation* info = ProcessInternal::opened_process_info();
        info->processId = processId;
        ProcessInternal::vertex_cpy(info->processName, exeName, VERTEX_MAX_NAME_LENGTH);

        return StatusCode::STATUS_OK;
    }
}
