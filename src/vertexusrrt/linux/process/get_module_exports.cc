//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

#include <elf.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <vector>

namespace
{
    StatusCode copy_to_user_buffer(const std::vector<ModuleExport>& source,
        ModuleExport** exports, std::uint32_t* count)
    {
        const auto actualCount = static_cast<std::uint32_t>(source.size());

        if (!exports)
        {
            *count = actualCount;
            return StatusCode::STATUS_OK;
        }

        if (!*exports || *count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t capacity = *count;
        const std::uint32_t copyCount = std::min(capacity, actualCount);

        std::copy_n(source.begin(), copyCount, *exports);
        *count = copyCount;

        if (actualCount > capacity)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }

    [[nodiscard]] std::uint32_t symbol_count_from_hash(const std::uint64_t hashAddr)
    {
        std::uint32_t nchain{};
        if (!ProcessInternal::read_remote(hashAddr + sizeof(std::uint32_t), nchain))
        {
            return 0;
        }
        return nchain;
    }

    [[nodiscard]] std::uint32_t symbol_count_from_gnu_hash(
        const std::uint64_t gnuHashAddr, const bool is64)
    {
        std::uint32_t nbuckets{}, symoffset{}, bloomSize{};
        std::uint64_t cursor = gnuHashAddr;

        if (!ProcessInternal::read_remote(cursor, nbuckets))
        {
            return 0;
        }
        cursor += 4;
        if (!ProcessInternal::read_remote(cursor, symoffset))
        {
            return 0;
        }
        cursor += 4;
        if (!ProcessInternal::read_remote(cursor, bloomSize))
        {
            return 0;
        }
        cursor += 8;

        const std::uint64_t bloomEntrySize = is64 ? 8u : 4u;
        cursor += static_cast<std::uint64_t>(bloomSize) * bloomEntrySize;

        const std::uint64_t bucketBase = cursor;
        cursor += static_cast<std::uint64_t>(nbuckets) * 4;
        const std::uint64_t chainBase = cursor;

        std::uint32_t maxSymIdx = symoffset;
        for (std::uint32_t b = 0; b < nbuckets; ++b)
        {
            std::uint32_t symIdx{};
            if (!ProcessInternal::read_remote(bucketBase + b * 4, symIdx) || symIdx == 0)
            {
                continue;
            }

            std::uint32_t chainIdx = symIdx - symoffset;
            for (std::uint32_t j = 0; j < 65536u; ++j)
            {
                std::uint32_t chainVal{};
                if (!ProcessInternal::read_remote(chainBase + (chainIdx + j) * 4, chainVal))
                {
                    break;
                }
                if (symIdx + j > maxSymIdx)
                {
                    maxSymIdx = symIdx + j;
                }
                if (chainVal & 1u)
                {
                    break;
                }
            }
        }

        return maxSymIdx + 1;
    }

    struct ElfDynInfo final
    {
        std::uint64_t symTabAddr{};
        std::uint64_t strTabAddr{};
        std::uint64_t hashAddr{};
        std::uint64_t gnuHashAddr{};
        std::uint64_t symEnt{sizeof(Elf64_Sym)};
    };

    [[nodiscard]] std::optional<ElfDynInfo> parse_dynamic64(
        const std::uint64_t dynAddr, const std::uint64_t dynSize)
    {
        ElfDynInfo info{};
        const std::uint64_t dynEntries = dynSize / sizeof(Elf64_Dyn);

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
            case DT_SYMTAB:   info.symTabAddr = dyn.d_un.d_ptr; break;
            case DT_STRTAB:   info.strTabAddr = dyn.d_un.d_ptr; break;
            case DT_SYMENT:   info.symEnt = dyn.d_un.d_val; break;
            case DT_HASH:     info.hashAddr = dyn.d_un.d_ptr; break;
            case DT_GNU_HASH: info.gnuHashAddr = dyn.d_un.d_ptr; break;
            default:          break;
            }
        }

