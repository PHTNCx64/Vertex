//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/memory_internal.hh>

#include <charconv>
#include <format>
#include <fstream>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern native_handle& get_native_handle();

namespace
{
    struct MapsEntry final
    {
        std::uint64_t start{};
        std::uint64_t end{};
        bool readable{};
        bool writable{};
        bool executable{};
        bool isPrivate{};
        bool isFileBacked{};
        std::uint64_t offset{};
        std::string path{};

        [[nodiscard]] bool same_permissions(const MapsEntry& other) const
        {
            return writable == other.writable
                && executable == other.executable
                && isPrivate == other.isPrivate
                && isFileBacked == other.isFileBacked;
        }
    };

    [[nodiscard]] std::optional<MapsEntry> parse_maps_line(std::string_view sv)
    {
        MapsEntry entry{};

        const auto dashPos = sv.find('-');
        if (dashPos == std::string_view::npos)
        {
            return std::nullopt;
        }

        std::from_chars(sv.data(), sv.data() + dashPos, entry.start, 16);
        sv.remove_prefix(dashPos + 1);

        const auto spacePos = sv.find(' ');
        if (spacePos == std::string_view::npos)
        {
            return std::nullopt;
        }

        std::from_chars(sv.data(), sv.data() + spacePos, entry.end, 16);
        sv.remove_prefix(spacePos + 1);

        if (sv.size() < 4)
        {
            return std::nullopt;
        }

        entry.readable = sv[0] == 'r';
        entry.writable = sv[1] == 'w';
        entry.executable = sv[2] == 'x';
        entry.isPrivate = sv[3] == 'p';

        const auto permsEnd = sv.find(' ');
        if (permsEnd == std::string_view::npos)
        {
            return std::nullopt;
        }
        sv.remove_prefix(permsEnd + 1);

        const auto offsetEnd = sv.find(' ');
        if (offsetEnd == std::string_view::npos)
        {
            return std::nullopt;
        }

        std::from_chars(sv.data(), sv.data() + offsetEnd, entry.offset, 16);
        sv.remove_prefix(offsetEnd + 1);

        for (int i = 0; i < 2; ++i)
        {
            const auto nextSpace = sv.find(' ');
            if (nextSpace == std::string_view::npos)
            {
                return entry;
            }
            sv.remove_prefix(nextSpace + 1);
            while (!sv.empty() && sv.front() == ' ')
            {
                sv.remove_prefix(1);
            }
        }

        if (!sv.empty() && sv.front() != '[')
        {
            entry.isFileBacked = true;
            entry.path = std::string{sv};
        }

        return entry;
    }

    [[nodiscard]] bool matches_protection_filter(const MapsEntry& entry)
    {
        bool matchesFilter{};

        if (entry.readable && !entry.writable && !entry.executable && MemoryInternal::g_memoryProtectionFlags[0])
        {
            matchesFilter = true;
        }
        if (entry.readable && entry.writable && !entry.executable && MemoryInternal::g_memoryProtectionFlags[1])
        {
            matchesFilter = true;
        }
        if (entry.readable && entry.writable && entry.isPrivate && MemoryInternal::g_memoryProtectionFlags[2])
        {
            matchesFilter = true;
        }
        if (entry.readable && !entry.writable && entry.executable && MemoryInternal::g_memoryProtectionFlags[3])
        {
            matchesFilter = true;
        }
        if (entry.readable && entry.writable && entry.executable && MemoryInternal::g_memoryProtectionFlags[4])
        {
            matchesFilter = true;
        }
        if (entry.readable && entry.writable && entry.executable && entry.isPrivate && MemoryInternal::g_memoryProtectionFlags[5])
        {
            matchesFilter = true;
        }
        if (!entry.isPrivate && MemoryInternal::g_memoryProtectionFlags[6])
        {
            matchesFilter = true;
        }
        if (entry.isPrivate && MemoryInternal::g_memoryProtectionFlags[7])
        {
            matchesFilter = true;
        }

        if (MemoryInternal::g_memoryProtectionFlags[8])
        {
            matchesFilter = true;
        }

        if (entry.isFileBacked && entry.executable && MemoryInternal::g_memoryProtectionFlags[9])
        {
            matchesFilter = true;
        }
        if (entry.isFileBacked && !entry.executable && MemoryInternal::g_memoryProtectionFlags[10])
        {
            matchesFilter = true;
        }
        if (!entry.isFileBacked && MemoryInternal::g_memoryProtectionFlags[11])
        {
            matchesFilter = true;
        }

        return matchesFilter;
    }
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, std::uint64_t* size)
{
    if (!regions || !size)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    MemoryInternal::g_moduleLookup.build(nativeHandle);

    const auto mapsPath = std::format("/proc/{}/maps", nativeHandle);
    std::ifstream mapsFile{mapsPath};
    if (!mapsFile)
    {
        *regions = nullptr;
        *size = 0;
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    constexpr std::uint64_t MAX_REGION_SIZE = 32ULL * 1024 * 1024;

    std::vector<MemoryRegion> tempRegions{};
    tempRegions.reserve(2048);

    MemoryRegion* lastRegion{};
    std::optional<MapsEntry> lastEntry{};

    std::string line{};
    while (std::getline(mapsFile, line))
    {
        auto parsed = parse_maps_line(line);
        if (!parsed)
        {
            continue;
        }

        auto& entry = *parsed;

        if (!entry.readable)
        {
            lastEntry = std::nullopt;
            continue;
        }

        if (!matches_protection_filter(entry))
        {
            lastEntry = std::nullopt;
            continue;
        }

        const auto regionSize = entry.end - entry.start;
        const char* moduleName = MemoryInternal::g_moduleLookup.find(entry.start);

        if (!moduleName && entry.isFileBacked)
        {
            moduleName = MemoryInternal::g_moduleLookup.find(entry.start - entry.offset);
        }

        const bool canMerge = lastRegion != nullptr
            && lastEntry.has_value()
            && lastRegion->baseAddress + lastRegion->regionSize == entry.start
            && lastRegion->regionSize + regionSize <= MAX_REGION_SIZE
            && lastRegion->baseModuleName == moduleName
            && lastEntry->same_permissions(entry);

        if (canMerge)
        {
            lastRegion->regionSize += regionSize;
        }
        else
        {
            if (regionSize > MAX_REGION_SIZE)
            {
                std::uint64_t remainingSize = regionSize;
                std::uint64_t currentBase = entry.start;

                while (remainingSize > 0)
                {
                    const std::uint64_t chunkSize = std::min(remainingSize, MAX_REGION_SIZE);
                    tempRegions.push_back({.baseModuleName = moduleName, .baseAddress = currentBase, .regionSize = chunkSize});
                    currentBase += chunkSize;
                    remainingSize -= chunkSize;
                }

                lastRegion = &tempRegions.back();
            }
            else
            {
                tempRegions.push_back({.baseModuleName = moduleName, .baseAddress = entry.start, .regionSize = regionSize});
                lastRegion = &tempRegions.back();
            }
        }

        lastEntry = entry;
    }

    if (tempRegions.empty())
    {
        *regions = nullptr;
        *size = 0;
        return StatusCode::STATUS_OK;
    }

    *size = tempRegions.size();
    *regions = static_cast<MemoryRegion*>(std::malloc(sizeof(MemoryRegion) * tempRegions.size()));
    if (*regions == nullptr)
    {
        return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    std::copy_n(tempRegions.data(), tempRegions.size(), *regions);

    return StatusCode::STATUS_OK;
}
