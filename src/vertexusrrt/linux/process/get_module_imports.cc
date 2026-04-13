//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <array>
#include <vertexusrrt/process_internal.hh>

#include <elf.h>

#include <algorithm>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{
    StatusCode copy_to_user_buffer(const std::vector<ModuleImport>& source,
        ModuleImport** imports, std::uint32_t* count)
    {
        const auto actualCount = static_cast<std::uint32_t>(source.size());

        if (!imports)
        {
            *count = actualCount;
            return StatusCode::STATUS_OK;
        }

        if (!*imports || *count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t capacity = *count;
        const std::uint32_t copyCount = std::min(capacity, actualCount);

        std::copy_n(source.begin(), copyCount, *imports);
        *count = copyCount;

        if (actualCount > capacity)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }

    void enumerate_rela_plt64(const std::uint64_t baseAddress,
        const std::uint64_t relaAddr, const std::uint64_t relaSize,
        const std::uint64_t symTabAddr, const std::uint64_t strTabAddr,
        const std::uint64_t symEnt, const char* libName,
        ProcessInternal::ModuleImportCache& newCache)
    {
        const std::uint64_t nEntries = relaSize / sizeof(Elf64_Rela);
        for (std::uint64_t i = 0; i < nEntries; ++i)
        {
            Elf64_Rela rela{};
            if (!ProcessInternal::read_remote(relaAddr + i * sizeof(Elf64_Rela), rela))
            {
                break;
            }

            const std::uint32_t symIdx = ELF64_R_SYM(rela.r_info);
            if (symIdx == 0)
            {
                continue;
            }

            Elf64_Sym sym{};
            if (!ProcessInternal::read_remote(symTabAddr + symIdx * symEnt, sym))
            {
                continue;
            }

            if (sym.st_shndx != SHN_UNDEF)
            {
                continue;
            }

            auto nameOpt = ProcessInternal::read_remote_string(strTabAddr + sym.st_name, 256);
            if (!nameOpt || nameOpt->empty())
            {
                continue;
            }

            newCache.stringStorage.push_back(std::move(*nameOpt));
            const char* symName = newCache.stringStorage.back().c_str();

            ModuleImport imp{};
            imp.libraryName = libName;
            imp.importAddress = reinterpret_cast<void*>(rela.r_offset);
            imp.entry.name = symName;
            imp.entry.address = nullptr;
            imp.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
            imp.entry.ordinal = static_cast<int>(symIdx);
            imp.entry.isFunction = (ELF64_ST_TYPE(sym.st_info) == STT_FUNC) ? 1 : 0;
            imp.entry.isImport = 1;
            imp.entry.isForwarder = 0;
            imp.entry.forwarderName = nullptr;
            imp.isOrdinal = 0;
            imp.hint = 0;

            newCache.imports.push_back(imp);
        }
    }

    void enumerate_rela_plt32(const std::uint64_t baseAddress,
        const std::uint64_t relAddr, const std::uint64_t relSize,
        const std::uint64_t symTabAddr, const std::uint64_t strTabAddr,
        const std::uint64_t symEnt, const char* libName,
        ProcessInternal::ModuleImportCache& newCache)
    {
        const std::uint64_t nEntries = relSize / sizeof(Elf32_Rel);
        for (std::uint64_t i = 0; i < nEntries; ++i)
        {
            Elf32_Rel rel{};
            if (!ProcessInternal::read_remote(relAddr + i * sizeof(Elf32_Rel), rel))
            {
                break;
            }

            const std::uint32_t symIdx = ELF32_R_SYM(rel.r_info);
            if (symIdx == 0)
            {
                continue;
            }

            Elf32_Sym sym{};
            if (!ProcessInternal::read_remote(symTabAddr + symIdx * symEnt, sym))
            {
                continue;
            }

            if (sym.st_shndx != SHN_UNDEF)
            {
                continue;
            }

            auto nameOpt = ProcessInternal::read_remote_string(strTabAddr + sym.st_name, 256);
            if (!nameOpt || nameOpt->empty())
            {
                continue;
            }

            newCache.stringStorage.push_back(std::move(*nameOpt));
            const char* symName = newCache.stringStorage.back().c_str();

            ModuleImport imp{};
            imp.libraryName = libName;
            imp.importAddress = reinterpret_cast<void*>(static_cast<std::uint64_t>(rel.r_offset));
            imp.entry.name = symName;
            imp.entry.address = nullptr;
            imp.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
            imp.entry.ordinal = static_cast<int>(symIdx);
            imp.entry.isFunction = (ELF32_ST_TYPE(sym.st_info) == STT_FUNC) ? 1 : 0;
            imp.entry.isImport = 1;
            imp.entry.isForwarder = 0;
            imp.entry.forwarderName = nullptr;
            imp.isOrdinal = 0;
            imp.hint = 0;

            newCache.imports.push_back(imp);
        }
    }

    struct DynImportInfo final
    {
        std::uint64_t symTabAddr{};
        std::uint64_t strTabAddr{};
        std::uint64_t symEnt{sizeof(Elf64_Sym)};
        std::uint64_t relaPltAddr{};
        std::uint64_t relaPltSize{};
        std::uint64_t relPltAddr{};
        std::uint64_t relPltSize{};
        std::vector<std::string> neededLibs{};
    };

    [[nodiscard]] DynImportInfo parse_dyn_imports64(
        const std::uint64_t dynAddr, const std::uint64_t dynSize,
        const std::uint64_t strTabAddr)
    {
        DynImportInfo info{};
        info.symEnt = sizeof(Elf64_Sym);
        const std::uint64_t dynEntries = dynSize / sizeof(Elf64_Dyn);

        std::vector<std::uint64_t> neededOffsets{};

        for (std::uint64_t i = 0; i < dynEntries; ++i)
        {
            Elf64_Dyn dyn{};
            if (!ProcessInternal::read_remote(dynAddr + i * sizeof(Elf64_Dyn), dyn))
            {
                break;
            }
            if (dyn.d_tag == DT_NULL)
            {
                break;
            }
            switch (dyn.d_tag)
            {
            case DT_SYMTAB:    info.symTabAddr = dyn.d_un.d_ptr; break;
            case DT_STRTAB:    info.strTabAddr = dyn.d_un.d_ptr; break;
            case DT_SYMENT:    info.symEnt = dyn.d_un.d_val; break;
            case DT_JMPREL:    info.relaPltAddr = dyn.d_un.d_ptr; break;
            case DT_PLTRELSZ:  info.relaPltSize = dyn.d_un.d_val; break;
            case DT_NEEDED:    neededOffsets.push_back(dyn.d_un.d_val); break;
            default:           break;
            }
        }

        const std::uint64_t resolvedStrTab =
            (info.strTabAddr != 0) ? info.strTabAddr : strTabAddr;

        for (const auto offset : neededOffsets)
        {
            auto name = ProcessInternal::read_remote_string(resolvedStrTab + offset, 256);
            if (name && !name->empty())
            {
                info.neededLibs.push_back(std::move(*name));
            }
        }

        return info;
    }

    [[nodiscard]] DynImportInfo parse_dyn_imports32(
        const std::uint64_t dynAddr, const std::uint64_t dynSize,
        const std::uint64_t strTabAddr)
    {
        DynImportInfo info{};
        info.symEnt = sizeof(Elf32_Sym);
        const std::uint64_t dynEntries = dynSize / sizeof(Elf32_Dyn);

        std::vector<std::uint64_t> neededOffsets{};

        for (std::uint64_t i = 0; i < dynEntries; ++i)
        {
            Elf32_Dyn dyn{};
            if (!ProcessInternal::read_remote(dynAddr + i * sizeof(Elf32_Dyn), dyn))
            {
                break;
            }
            if (dyn.d_tag == DT_NULL)
            {
                break;
            }
            switch (dyn.d_tag)
            {
            case DT_SYMTAB:    info.symTabAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_STRTAB:    info.strTabAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_SYMENT:    info.symEnt = static_cast<std::uint64_t>(dyn.d_un.d_val); break;
            case DT_JMPREL:    info.relPltAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_PLTRELSZ:  info.relPltSize = static_cast<std::uint64_t>(dyn.d_un.d_val); break;
            case DT_NEEDED:    neededOffsets.push_back(static_cast<std::uint64_t>(dyn.d_un.d_val)); break;
            default:           break;
            }
        }

        const std::uint64_t resolvedStrTab =
            (info.strTabAddr != 0) ? info.strTabAddr : strTabAddr;

        for (const auto offset : neededOffsets)
        {
            auto name = ProcessInternal::read_remote_string(resolvedStrTab + offset, 256);
            if (name && !name->empty())
            {
                info.neededLibs.push_back(std::move(*name));
            }
        }

        return info;
    }

    StatusCode populate_and_copy(const std::uint64_t baseAddress,
        ProcessInternal::ModuleCache& cache,
        ProcessInternal::ModuleImportCache&& builtCache,
        ModuleImport** imports, std::uint32_t* count)
    {
        std::scoped_lock lock{cache.cacheMutex};
        auto [it, inserted] = cache.importCache.try_emplace(baseAddress);
        if (inserted)
        {
            it->second.imports.swap(builtCache.imports);
            it->second.stringStorage.swap(builtCache.stringStorage);
        }
        return copy_to_user_buffer(it->second.imports, imports, count);
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_imports(
        const ModuleInformation* module, ModuleImport** imports, uint32_t* count)
    {
        if (!module || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint64_t baseAddress = module->baseAddress;
        ProcessInternal::ModuleCache& cache = ProcessInternal::get_module_cache();

        {
            std::scoped_lock lock{cache.cacheMutex};
            if (const auto it = cache.importCache.find(baseAddress); it != cache.importCache.end())
            {
                return copy_to_user_buffer(it->second.imports, imports, count);
            }
        }

        std::array<std::uint8_t, EI_NIDENT> ident{};
        if (!ProcessInternal::read_remote_buffer(baseAddress, ident.data(), EI_NIDENT))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (ident[EI_MAG0] != ELFMAG0 || ident[EI_MAG1] != ELFMAG1 ||
            ident[EI_MAG2] != ELFMAG2 || ident[EI_MAG3] != ELFMAG3)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        const bool is64 = (ident[EI_CLASS] == ELFCLASS64);

        ProcessInternal::ModuleImportCache newCache{};

        std::uint64_t dynVaddr{};
        std::uint64_t dynSize{};
        std::uint64_t strTabFromPhdr{};
        std::uint64_t firstLoadVaddr{std::numeric_limits<std::uint64_t>::max()};

        if (is64)
        {
            Elf64_Ehdr ehdr{};
            if (!ProcessInternal::read_remote(baseAddress, ehdr))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }

            for (std::uint16_t i = 0; i < ehdr.e_phnum; ++i)
            {
                Elf64_Phdr phdr{};
                if (!ProcessInternal::read_remote(
                        baseAddress + ehdr.e_phoff + i * sizeof(Elf64_Phdr), phdr))
                {
                    continue;
                }
                if (phdr.p_type == PT_LOAD && phdr.p_vaddr < firstLoadVaddr)
                {
                    firstLoadVaddr = phdr.p_vaddr;
                }
                if (phdr.p_type == PT_DYNAMIC)
                {
                    dynVaddr = phdr.p_vaddr;
                    dynSize = phdr.p_filesz;
                }
            }
        }
        else
        {
            Elf32_Ehdr ehdr{};
            if (!ProcessInternal::read_remote(baseAddress, ehdr))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }

            for (std::uint16_t i = 0; i < ehdr.e_phnum; ++i)
            {
                Elf32_Phdr phdr{};
                if (!ProcessInternal::read_remote(
                        baseAddress + ehdr.e_phoff + i * sizeof(Elf32_Phdr), phdr))
                {
                    continue;
                }
                if (phdr.p_type == PT_LOAD && static_cast<std::uint64_t>(phdr.p_vaddr) < firstLoadVaddr)
                {
                    firstLoadVaddr = static_cast<std::uint64_t>(phdr.p_vaddr);
                }
                if (phdr.p_type == PT_DYNAMIC)
                {
                    dynVaddr = static_cast<std::uint64_t>(phdr.p_vaddr);
                    dynSize = static_cast<std::uint64_t>(phdr.p_filesz);
                }
            }
        }

        if (dynVaddr == 0)
        {
            return populate_and_copy(baseAddress, cache, std::move(newCache), imports, count);
        }

        if (firstLoadVaddr == std::numeric_limits<std::uint64_t>::max())
        {
            firstLoadVaddr = 0;
        }

        const std::uint64_t loadBias = baseAddress - firstLoadVaddr;
        const std::uint64_t dynAddr = loadBias + dynVaddr;

        if (is64)
        {
            const auto info = parse_dyn_imports64(dynAddr, dynSize, strTabFromPhdr);

            if (info.symTabAddr == 0 || info.strTabAddr == 0)
            {
                return populate_and_copy(baseAddress, cache, std::move(newCache), imports, count);
            }

            const char* defaultLib = "";
            if (!info.neededLibs.empty())
            {
                newCache.stringStorage.push_back(info.neededLibs.front());
                defaultLib = newCache.stringStorage.back().c_str();
            }

            if (info.relaPltAddr != 0 && info.relaPltSize != 0)
            {
                enumerate_rela_plt64(baseAddress, info.relaPltAddr, info.relaPltSize,
                    info.symTabAddr, info.strTabAddr, info.symEnt, defaultLib, newCache);
            }
        }
        else
        {
            const auto info = parse_dyn_imports32(dynAddr, dynSize, strTabFromPhdr);

            if (info.symTabAddr == 0 || info.strTabAddr == 0)
            {
                return populate_and_copy(baseAddress, cache, std::move(newCache), imports, count);
            }

            const char* defaultLib = "";
            if (!info.neededLibs.empty())
            {
                newCache.stringStorage.push_back(info.neededLibs.front());
                defaultLib = newCache.stringStorage.back().c_str();
            }

            if (info.relPltAddr != 0 && info.relPltSize != 0)
            {
                enumerate_rela_plt32(baseAddress, info.relPltAddr, info.relPltSize,
                    info.symTabAddr, info.strTabAddr, info.symEnt, defaultLib, newCache);
            }
        }

        return populate_and_copy(baseAddress, cache, std::move(newCache), imports, count);
    }
}