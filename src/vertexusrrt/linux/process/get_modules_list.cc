//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <algorithm>
#include <charconv>
#include <filesystem>
#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <map>
#include <vector>

namespace
{
    struct ParsedMapsLine final
    {
        std::uint64_t start{};
        std::uint64_t end{};
        std::string path{};
        bool valid{};
    };

    [[nodiscard]] ParsedMapsLine parse_maps_line(std::string_view line)
    {
        ParsedMapsLine result{};

        const auto dashPos = line.find('-');
        if (dashPos == std::string_view::npos)
        {
            return result;
        }

        std::from_chars(line.data(), line.data() + dashPos, result.start, 16);
        line.remove_prefix(dashPos + 1);

        const auto spacePos = line.find(' ');
        if (spacePos == std::string_view::npos)
        {
            return result;
        }

        std::from_chars(line.data(), line.data() + spacePos, result.end, 16);
        line.remove_prefix(spacePos + 1);

        for (int i = 0; i < 4; ++i)
        {
            const auto nextSpace = line.find(' ');
            if (nextSpace == std::string_view::npos)
            {
                return result;
            }
            line.remove_prefix(nextSpace + 1);
            while (!line.empty() && line.front() == ' ')
            {
                line.remove_prefix(1);
            }
        }

        if (line.empty() || line.front() == '[')
        {
            return result;
        }

        result.path = std::string{line};
        result.valid = true;
        return result;
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_modules_list(ModuleInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const native_handle handle = get_native_handle();
        if (handle == -1)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        const uint32_t processId = ProcessInternal::opened_process_info()->processId;
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        const auto mapsPath = std::format("/proc/{}/maps", processId);
        std::ifstream mapsFile{mapsPath};
        if (!mapsFile)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        struct ModuleRange final
        {
            std::uint64_t base{std::numeric_limits<std::uint64_t>::max()};
            std::uint64_t end{};
        };

        std::map<std::string, ModuleRange> moduleMap{};

        std::string line{};
        while (std::getline(mapsFile, line))
        {
            const auto parsed = parse_maps_line(line);
            if (!parsed.valid)
            {
                continue;
            }

            auto& range = moduleMap[parsed.path];
            if (parsed.start < range.base)
            {
                range.base = parsed.start;
            }
            if (parsed.end > range.end)
            {
                range.end = parsed.end;
            }
        }

        std::vector<ModuleInformation> modules{};
        modules.reserve(moduleMap.size());

        for (const auto& [path, range] : moduleMap)
        {
            ModuleInformation info{};
            info.baseAddress = range.base;
            info.size = range.end - range.base;
            ProcessInternal::vertex_cpy(info.moduleName,
                std::filesystem::path{path}.filename().string(), VERTEX_MAX_NAME_LENGTH);
            ProcessInternal::vertex_cpy(info.modulePath, path, VERTEX_MAX_PATH_LENGTH);
            modules.push_back(info);
        }

        const auto actualCount = static_cast<uint32_t>(modules.size());

        if (!list)
        {
            *count = actualCount;
            return StatusCode::STATUS_OK;
        }

        if (!*list || *count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t bufferSize = *count;
        const uint32_t copyCount = std::min(bufferSize, actualCount);

        std::copy_n(modules.begin(), copyCount, *list);
        *count = copyCount;

        if (actualCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }
}
