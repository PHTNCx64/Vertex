//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <tlhelp32.h>

#include <algorithm>
#include <vector>

extern void clear_process_architecture();

native_handle& get_native_handle()
{
    static native_handle handle;
    return handle;
}

namespace ProcessInternal
{
    ModuleCache& get_module_cache()
    {
        static ModuleCache cache{};
        return cache;
    }

    ProcessInformation* opened_process_info()
    {
        static ProcessInformation info{};
        return &info;
    }

    StatusCode invalidate_handle()
    {
        native_handle& handle = get_native_handle();
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;

        clear_process_architecture();

        opened_process_info()->processId = 0;
        std::fill_n(opened_process_info()->processName, VERTEX_MAX_NAME_LENGTH, 0);
        std::fill_n(opened_process_info()->processOwner, VERTEX_MAX_OWNER_LENGTH, 0);

        return StatusCode::STATUS_OK;
    }
}

namespace ProcessInternal
{
    static bool build_section_cache_for_module(std::uint64_t moduleBase, ModuleSectionCache& out)
    {
        IMAGE_DOS_HEADER dosHeader{};
        if (!read_remote(moduleBase, dosHeader))
        {
            return false;
        }

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return false;
        }

        std::uint32_t signature{};
        if (!read_remote(moduleBase + dosHeader.e_lfanew, signature))
        {
            return false;
        }

        if (signature != IMAGE_NT_SIGNATURE)
        {
            return false;
        }

        IMAGE_FILE_HEADER fileHeader{};
        if (!read_remote(moduleBase + dosHeader.e_lfanew + sizeof(DWORD), fileHeader))
        {
            return false;
        }

        if (fileHeader.NumberOfSections == 0 || fileHeader.NumberOfSections > 96)
        {
            return false;
        }

        const auto sectionTableOffset = static_cast<std::uint64_t>(dosHeader.e_lfanew) +
            sizeof(DWORD) + sizeof(IMAGE_FILE_HEADER) + fileHeader.SizeOfOptionalHeader;

        std::vector<IMAGE_SECTION_HEADER> sectionHeaders(fileHeader.NumberOfSections);
        if (!read_remote_buffer(moduleBase + sectionTableOffset, sectionHeaders.data(),
            fileHeader.NumberOfSections * sizeof(IMAGE_SECTION_HEADER)))
        {
            return false;
        }

        out.sections.reserve(fileHeader.NumberOfSections);

        for (const auto& sec : sectionHeaders)
        {
            SectionEntry entry{};
            std::copy_n(reinterpret_cast<const char*>(sec.Name),
                IMAGE_SIZEOF_SHORT_NAME, entry.name);
            entry.name[IMAGE_SIZEOF_SHORT_NAME] = '\0';
            entry.virtualAddress = sec.VirtualAddress;
            entry.virtualSize = std::max(sec.Misc.VirtualSize, sec.SizeOfRawData);
            out.sections.push_back(entry);
        }

        return true;
    }

    std::optional<ResolvedModule> resolve_module_sections(const std::uint64_t address)
    {
        const DWORD processId = opened_process_info()->processId;
        if (processId == 0)
        {
            return std::nullopt;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return std::nullopt;
        }

        std::uint64_t moduleBase{};
        MODULEENTRY32W moduleEntry{};
        moduleEntry.dwSize = sizeof(MODULEENTRY32W);

        if (Module32FirstW(snapshot, &moduleEntry))
        {
            do
            {
                const auto base = reinterpret_cast<std::uint64_t>(moduleEntry.modBaseAddr);
                const auto size = static_cast<std::uint64_t>(moduleEntry.modBaseSize);

                if (address >= base && address < base + size)
                {
                    moduleBase = base;
                    break;
                }

                moduleEntry.dwSize = sizeof(MODULEENTRY32W);
            } while (Module32NextW(snapshot, &moduleEntry));
        }

        CloseHandle(snapshot);

        if (moduleBase == 0)
        {
            return std::nullopt;
        }

        ModuleCache& cache = get_module_cache();
        std::scoped_lock lock{cache.cacheMutex};

        auto it = cache.sectionCache.find(moduleBase);
        if (it == cache.sectionCache.end())
        {
            ModuleSectionCache newCache{};
            if (!build_section_cache_for_module(moduleBase, newCache))
            {
                cache.sectionCache.try_emplace(moduleBase, ModuleSectionCache{});
                return std::nullopt;
            }
            auto [iter, ok] = cache.sectionCache.try_emplace(moduleBase, std::move(newCache));
            it = iter;
        }

        if (it->second.sections.empty())
        {
            return std::nullopt;
        }

        return ResolvedModule{.baseAddress = moduleBase, .sections = it->second.sections};
    }

    const char* find_section_for_rva(const std::vector<SectionEntry>& sections, const std::uint64_t rva)
    {
        for (const auto& [name, virtualAddress, virtualSize] : sections)
        {
            if (rva >= virtualAddress && rva < virtualAddress + virtualSize)
            {
                return name;
            }
        }
        return nullptr;
    }
}

extern "C"
{
    void clear_module_cache()
    {
        auto& [importCache, exportCache, sectionCache, cacheMutex] = ProcessInternal::get_module_cache();
        std::scoped_lock lock{cacheMutex};
        importCache.clear();
        exportCache.clear();
        sectionCache.clear();
    }
}