        if (info.symTabAddr == 0 || info.strTabAddr == 0)
        {
            return std::nullopt;
        }
        return info;
    }

    [[nodiscard]] std::optional<ElfDynInfo> parse_dynamic32(
        const std::uint64_t dynAddr, const std::uint64_t dynSize)
    {
        ElfDynInfo info{};
        info.symEnt = sizeof(Elf32_Sym);
        const std::uint64_t dynEntries = dynSize / sizeof(Elf32_Dyn);

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
            case DT_SYMTAB:   info.symTabAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_STRTAB:   info.strTabAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_SYMENT:   info.symEnt = static_cast<std::uint64_t>(dyn.d_un.d_val); break;
            case DT_HASH:     info.hashAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            case DT_GNU_HASH: info.gnuHashAddr = static_cast<std::uint64_t>(dyn.d_un.d_ptr); break;
            default:          break;
            }
        }

        if (info.symTabAddr == 0 || info.strTabAddr == 0)
        {
            return std::nullopt;
        }
        return info;
    }

    StatusCode populate_and_copy(const std::uint64_t baseAddress,
        ProcessInternal::ModuleCache& cache,
        ProcessInternal::ModuleExportCache&& builtCache,
        ModuleExport** exports, std::uint32_t* count)
    {
        std::scoped_lock lock{cache.cacheMutex};
        auto [it, inserted] = cache.exportCache.try_emplace(baseAddress);
        if (inserted)
        {
            it->second.exports.swap(builtCache.exports);
            it->second.stringStorage.swap(builtCache.stringStorage);
        }
        return copy_to_user_buffer(it->second.exports, exports, count);
    }

    StatusCode enumerate_exports64(const std::uint64_t baseAddress,
        const ElfDynInfo& dynInfo,
        const char* moduleName,
        ProcessInternal::ModuleCache& cache,
        ProcessInternal::ModuleExportCache& newCache,
        ModuleExport** exports, std::uint32_t* count)
    {
        std::uint32_t nSymbols = 0;
        if (dynInfo.hashAddr != 0)
        {
            nSymbols = symbol_count_from_hash(dynInfo.hashAddr);
        }
        else if (dynInfo.gnuHashAddr != 0)
        {
            nSymbols = symbol_count_from_gnu_hash(dynInfo.gnuHashAddr, true);
        }

        if (nSymbols == 0 || nSymbols > 65536u)
        {
            return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
        }

        for (std::uint32_t i = 1; i < nSymbols; ++i)
        {
            Elf64_Sym sym{};
            if (!ProcessInternal::read_remote(
                    dynInfo.symTabAddr + i * dynInfo.symEnt, sym))
            {
                break;
            }

            if (sym.st_shndx == SHN_UNDEF)
            {
                continue;
            }

            const auto bind = ELF64_ST_BIND(sym.st_info);
            if (bind != STB_GLOBAL && bind != STB_WEAK)
            {
                continue;
            }

            const auto vis = ELF64_ST_VISIBILITY(sym.st_other);
            if (vis == STV_HIDDEN || vis == STV_INTERNAL)
            {
                continue;
            }

            auto nameOpt = ProcessInternal::read_remote_string(dynInfo.strTabAddr + sym.st_name, 256);
            if (!nameOpt || nameOpt->empty())
            {
                continue;
            }

            newCache.stringStorage.push_back(std::move(*nameOpt));
            const char* symName = newCache.stringStorage.back().c_str();

            ModuleExport exp{};
            exp.moduleName = moduleName;
            exp.entry.name = symName;
            exp.entry.address = reinterpret_cast<void*>(sym.st_value);
            exp.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
            exp.entry.ordinal = static_cast<int>(i);
            exp.entry.isFunction = (ELF64_ST_TYPE(sym.st_info) == STT_FUNC) ? 1 : 0;
            exp.entry.isImport = 0;
            exp.entry.isForwarder = 0;
            exp.entry.forwarderName = nullptr;
            exp.isData = (ELF64_ST_TYPE(sym.st_info) == STT_OBJECT) ? 1 : 0;
            exp.isThunk = 0;
            exp.relocationTable = nullptr;
            exp.characteristics = 0;

            newCache.exports.push_back(exp);
        }

        return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
    }

    StatusCode enumerate_exports32(const std::uint64_t baseAddress,
        const ElfDynInfo& dynInfo,
        const char* moduleName,
        ProcessInternal::ModuleCache& cache,
        ProcessInternal::ModuleExportCache& newCache,
        ModuleExport** exports, std::uint32_t* count)
    {
        std::uint32_t nSymbols = 0;
        if (dynInfo.hashAddr != 0)
        {
            nSymbols = symbol_count_from_hash(dynInfo.hashAddr);
        }
        else if (dynInfo.gnuHashAddr != 0)
        {
            nSymbols = symbol_count_from_gnu_hash(dynInfo.gnuHashAddr, false);
        }

        if (nSymbols == 0 || nSymbols > 65536u)
        {
            return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
        }

        for (std::uint32_t i = 1; i < nSymbols; ++i)
        {
            Elf32_Sym sym{};
            if (!ProcessInternal::read_remote(
                    dynInfo.symTabAddr + i * dynInfo.symEnt, sym))
            {
                break;
            }

            if (sym.st_shndx == SHN_UNDEF)
            {
                continue;
            }

            const auto bind = ELF32_ST_BIND(sym.st_info);
            if (bind != STB_GLOBAL && bind != STB_WEAK)
            {
                continue;
            }

            const auto vis = ELF32_ST_VISIBILITY(sym.st_other);
            if (vis == STV_HIDDEN || vis == STV_INTERNAL)
            {
                continue;
            }

            auto nameOpt = ProcessInternal::read_remote_string(dynInfo.strTabAddr + sym.st_name, 256);
            if (!nameOpt || nameOpt->empty())
            {
                continue;
            }

            newCache.stringStorage.push_back(std::move(*nameOpt));
            const char* symName = newCache.stringStorage.back().c_str();

            ModuleExport exp{};
            exp.moduleName = moduleName;
            exp.entry.name = symName;
            exp.entry.address = reinterpret_cast<void*>(static_cast<std::uint64_t>(sym.st_value));
            exp.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
            exp.entry.ordinal = static_cast<int>(i);
            exp.entry.isFunction = (ELF32_ST_TYPE(sym.st_info) == STT_FUNC) ? 1 : 0;
            exp.entry.isImport = 0;
            exp.entry.isForwarder = 0;
            exp.entry.forwarderName = nullptr;
            exp.isData = (ELF32_ST_TYPE(sym.st_info) == STT_OBJECT) ? 1 : 0;
            exp.isThunk = 0;
            exp.relocationTable = nullptr;
            exp.characteristics = 0;

            newCache.exports.push_back(exp);
        }

        return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_exports(
        const ModuleInformation* module, ModuleExport** exports, uint32_t* count)
    {
        if (!module || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint64_t baseAddress = module->baseAddress;
        ProcessInternal::ModuleCache& cache = ProcessInternal::get_module_cache();

        {
            std::scoped_lock lock{cache.cacheMutex};
            if (const auto it = cache.exportCache.find(baseAddress); it != cache.exportCache.end())
            {
                return copy_to_user_buffer(it->second.exports, exports, count);
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

        ProcessInternal::ModuleExportCache newCache{};
        newCache.stringStorage.emplace_back(module->moduleName);
        const char* moduleName = newCache.stringStorage.back().c_str();

        std::uint64_t dynVaddr{};
        std::uint64_t dynSize{};
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
            return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
        }

        if (firstLoadVaddr == std::numeric_limits<std::uint64_t>::max())
        {
            firstLoadVaddr = 0;
        }

        const std::uint64_t loadBias = baseAddress - firstLoadVaddr;
        const std::uint64_t dynAddr = loadBias + dynVaddr;

        if (is64)
        {
            const auto dynInfo = parse_dynamic64(dynAddr, dynSize);
            if (!dynInfo)
            {
                return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
            }
            return enumerate_exports64(baseAddress, *dynInfo, moduleName, cache, newCache, exports, count);
        }

        const auto dynInfo = parse_dynamic32(dynAddr, dynSize);
        if (!dynInfo)
        {
            return populate_and_copy(baseAddress, cache, std::move(newCache), exports, count);
        }
        return enumerate_exports32(baseAddress, *dynInfo, moduleName, cache, newCache, exports, count);
    }
}
