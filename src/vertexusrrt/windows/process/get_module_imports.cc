//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_imports(const ModuleInformation* module, ModuleImport** imports, uint32_t* count)
    {
        if (!module || !imports || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint64_t baseAddress = module->baseAddress;
        ProcessInternal::ModuleCache& cache = ProcessInternal::get_module_cache();

        {
            std::scoped_lock lock{cache.cacheMutex};
            auto it = cache.importCache.find(baseAddress);
            if (it != cache.importCache.end())
            {
                *imports = it->second.imports.data();
                *count = static_cast<uint32_t>(it->second.imports.size());
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

        bool is64Bit = false;

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

        is64Bit = (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 || fileHeader.Machine == IMAGE_FILE_MACHINE_ARM64);

        DWORD importRva = 0;
        if (is64Bit)
        {
            IMAGE_NT_HEADERS64 ntHeaders64{};
            if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders64))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            importRva = ntHeaders64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        }
        else
        {
            IMAGE_NT_HEADERS32 ntHeaders32{};
            if (!ProcessInternal::read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders32))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            importRva = ntHeaders32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        }

        ProcessInternal::ModuleImportCache newCache{};

        if (importRva == 0)
        {
            std::scoped_lock lock{cache.cacheMutex};
            cache.importCache[baseAddress] = std::move(newCache);
            *imports = nullptr;
            *count = 0;
            return StatusCode::STATUS_OK;
        }

        uint64_t importDescAddr = baseAddress + importRva;
        IMAGE_IMPORT_DESCRIPTOR importDesc{};

        while (true)
        {
            if (!ProcessInternal::read_remote(importDescAddr, importDesc))
            {
                break;
            }

            if (importDesc.Name == 0)
            {
                break;
            }

            auto libraryNameOpt = ProcessInternal::read_remote_string(baseAddress + importDesc.Name);
            if (!libraryNameOpt)
            {
                importDescAddr += sizeof(IMAGE_IMPORT_DESCRIPTOR);
                continue;
            }

            newCache.stringStorage.push_back(std::move(*libraryNameOpt));
            const char* libraryName = newCache.stringStorage.back().c_str();

            uint64_t thunkAddr = baseAddress + (importDesc.OriginalFirstThunk ? importDesc.OriginalFirstThunk : importDesc.FirstThunk);
            uint64_t iatAddr = baseAddress + importDesc.FirstThunk;

            if (is64Bit)
            {
                IMAGE_THUNK_DATA64 thunk{};
                size_t thunkIndex = 0;

                while (true)
                {
                    if (!ProcessInternal::read_remote(thunkAddr, thunk))
                        break;
                    if (thunk.u1.AddressOfData == 0)
                        break;

                    ModuleImport import{};
                    import.entry = {};
                    import.libraryName = libraryName;
                    import.importAddress = reinterpret_cast<void*>(iatAddr + thunkIndex * sizeof(IMAGE_THUNK_DATA64));
                    import.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
                    import.entry.isImport = 1;

                    if (IMAGE_SNAP_BY_ORDINAL64(thunk.u1.Ordinal))
                    {
                        import.isOrdinal = 1;
                        import.entry.ordinal = static_cast<int>(IMAGE_ORDINAL64(thunk.u1.Ordinal));
                        import.entry.name = nullptr;
                        import.hint = 0;
                    }
                    else
                    {
                        import.isOrdinal = 0;
                        uint64_t nameAddr = baseAddress + (thunk.u1.AddressOfData & 0x7FFFFFFFFFFFFFFF);

                        uint16_t hint;
                        if (ProcessInternal::read_remote(nameAddr, hint))
                        {
                            import.hint = hint;
                        }

                        auto funcNameOpt = ProcessInternal::read_remote_string(nameAddr + sizeof(uint16_t));
                        if (funcNameOpt)
                        {
                            newCache.stringStorage.push_back(std::move(*funcNameOpt));
                            import.entry.name = newCache.stringStorage.back().c_str();
                        }
                    }

                    import.entry.isFunction = 1;
                    newCache.imports.push_back(import);

                    thunkAddr += sizeof(IMAGE_THUNK_DATA64);
                    thunkIndex++;
                }
            }
            else
            {
                IMAGE_THUNK_DATA32 thunk{};
                size_t thunkIndex = 0;

                while (true)
                {
                    if (!ProcessInternal::read_remote(thunkAddr, thunk))
                        break;
                    if (thunk.u1.AddressOfData == 0)
                        break;

                    ModuleImport import{};
                    import.entry = {};
                    import.libraryName = libraryName;
                    import.importAddress = reinterpret_cast<void*>(iatAddr + thunkIndex * sizeof(IMAGE_THUNK_DATA32));
                    import.entry.moduleHandle = reinterpret_cast<void*>(baseAddress);
                    import.entry.isImport = 1;

                    if (IMAGE_SNAP_BY_ORDINAL32(thunk.u1.Ordinal))
                    {
                        import.isOrdinal = 1;
                        import.entry.ordinal = static_cast<int>(IMAGE_ORDINAL32(thunk.u1.Ordinal));
                        import.entry.name = nullptr;
                        import.hint = 0;
                    }
                    else
                    {
                        import.isOrdinal = 0;
                        uint64_t nameAddr = baseAddress + (thunk.u1.AddressOfData & 0x7FFFFFFF);

                        uint16_t hint;
                        if (ProcessInternal::read_remote(nameAddr, hint))
                        {
                            import.hint = hint;
                        }

                        auto funcNameOpt = ProcessInternal::read_remote_string(nameAddr + sizeof(uint16_t));
                        if (funcNameOpt)
                        {
                            newCache.stringStorage.push_back(std::move(*funcNameOpt));
                            import.entry.name = newCache.stringStorage.back().c_str();
                        }
                    }

                    import.entry.isFunction = 1;
                    newCache.imports.push_back(import);

                    thunkAddr += sizeof(IMAGE_THUNK_DATA32);
                    thunkIndex++;
                }
            }

            importDescAddr += sizeof(IMAGE_IMPORT_DESCRIPTOR);
        }

        std::scoped_lock lock{cache.cacheMutex};
        cache.importCache[baseAddress] = std::move(newCache);
        auto& cached = cache.importCache[baseAddress];
        *imports = cached.imports.empty() ? nullptr : cached.imports.data();
        *count = static_cast<uint32_t>(cached.imports.size());

        return StatusCode::STATUS_OK;
    }
}
