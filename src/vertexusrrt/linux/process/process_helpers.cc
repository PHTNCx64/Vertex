//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <elf.h>

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

extern void clear_process_architecture();

bool detect_wine_process(const std::uint32_t pid)
{
    const auto environPath = std::format("/proc/{}/environ", pid);
    std::ifstream environFile{environPath, std::ios::binary};
    if (!environFile)
    {
        return false;
    }

    static constexpr std::array WINE_ENV_MARKERS = {
        std::string_view{"WINELOADER="},
        std::string_view{"WINEPREFIX="},
        std::string_view{"WINELOADERNOEXEC="},
        std::string_view{"STEAM_COMPAT_DATA_PATH="},
        std::string_view{"PROTON_VERSION="},
    };

    std::string environ{};
    environ.assign(std::istreambuf_iterator<char>{environFile}, std::istreambuf_iterator<char>{});

    std::string_view remaining{environ};
    while (!remaining.empty())
    {
        const auto nullPos = remaining.find('\0');
        const auto entry = remaining.substr(0, nullPos);

        for (const auto marker : WINE_ENV_MARKERS)
        {
            if (entry.starts_with(marker))
            {
                return true;
            }
        }

        if (nullPos == std::string_view::npos)
        {
            break;
        }
        remaining.remove_prefix(nullPos + 1);
    }

    return false;
}

native_handle& get_native_handle()
{
    static native_handle handle{-1};
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
        get_native_handle() = -1;

        clear_process_architecture();

        ProcessInformation* info = opened_process_info();
        info->processId = 0;
        std::fill_n(info->processName, VERTEX_MAX_NAME_LENGTH, '\0');
        std::fill_n(info->processOwner, VERTEX_MAX_OWNER_LENGTH, '\0');

        return StatusCode::STATUS_OK;
    }
}

namespace ProcessInternal
{
    namespace
    {
        struct MapEntry final
        {
            std::uint64_t base{};
            std::uint64_t end{};
            std::string path{};
        };

        [[nodiscard]] std::vector<MapEntry> read_proc_maps(const std::uint32_t pid)
        {
            const auto mapsPath = std::format("/proc/{}/maps", pid);
            std::ifstream mapsFile{mapsPath};
            if (!mapsFile)
            {
                return {};
            }

            std::vector<MapEntry> entries{};
            std::string line{};

            while (std::getline(mapsFile, line))
            {
                std::string_view sv{line};

                const auto dashPos = sv.find('-');
                if (dashPos == std::string_view::npos)
                {
                    continue;
                }

                std::uint64_t start{}, end{};
                std::from_chars(sv.data(), sv.data() + dashPos, start, 16);
                sv.remove_prefix(dashPos + 1);

                const auto spacePos = sv.find(' ');
                if (spacePos == std::string_view::npos)
                {
                    continue;
                }

                std::from_chars(sv.data(), sv.data() + spacePos, end, 16);
                sv.remove_prefix(spacePos + 1);

                for (int i = 0; i < 4; ++i)
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

                entries.push_back({start, end, std::string{sv}});
            }

            return entries;
        }

