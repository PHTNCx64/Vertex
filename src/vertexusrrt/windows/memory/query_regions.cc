//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/memory_internal.hh>

#include <vector>

extern native_handle& get_native_handle();

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, std::uint64_t* size)
{
    if (!regions || !size)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto& nativeHandle = get_native_handle();
    if (nativeHandle == INVALID_HANDLE_VALUE)
    {
        return STATUS_ERROR_PROCESS_INVALID;
    }

    MemoryInternal::g_moduleLookup.build(nativeHandle);

    SYSTEM_INFO sysInfo{};
    GetSystemInfo(&sysInfo);

    auto currentAddress = reinterpret_cast<std::uint64_t>(sysInfo.lpMinimumApplicationAddress);
    const auto maxAddress = reinterpret_cast<std::uint64_t>(sysInfo.lpMaximumApplicationAddress);

    std::vector<MemoryRegion> tempRegions{};
    tempRegions.reserve(1024);

    MEMORY_BASIC_INFORMATION memInfo{};
    MemoryRegion* lastRegion = nullptr;

    constexpr std::uint64_t MAX_REGION_SIZE = 512ULL * 1024 * 1024;

    while (currentAddress < maxAddress)
    {
        const SIZE_T result = VirtualQueryEx(nativeHandle, reinterpret_cast<LPCVOID>(currentAddress), &memInfo, sizeof(memInfo));
        if (result == 0)
        {
            break;
        }

        if (memInfo.State != MEM_COMMIT)
        {
            currentAddress = reinterpret_cast<std::uint64_t>(memInfo.BaseAddress) + memInfo.RegionSize;
            continue;
        }

        const bool isUnreadable =
            (memInfo.Protect & PAGE_NOACCESS) ||
            (memInfo.Protect & PAGE_GUARD) ||
            (memInfo.Protect == PAGE_EXECUTE);

        if (isUnreadable)
        {
            currentAddress = reinterpret_cast<std::uint64_t>(memInfo.BaseAddress) + memInfo.RegionSize;
            continue;
        }

        bool matchesFilter = false;

        if ((memInfo.Protect & PAGE_READONLY) && MemoryInternal::g_memoryProtectionFlags[0])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_READWRITE) && MemoryInternal::g_memoryProtectionFlags[1])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_WRITECOPY) && MemoryInternal::g_memoryProtectionFlags[2])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_EXECUTE_READ) && MemoryInternal::g_memoryProtectionFlags[3])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_EXECUTE_READWRITE) && MemoryInternal::g_memoryProtectionFlags[4])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_EXECUTE_WRITECOPY) && MemoryInternal::g_memoryProtectionFlags[5])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_NOCACHE) && MemoryInternal::g_memoryProtectionFlags[6])
        {
            matchesFilter = true;
        }
        if ((memInfo.Protect & PAGE_WRITECOMBINE) && MemoryInternal::g_memoryProtectionFlags[7])
        {
            matchesFilter = true;
        }

        if (MemoryInternal::g_memoryProtectionFlags[8])
        {
            matchesFilter = true;
        }

        if (memInfo.Type == MEM_IMAGE && MemoryInternal::g_memoryProtectionFlags[9])
        {
            matchesFilter = true;
        }
        if (memInfo.Type == MEM_MAPPED && MemoryInternal::g_memoryProtectionFlags[10])
        {
            matchesFilter = true;
        }
        if (memInfo.Type == MEM_PRIVATE && MemoryInternal::g_memoryProtectionFlags[11])
        {
            matchesFilter = true;
        }

        if (matchesFilter)
        {
            const auto baseAddr = reinterpret_cast<std::uint64_t>(memInfo.BaseAddress);
            const auto regionSize = static_cast<std::uint64_t>(memInfo.RegionSize);
            const auto allocBase = reinterpret_cast<std::uint64_t>(memInfo.AllocationBase);
            const char* moduleName = MemoryInternal::g_moduleLookup.find(allocBase);

            if (lastRegion != nullptr
                && lastRegion->baseAddress + lastRegion->regionSize == baseAddr
                && lastRegion->regionSize + regionSize <= MAX_REGION_SIZE
                && lastRegion->baseModuleName == moduleName)
            {
                lastRegion->regionSize += regionSize;
            }
            else
            {
                if (regionSize > MAX_REGION_SIZE)
                {
                    std::uint64_t remainingSize = regionSize;
                    std::uint64_t currentBase = baseAddr;

                    while (remainingSize > 0)
                    {
                        const std::uint64_t chunkSize = (remainingSize > MAX_REGION_SIZE) ? MAX_REGION_SIZE : remainingSize;
                        tempRegions.push_back({.baseModuleName = moduleName, .baseAddress = currentBase, .regionSize = chunkSize});
                        currentBase += chunkSize;
                        remainingSize -= chunkSize;
                    }

                    lastRegion = &tempRegions.back();
                }
                else
                {
                    tempRegions.push_back({.baseModuleName = moduleName, .baseAddress = baseAddr, .regionSize = regionSize});
                    lastRegion = &tempRegions.back();
                }
            }
        }

        currentAddress = reinterpret_cast<std::uint64_t>(memInfo.BaseAddress) + memInfo.RegionSize;
    }

    if (tempRegions.empty())
    {
        *regions = nullptr;
        *size = 0;
        return STATUS_OK;
    }

    *size = tempRegions.size();
    *regions = static_cast<MemoryRegion*>(std::malloc(sizeof(MemoryRegion) * tempRegions.size()));
    if (*regions == nullptr)
    {
        return STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    std::memcpy(*regions, tempRegions.data(), sizeof(MemoryRegion) * tempRegions.size());

    return STATUS_OK;
}
