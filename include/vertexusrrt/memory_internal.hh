//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <array>
#include <memory>
#include <string>
#include <vector>

#include <Windows.h>
#include <tlhelp32.h>

namespace MemoryInternal
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
