//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <array>
#include <memory>
#include <regex>
#include <string>
#include <vector>

#include <tlhelp32.h>

extern native_handle& get_native_handle();
extern ProcessArchitecture get_process_architecture();

namespace
{
    enum class ProtectionFlag : std::uint32_t
    {
        STATE_PAGE_READ_ONLY = 0,
        STATE_PAGE_READ_WRITE,
        STATE_PAGE_WRITE_COPY,
        STATE_PAGE_EXECUTE_READ,
        STATE_PAGE_EXECUTE_READ_WRITE,
        STATE_PAGE_EXECUTE_WRITE_COPY,
        STATE_PAGE_NO_CACHE,
        STATE_PAGE_WRITE_COMBINE,
        STATE_MEM_COMMIT,
        STATE_MEM_IMAGE,
        STATE_MEM_MAPPED,
        STATE_MEM_PRIVATE
    };

    constexpr std::size_t g_memoryAttributeOptionsSize = 12;

    inline std::array<std::uint8_t, g_memoryAttributeOptionsSize> g_memoryProtectionFlags{};

    template <ProtectionFlag flag>
    void set_page_state(const std::uint8_t state)
    {
        g_memoryProtectionFlags[static_cast<std::uint32_t>(flag)] = state;
    }

    inline std::array<MemoryAttributeOption, g_memoryAttributeOptionsSize> g_memoryProtectionOptions = {
      {{"PAGE_READONLY", set_page_state<ProtectionFlag::STATE_PAGE_READ_ONLY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_ONLY)]},
       {"PAGE_READWRITE", set_page_state<ProtectionFlag::STATE_PAGE_READ_WRITE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_WRITE)]},
       {"PAGE_WRITECOPY", set_page_state<ProtectionFlag::STATE_PAGE_WRITE_COPY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COPY)]},
       {"PAGE_EXECUTE_READ", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_READ>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_READ)]},
       {"PAGE_EXECUTE_READWRITE", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_READ_WRITE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_READ_WRITE)]},
       {"PAGE_EXECUTE_WRITECOPY", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY)]},
       {"PAGE_NOCACHE", set_page_state<ProtectionFlag::STATE_PAGE_NO_CACHE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_NO_CACHE)]},
       {"PAGE_WRITECOMBINE", set_page_state<ProtectionFlag::STATE_PAGE_WRITE_COMBINE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COMBINE)]},
       {"MEM_COMMIT", set_page_state<ProtectionFlag::STATE_MEM_COMMIT>, VERTEX_STATE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_COMMIT)]},
       {"MEM_IMAGE", set_page_state<ProtectionFlag::STATE_MEM_IMAGE>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_IMAGE)]},
       {"MEM_MAPPED", set_page_state<ProtectionFlag::STATE_MEM_MAPPED>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_MAPPED)]},
       {"MEM_PRIVATE", set_page_state<ProtectionFlag::STATE_MEM_PRIVATE>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_PRIVATE)]}}};

    struct ModuleLookup final
    {
        std::vector<std::string> nameStorage{};
        std::vector<std::pair<std::uint64_t, std::size_t>> baseToNameIndex{};

        void build(HANDLE processHandle)
        {
            nameStorage.clear();
            baseToNameIndex.clear();

            const DWORD processId = GetProcessId(processHandle);
            if (processId == 0)
            {
                return;
            }

            HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
            if (snapshot == INVALID_HANDLE_VALUE)
            {
                return;
            }

            MODULEENTRY32W me{};
            me.dwSize = sizeof(MODULEENTRY32W);

            if (Module32FirstW(snapshot, &me))
            {
                do
                {
                    const auto base = reinterpret_cast<std::uint64_t>(me.modBaseAddr);

                    int len = WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, nullptr, 0, nullptr, nullptr);
                    if (len > 0)
                    {
                        std::string name(len - 1, '\0');
                        WideCharToMultiByte(CP_UTF8, 0, me.szModule, -1, name.data(), len, nullptr, nullptr);
                        baseToNameIndex.emplace_back(base, nameStorage.size());
                        nameStorage.push_back(std::move(name));
                    }

                    me.dwSize = sizeof(MODULEENTRY32W);
                } while (Module32NextW(snapshot, &me));
            }

            CloseHandle(snapshot);
        }

        [[nodiscard]] const char* find(const std::uint64_t allocationBase) const
        {
            for (const auto& [base, index] : baseToNameIndex)
            {
                if (base == allocationBase)
                {
                    return nameStorage[index].c_str();
                }
            }
            return nullptr;
        }
    };

    inline ModuleLookup g_moduleLookup{};
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_read_process(const std::uint64_t address, const std::uint64_t size, char* buffer)
    {
        if (!buffer || size == 0)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto& nativeHandle = get_native_handle();
        if (nativeHandle == INVALID_HANDLE_VALUE)
        {
            return STATUS_ERROR_PROCESS_INVALID;
        }

        SIZE_T numberOfBytesRead{};
        const BOOL status = ReadProcessMemory(nativeHandle, reinterpret_cast<LPCVOID>(address), buffer, size, &numberOfBytesRead);
        return status && numberOfBytesRead == size ? STATUS_OK : STATUS_ERROR_MEMORY_READ;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_write_process(const std::uint64_t address, const std::uint64_t size, const char* buffer)
    {
        if (!buffer || size == 0)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto& nativeHandle = get_native_handle();
        if (nativeHandle == INVALID_HANDLE_VALUE)
        {
            return STATUS_ERROR_PROCESS_INVALID;
        }

        SIZE_T numberOfBytesWritten{};
        BOOL status = WriteProcessMemory(nativeHandle, reinterpret_cast<LPVOID>(address), buffer, size, &numberOfBytesWritten);

        if (status && numberOfBytesWritten == size)
        {
            return STATUS_OK;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtectEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return STATUS_ERROR_MEMORY_WRITE;
        }

        numberOfBytesWritten = 0;
        status = WriteProcessMemory(nativeHandle, reinterpret_cast<LPVOID>(address), buffer, size, &numberOfBytesWritten);

        DWORD tempProtect = 0;
        VirtualProtectEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, oldProtect, &tempProtect);

        return status && numberOfBytesWritten == size ? STATUS_OK : STATUS_ERROR_MEMORY_WRITE;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_allocate(const std::uint64_t address, const std::uint64_t size, const MemoryAttributeOption** protection, const std::size_t attributeSize, std::uint64_t* targetAddress)
    {
        if (!targetAddress)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        DWORD allocType{};

        const auto& nativeHandle = get_native_handle();
        if (nativeHandle == INVALID_HANDLE_VALUE)
        {
            return STATUS_ERROR_PROCESS_INVALID;
        }

        LPVOID target = VirtualAllocEx(nativeHandle, reinterpret_cast<LPVOID>(address), size, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE);
        *targetAddress = reinterpret_cast<std::uint64_t>(target);

        return STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_query_regions(MemoryRegion** regions, std::uint64_t* size)
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

        g_moduleLookup.build(nativeHandle);

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

            if ((memInfo.Protect & PAGE_READONLY) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_ONLY)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_READWRITE) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_WRITE)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_WRITECOPY) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COPY)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_EXECUTE_READ) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_READ)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_EXECUTE_READWRITE) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_READ_WRITE)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_EXECUTE_WRITECOPY) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_NOCACHE) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_NO_CACHE)])
            {
                matchesFilter = true;
            }
            if ((memInfo.Protect & PAGE_WRITECOMBINE) && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COMBINE)])
            {
                matchesFilter = true;
            }

            if (g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_COMMIT)])
            {
                matchesFilter = true;
            }

            if (memInfo.Type == MEM_IMAGE && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_IMAGE)])
            {
                matchesFilter = true;
            }
            if (memInfo.Type == MEM_MAPPED && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_MAPPED)])
            {
                matchesFilter = true;
            }
            if (memInfo.Type == MEM_PRIVATE && g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_PRIVATE)])
            {
                matchesFilter = true;
            }

            if (matchesFilter)
            {
                const auto baseAddr = reinterpret_cast<std::uint64_t>(memInfo.BaseAddress);
                const auto regionSize = static_cast<std::uint64_t>(memInfo.RegionSize);
                const auto allocBase = reinterpret_cast<std::uint64_t>(memInfo.AllocationBase);
                const char* moduleName = g_moduleLookup.find(allocBase);

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

    VERTEX_EXPORT StatusCode vertex_memory_get_process_pointer_size(std::uint64_t* size)
    {
        const ProcessArchitecture arch =  get_process_architecture();
        if (arch == ProcessArchitecture::X86)
        {
            *size = sizeof(std::uint32_t);
            return StatusCode::STATUS_OK;
        }
        if (arch == ProcessArchitecture::X86_64 || arch == ProcessArchitecture::ARM64)
        {
            *size = sizeof(std::uint64_t);
            return StatusCode::STATUS_OK;
        }
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_construct_attribute_filters(MemoryAttributeOption** options, std::uint32_t* count)
    {
        if (!options || !count)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        *count = g_memoryAttributeOptionsSize;
        *options = g_memoryProtectionOptions.data();

        return STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_free(const std::uint64_t address, const std::uint64_t size)
    {
        if (!address || !size)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        BOOL status = VirtualFreeEx(get_native_handle(), reinterpret_cast<LPVOID>(address), size, MEM_RELEASE);
        return status ? STATUS_OK : STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_min_process_address(std::uint64_t* address)
    {
        if (!address)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        SYSTEM_INFO sys_info{};
        GetSystemInfo(&sys_info);
        *address = reinterpret_cast<std::uint64_t>(sys_info.lpMinimumApplicationAddress);

        return STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_get_max_process_address(std::uint64_t* address)
    {
        if (!address)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        SYSTEM_INFO sys_info{};
        GetSystemInfo(&sys_info);
        *address = reinterpret_cast<std::uint64_t>(sys_info.lpMaximumApplicationAddress);

        return STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_change_protection(const std::uint64_t address, const std::uint64_t size, MemoryAttributeOption option)
    {
        (void)address;
        (void)size;
        (void)option;
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }
}