        template <typename EhdrT, typename ShdrT>
        [[nodiscard]] bool build_elf_section_cache(std::ifstream& file, ModuleSectionCache& out)
        {
            EhdrT ehdr{};
            file.seekg(0);
            file.read(reinterpret_cast<char*>(&ehdr), sizeof(ehdr));
            if (!file || ehdr.e_shnum == 0 || ehdr.e_shstrndx == SHN_UNDEF)
            {
                return false;
            }

            file.seekg(static_cast<std::streamoff>(ehdr.e_shoff) +
                       static_cast<std::streamoff>(ehdr.e_shstrndx) * sizeof(ShdrT));
            ShdrT shstrtabHdr{};
            file.read(reinterpret_cast<char*>(&shstrtabHdr), sizeof(shstrtabHdr));
            if (!file || shstrtabHdr.sh_size == 0)
            {
                return false;
            }

            std::vector<char> shstrtab(shstrtabHdr.sh_size);
            file.seekg(static_cast<std::streamoff>(shstrtabHdr.sh_offset));
            file.read(shstrtab.data(), static_cast<std::streamsize>(shstrtabHdr.sh_size));
            if (!file)
            {
                return false;
            }

            std::vector<ShdrT> shdrs(ehdr.e_shnum);
            file.seekg(static_cast<std::streamoff>(ehdr.e_shoff));
            file.read(reinterpret_cast<char*>(shdrs.data()),
                static_cast<std::streamsize>(ehdr.e_shnum * sizeof(ShdrT)));
            if (!file)
            {
                return false;
            }

            out.sections.reserve(ehdr.e_shnum);
            for (const auto& shdr : shdrs)
            {
                if (shdr.sh_addr == 0 && shdr.sh_size == 0)
                {
                    continue;
                }

                SectionEntry entry{};
                if (shdr.sh_name < shstrtab.size())
                {
                    const std::string_view name{&shstrtab[shdr.sh_name]};
                    const auto copyLen = std::min(name.size(), PROCESS_INTERNAL_SECTION_NAME_MAX);
                    std::copy_n(name.data(), copyLen, entry.name);
                    entry.name[copyLen] = '\0';
                }
                entry.virtualAddress = static_cast<std::uint64_t>(shdr.sh_addr);
                entry.virtualSize = static_cast<std::uint64_t>(shdr.sh_size);
                out.sections.push_back(entry);
            }

            return !out.sections.empty();
        }

        [[nodiscard]] bool build_section_cache_for_module(const std::string& filePath, ModuleSectionCache& out)
        {
            std::ifstream file{filePath, std::ios::binary};
            if (!file)
            {
                return false;
            }

            std::array<std::uint8_t, EI_NIDENT> ident{};
            file.read(reinterpret_cast<char*>(ident.data()), EI_NIDENT);
            if (!file || ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
                ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
            {
                return false;
            }

            if (ident[EI_CLASS] == ELFCLASS64)
            {
                return build_elf_section_cache<Elf64_Ehdr, Elf64_Shdr>(file, out);
            }
            if (ident[EI_CLASS] == ELFCLASS32)
            {
                return build_elf_section_cache<Elf32_Ehdr, Elf32_Shdr>(file, out);
            }
            return false;
        }
    }

    std::optional<ResolvedModule> resolve_module_sections(const std::uint64_t address)
    {
        const std::uint32_t processId = opened_process_info()->processId;
        if (processId == 0)
        {
            return std::nullopt;
        }

        const auto mapEntries = read_proc_maps(processId);

        std::string modulePath{};
        std::uint64_t moduleBase{std::numeric_limits<std::uint64_t>::max()};

        for (const auto& entry : mapEntries)
        {
            if (address >= entry.base && address < entry.end)
            {
                modulePath = entry.path;
                break;
            }
        }

        if (modulePath.empty())
        {
            return std::nullopt;
        }

        for (const auto& entry : mapEntries)
        {
            if (entry.path == modulePath && entry.base < moduleBase)
            {
                moduleBase = entry.base;
            }
        }

        if (moduleBase == std::numeric_limits<std::uint64_t>::max())
        {
            return std::nullopt;
        }

        ModuleCache& cache = get_module_cache();
        std::scoped_lock lock{cache.cacheMutex};

        auto it = cache.sectionCache.find(moduleBase);
        if (it == cache.sectionCache.end())
        {
            ModuleSectionCache newCache{};
            if (!build_section_cache_for_module(modulePath, newCache))
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
        auto& cache = ProcessInternal::get_module_cache();
        std::scoped_lock lock{cache.cacheMutex};
        cache.importCache.clear();
        cache.exportCache.clear();
        cache.sectionCache.clear();
    }
}
