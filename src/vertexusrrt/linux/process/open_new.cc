//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <spawn.h>
#include <unistd.h>

#include <string>
#include <string_view>
#include <vector>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const int argc, const char** argv)
    {
        if (!process_path || std::string_view{process_path}.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::string pathStr{process_path};

        std::vector<std::string> argStrings{};
        argStrings.push_back(pathStr);

        if (argv)
        {
            for (int i = 0; i < argc; ++i)
            {
                if (argv[i])
                {
                    argStrings.emplace_back(argv[i]);
                }
            }
        }

        std::vector<char*> argPtrs{};
        argPtrs.reserve(argStrings.size() + 1);
        for (auto& s : argStrings)
        {
            argPtrs.push_back(s.data());
        }
        argPtrs.push_back(nullptr);

        posix_spawn_file_actions_t fileActions{};
        posix_spawnattr_t spawnAttr{};
        posix_spawn_file_actions_init(&fileActions);
        posix_spawnattr_init(&spawnAttr);

        pid_t pid{};
        const int result = posix_spawn(&pid, pathStr.c_str(), &fileActions, &spawnAttr,
            argPtrs.data(), environ);

        posix_spawn_file_actions_destroy(&fileActions);
        posix_spawnattr_destroy(&spawnAttr);

        if (result != 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        return vertex_process_open(static_cast<uint32_t>(pid));
    }
}
