//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_exports(const ModuleInformation* module, ModuleExport** exports, uint32_t* count)
    {
        if (!module || !exports || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint64_t baseAddress = module->baseAddress;
        ProcessInternal::ModuleCache& cache = ProcessInternal::get_module_cache();

        {
            std::scoped_lock lock{cache.cacheMutex};
            auto it = cache.exportCache.find(baseAddress);
            if (it != cache.exportCache.end())
            {
                *exports = it->second.exports.data();
                *count = static_cast<uint32_t>(it->second.exports.size());
                return StatusCode::STATUS_OK;
            }
        }

        IMAGE_DOS_HEADER dosHeader{};
        if (!ProcessInternal::read_remote(baseAddress, dosHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        uint32_t signature;
        if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew, signature))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (signature != IMAGE_NT_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        IMAGE_FILE_HEADER fileHeader{};
        if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew + sizeof(DWORD), fileHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        bool is64Bit = (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 || fileHeader.Machine == IMAGE_FILE_MACHINE_ARM64);

        DWORD exportRva = 0;
        DWORD exportSize = 0;

        if (is64Bit)
        {
            IMAGE_NT_HEADERS64 ntHeaders{};
            if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }
        else
        {
            IMAGE_NT_HEADERS32 ntHeaders{};
            if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }

        ProcessInternal::ModuleExportCache newCache{};

        newCache.stringStorage.emplace_back(module->moduleName);
        const char* moduleName = newCache.stringStorage.back().c_str();

        if (exportRva == 0)
        {
            std::scoped_lock lock{cache.cacheMutex};
            cache.exportCache[baseAddress] = std::move(newCache);
            *exports = nullptr;
            *count = 0;
            return StatusCode::STATUS_OK;
        }

        IMAGE_EXPORT_DIRECTORY exportDir{};
        if (!ProcessInternal::read_remote(baseAddress + exportRva, exportDir))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        uint32_t numFunctions = exportDir.NumberOfFunctions;
        uint32_t numNames = exportDir.NumberOfNames;

        if (numFunctions == 0)
        {
            std::scoped_lock lock{cache.cacheMutex};
            cache.exportCache[baseAddress] = std::move(newCache);
            *exports = nullptr;
            *count = 0;
            return StatusCode::STATUS_OK;
        }

        std::vector<DWORD> functionRvas(numFunctions);
        if (!ProcessInternal::read_remote_buffer(baseAddress + exportDir.AddressOfFunctions, functionRvas.data(), numFunctions * sizeof(DWORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::vector<DWORD> nameRvas(numNames);
        if (numNames > 0 && !ProcessInternal::read_remote_buffer(baseAddress + exportDir.AddressOfNames, nameRvas.data(), numNames * sizeof(DWORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::vector<WORD> ordinals(numNames);
        if (numNames > 0 && !ProcessInternal::read_remote_buffer(baseAddress + exportDir.AddressOfNameOrdinals, ordinals.data(), numNames * sizeof(WORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::unordered_map<DWORD, std::string> ordinalToName{};
        for (uint32_t i = 0; i < numNames; ++i)
        {
            auto nameOpt = ProcessInternal::read_remote_string(baseAddress + nameRvas[i]);
            if (nameOpt)
            {
                ordinalToName[ordinals[i]] = std::move(*nameOpt);
            }
        }

        for (uint32_t i = 0; i < numFunctions; ++i)
        {
            if (functionRvas[i] == 0)
            {
                continue;
            }

            ModuleExport exp{};
            exp.entry = {};
            exp.moduleName = moduleName;
            exp.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
            exp.entry.ordinal = static_cast<int>(exportDir.Base + i);
            exp.entry.isImport = 0;

            uint64_t funcAddr = baseAddress + functionRvas[i];

            const bool isForwarder = (functionRvas[i] >= exportRva && functionRvas[i] < exportRva + exportSize);

            if (isForwarder)
            {
                exp.entry.isForwarder = 1;
                auto forwarderOpt = ProcessInternal::read_remote_string(funcAddr);
                if (forwarderOpt)
                {
                    newCache.stringStorage.push_back(std::move(*forwarderOpt));
                    exp.entry.forwarderName = newCache.stringStorage.back().c_str();
                }
                exp.entry.address = nullptr;
            }
            else
            {
                exp.entry.isForwarder = 0;
                exp.entry.forwarderName = nullptr;
                exp.entry.address = reinterpret_cast<void*>(funcAddr);
            }

            const auto nameIt = ordinalToName.find(i);
            if (nameIt != ordinalToName.end())
            {
                newCache.stringStorage.push_back(nameIt->second);
                exp.entry.name = newCache.stringStorage.back().c_str();
            }
            else
            {
                exp.entry.name = nullptr;
            }

            exp.entry.isFunction = 1;
            exp.isData = 0;
            exp.isThunk = 0;
            exp.relocationTable = nullptr;
            exp.characteristics = 0;

            newCache.exports.push_back(exp);
        }

        std::scoped_lock lock{cache.cacheMutex};
        cache.exportCache[baseAddress] = std::move(newCache);
        auto& cached = cache.exportCache[baseAddress];
        *exports = cached.exports.empty() ? nullptr : cached.exports.data();
        *count = static_cast<uint32_t>(cached.exports.size());

        return StatusCode::STATUS_OK;
    }
}
