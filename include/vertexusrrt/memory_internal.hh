//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32) || defined(_WIN64)
#include <windows.h>
#include <tlhelp32.h>
#else
#include <charconv>
#include <format>
#include <fstream>
#endif

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

#if defined(_WIN32) || defined(_WIN64)
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
#else
    inline std::array<MemoryAttributeOption, g_memoryAttributeOptionsSize> g_memoryProtectionOptions = {
      {{"PROT_READ (r--)", set_page_state<ProtectionFlag::STATE_PAGE_READ_ONLY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_ONLY)]},
       {"PROT_READ_WRITE (rw-)", set_page_state<ProtectionFlag::STATE_PAGE_READ_WRITE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_READ_WRITE)]},
       {"PROT_WRITE_PRIVATE (rw-p)", set_page_state<ProtectionFlag::STATE_PAGE_WRITE_COPY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COPY)]},
       {"PROT_READ_EXEC (r-x)", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_READ>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_READ)]},
       {"PROT_READ_WRITE_EXEC (rwx)", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_READ_WRITE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY)]},
       {"PROT_EXEC_PRIVATE (rwxp)", set_page_state<ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_EXECUTE_WRITE_COPY)]},
       {"MAP_SHARED (s)", set_page_state<ProtectionFlag::STATE_PAGE_NO_CACHE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_NO_CACHE)]},
       {"MAP_PRIVATE (p)", set_page_state<ProtectionFlag::STATE_PAGE_WRITE_COMBINE>, VERTEX_PROTECTION, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_PAGE_WRITE_COMBINE)]},
       {"MAPPED", set_page_state<ProtectionFlag::STATE_MEM_COMMIT>, VERTEX_STATE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_COMMIT)]},
       {"FILE_EXEC", set_page_state<ProtectionFlag::STATE_MEM_IMAGE>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_IMAGE)]},
       {"FILE_BACKED", set_page_state<ProtectionFlag::STATE_MEM_MAPPED>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_MAPPED)]},
       {"ANONYMOUS", set_page_state<ProtectionFlag::STATE_MEM_PRIVATE>, VERTEX_TYPE, &g_memoryProtectionFlags[static_cast<std::uint32_t>(ProtectionFlag::STATE_MEM_PRIVATE)]}}};
#endif

    struct ModuleLookup final
    {
        std::vector<std::string> nameStorage{};
        std::vector<std::pair<std::uint64_t, std::size_t>> baseToNameIndex{};

#if defined(_WIN32) || defined(_WIN64)
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
#else
        void build(native_handle pid)
        {
            nameStorage.clear();
            baseToNameIndex.clear();

            if (pid == INVALID_HANDLE_VALUE)
            {
                return;
            }

            const auto mapsPath = std::format("/proc/{}/maps", pid);
            std::ifstream mapsFile{mapsPath};
            if (!mapsFile)
            {
                return;
            }

            std::string line{};
            while (std::getline(mapsFile, line))
            {
                std::string_view sv{line};

                const auto dashPos = sv.find('-');
                if (dashPos == std::string_view::npos)
                {
                    continue;
                }

                std::uint64_t start{};
                std::from_chars(sv.data(), sv.data() + dashPos, start, 16);
                sv.remove_prefix(dashPos + 1);

                const auto spacePos = sv.find(' ');
                if (spacePos == std::string_view::npos)
                {
                    continue;
                }
                sv.remove_prefix(spacePos + 1);

                const auto permsEnd = sv.find(' ');
                if (permsEnd == std::string_view::npos)
                {
                    continue;
                }
                sv.remove_prefix(permsEnd + 1);

                const auto offsetEnd = sv.find(' ');
                if (offsetEnd == std::string_view::npos)
                {
                    continue;
                }

                std::uint64_t offset{};
                std::from_chars(sv.data(), sv.data() + offsetEnd, offset, 16);

                if (offset != 0)
                {
                    continue;
                }

                sv.remove_prefix(offsetEnd + 1);

                for (int i = 0; i < 2; ++i)
                {
                    const auto nextSpace = sv.find(' ');
                    if (nextSpace == std::string_view::npos)
                    {
                        break;
                    }
                    sv.remove_prefix(nextSpace + 1);
                    while (!sv.empty() && sv.front() == ' ')
                    {
                        sv.remove_prefix(1);
                    }
                }

                if (sv.empty() || sv.front() == '[')
                {
                    continue;
                }

                auto moduleName = std::filesystem::path{std::string{sv}}.filename().string();
                baseToNameIndex.emplace_back(start, nameStorage.size());
                nameStorage.push_back(std::move(moduleName));
            }
        }
#endif

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

    extern thread_local ModuleLookup g_moduleLookup;
}
