//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>
#include <sdk/process.h>

#include <Windows.h>
#include <tlhelp32.h>

#include <algorithm>
#include <array>
#include <mutex>
#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

#if !defined(UNICODE)
#error ("Vertex Plugin must be compiled with UNICODE directive.")
#endif

extern ProcessArchitecture get_process_architecture();
extern void cache_process_architecture();
extern void clear_process_architecture();

native_handle& get_native_handle()
{
    static native_handle handle;
    return handle;
}

namespace
{
    struct ModuleImportCache final
    {
        std::vector<ModuleImport> imports{};
        std::vector<std::string> stringStorage{};
    };

    struct ModuleExportCache final
    {
        std::vector<ModuleExport> exports{};
        std::vector<std::string> stringStorage{};
    };

    struct ModuleCache final
    {
        std::unordered_map<std::uint64_t, ModuleImportCache> importCache{};
        std::unordered_map<std::uint64_t, ModuleExportCache> exportCache{};
        std::mutex cacheMutex;
    };

    ModuleCache& get_module_cache()
    {
        static ModuleCache cache;
        return cache;
    }

    template <class T> bool read_remote(const std::uint64_t address, T& out) { return vertex_memory_read_process(address, sizeof(T), reinterpret_cast<char*>(&out)) == STATUS_OK; }

    bool read_remote_buffer(const std::uint64_t address, void* buffer, const std::size_t size) { return vertex_memory_read_process(address, size, static_cast<char*>(buffer)) == STATUS_OK; }

    std::optional<std::string> read_remote_string(const std::uint64_t address, std::size_t maxLen = 256)
    {
        std::string result;
        result.reserve(maxLen);

        char c;
        for (std::size_t i = 0; i < maxLen; ++i)
        {
            if (!read_remote(address + i, c))
            {
                return std::nullopt;
            }
            if (c == '\0')
            {
                break;
            }
            result.push_back(c);
        }

        return result;
    }
}

void clear_module_cache()
{
    auto& [importCache, exportCache, cacheMutex] = get_module_cache();
    std::scoped_lock lock{cacheMutex};
    importCache.clear();
    exportCache.clear();
}

namespace
{
    ProcessInformation* opened_process_info()
    {
        static ProcessInformation info{};
        return &info;
    }

    std::optional<std::string> wchar_to_utf8(const WCHAR* str) noexcept
    {
        if (!str)
        {
            return std::nullopt;
        }

        const int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0)
        {
            return std::nullopt;
        }

        std::string utf8_str(len - 1, '\0');
        const int result = WideCharToMultiByte(CP_UTF8, 0, str, -1, utf8_str.data(), len, nullptr, nullptr);
        return (result > 0) ? std::make_optional(std::move(utf8_str)) : std::nullopt;
    }

    std::optional<std::wstring> utf8_to_wchar(const char* utf8_str) noexcept
    {
        if (!utf8_str)
        {
            return std::nullopt;
        }

        const int len = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, nullptr, 0);
        if (len <= 0)
        {
            return std::nullopt;
        }

        std::wstring wide_str(len - 1, L'\0');
        const int result = MultiByteToWideChar(CP_UTF8, 0, utf8_str, -1, wide_str.data(), len);

        return (result > 0) ? std::make_optional(std::move(wide_str)) : std::nullopt;
    }

    void vertex_cpy(char* dst, const std::string& src, const std::size_t max_len) // NOLINT
    {
        if (!dst || max_len == 0)
        {
            return;
        }

        const std::size_t cpy_len = std::min(src.length(), max_len - 1);
        std::copy_n(src.begin(), cpy_len, dst);
        dst[cpy_len] = '\0';
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
} // namespace

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open(const uint32_t process_id)
    {
        native_handle& handle = get_native_handle();
        handle = OpenProcess(PROCESS_ALL_ACCESS, FALSE, process_id);
        if (handle == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        WCHAR full_proc_path[MAX_PATH];
        DWORD size = MAX_PATH;

        if (!QueryFullProcessImageName(handle, 0, full_proc_path, &size))
        {
            vertex_process_close();
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        const WCHAR* proc_name = wcsrchr(full_proc_path, L'\\');
        if (proc_name)
        {
            ++proc_name;
        }
        else
        {
            proc_name = full_proc_path;
        }

        cache_process_architecture();

        ProcessInformation* info = opened_process_info();

        info->processId = process_id;
        const auto proc_name_cpp_str = wchar_to_utf8(proc_name);
        if (!proc_name_cpp_str)
        {
            vertex_process_close();
            return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
        }
        vertex_cpy(info->processName, *proc_name_cpp_str, VERTEX_MAX_NAME_LENGTH);

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_close()
    {
        const native_handle& handle = get_native_handle();
        if (handle == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        return invalidate_handle();
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_kill()
    {
        const native_handle& handle = get_native_handle();
        if (handle == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        if (TerminateProcess(handle, STATUS_OK))
        {
            return invalidate_handle();
        }
        return StatusCode::STATUS_ERROR_PROCESS_INVALID;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_is_valid()
    {
        const native_handle& handle = get_native_handle();

        if (handle == INVALID_HANDLE_VALUE || handle == nullptr)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        DWORD exit_code = 0;
        if (!GetExitCodeProcess(handle, &exit_code))
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        return exit_code == STILL_ACTIVE ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_open_new(const char* process_path, const char* argv)
    {
        if (!process_path || !strlen(process_path))
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto proc_path_cpp_str = utf8_to_wchar(process_path);

        auto argv_cpp_str = utf8_to_wchar(argv);

        if (!proc_path_cpp_str)
        {
            return StatusCode::STATUS_ERROR_FMT_INVALID_CONVERSION;
        }

        STARTUPINFO startup_info{};
        startup_info.cb = sizeof(STARTUPINFO);

        PROCESS_INFORMATION process_info{};

        const BOOL result = CreateProcess(proc_path_cpp_str->c_str(), (argv_cpp_str && !argv_cpp_str->empty()) ? argv_cpp_str->data() : nullptr, nullptr, nullptr, TRUE, 0, nullptr, nullptr,
                                          &startup_info, &process_info);
        if (!result)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        CloseHandle(process_info.hThread);
        CloseHandle(process_info.hProcess);

        return vertex_process_open(process_info.dwProcessId);
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_extensions(char** extensions, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        static constexpr std::array kExecutableExtensions = {
          ".exe",
          ".com",
        };

        constexpr auto kActualCount = static_cast<uint32_t>(std::size(kExecutableExtensions));

        if (!extensions)
        {
            *count = kActualCount;
            return StatusCode::STATUS_OK;
        }

        if (*count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t buffer_size = *count;
        const uint32_t copy_count = std::min(buffer_size, kActualCount);

        for (uint32_t i = 0; i < copy_count; ++i)
        {
            extensions[i] = const_cast<char*>(kExecutableExtensions[i]);
        }

        *count = copy_count;

        if (kActualCount > buffer_size)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0); // NOLINT
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        std::vector<ProcessInformation> processes;

        PROCESSENTRY32W pe32{};
        pe32.dwSize = sizeof(PROCESSENTRY32W);

        if (Process32FirstW(snapshot, &pe32))
        {
            do
            {
                ProcessInformation info{};

                info.processId = pe32.th32ProcessID;

                const auto proc_name_opt = wchar_to_utf8(pe32.szExeFile);
                if (proc_name_opt && !proc_name_opt->empty())
                {
                    vertex_cpy(info.processName, *proc_name_opt, VERTEX_MAX_NAME_LENGTH);
                }
                else
                {
                    vertex_cpy(info.processName, "Unknown Process", VERTEX_MAX_NAME_LENGTH);
                }

                vertex_cpy(info.processOwner, "N/A", VERTEX_MAX_NAME_LENGTH);

                processes.push_back(info);

            } while (Process32NextW(snapshot, &pe32));
        }

        CloseHandle(snapshot);

        if (!list)
        {
            *count = static_cast<uint32_t>(processes.size());
            return StatusCode::STATUS_OK;
        }

        if (!*list)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t buffer_size = *count;
        const std::uint32_t actual_count = processes.size();

        if (buffer_size == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t copy_count = std::min(buffer_size, actual_count);

        ProcessInformation* buffer = *list;
        std::copy_n(processes.begin(), copy_count, buffer);

        *count = copy_count;

        if (actual_count > buffer_size)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_modules_list(ModuleInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const native_handle& processHandle = get_native_handle();
        if (processHandle == INVALID_HANDLE_VALUE || processHandle == nullptr)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        DWORD processId = opened_process_info()->processId;
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_NOT_FOUND;
        }

        HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE | TH32CS_SNAPMODULE32, processId);
        if (snapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
        }

        std::vector<ModuleInformation> modules{};
        MODULEENTRY32W moduleEntry{};
        moduleEntry.dwSize = sizeof(MODULEENTRY32W);

        if (Module32FirstW(snapshot, &moduleEntry))
        {
            do
            {
                ModuleInformation info{};

                if (moduleEntry.szModule[0] != L'\0')
                {
                    auto nameOpt = wchar_to_utf8(moduleEntry.szModule);
                    if (nameOpt && !nameOpt->empty())
                    {
                        vertex_cpy(info.moduleName, *nameOpt, VERTEX_MAX_NAME_LENGTH);
                    }
                    else
                    {
                        vertex_cpy(info.moduleName, std::string("Unknown"), VERTEX_MAX_NAME_LENGTH);
                    }
                }
                else
                {
                    vertex_cpy(info.moduleName, std::string("Unknown"), VERTEX_MAX_NAME_LENGTH);
                }

                if (moduleEntry.szExePath[0] != L'\0')
                {
                    auto pathOpt = wchar_to_utf8(moduleEntry.szExePath);
                    if (pathOpt && !pathOpt->empty())
                    {
                        vertex_cpy(info.modulePath, *pathOpt, VERTEX_MAX_PATH_LENGTH);
                    }
                    else
                    {
                        info.modulePath[0] = '\0';
                    }
                }
                else
                {
                    info.modulePath[0] = '\0';
                }

                info.baseAddress = reinterpret_cast<uint64_t>(moduleEntry.modBaseAddr);
                info.size = static_cast<uint64_t>(moduleEntry.modBaseSize);

                modules.push_back(info);

                moduleEntry.dwSize = sizeof(MODULEENTRY32W);

            } while (Module32NextW(snapshot, &moduleEntry));
        }

        CloseHandle(snapshot);

        if (!list)
        {
            *count = static_cast<uint32_t>(modules.size());
            return StatusCode::STATUS_OK;
        }

        if (!*list)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (*count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint32_t bufferSize = *count;
        const auto actualCount = static_cast<uint32_t>(modules.size());
        const uint32_t copyCount = std::min(bufferSize, actualCount);

        ModuleInformation* buffer = *list;
        std::copy_n(modules.begin(), copyCount, buffer);

        *count = copyCount;

        if (actualCount > bufferSize)
        {
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_injection_methods(VertexInjectionMethod** methods)
    {
        VertexInjectionMethod injectionMethods{};

        constexpr std::string_view normalInjection = "Normal Injection";
        constexpr std::string_view manualMappingInjection = "Manual Mapping Injection";

        std::copy_n(normalInjection.data(), normalInjection.size(), injectionMethods.methodName);
        std::copy_n(manualMappingInjection.data(), manualMappingInjection.size(), injectionMethods.methodName);

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_imports(const ModuleInformation* module, ModuleImport** imports, uint32_t* count)
    {
        if (!module || !imports || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint64_t baseAddress = module->baseAddress;
        ModuleCache& cache = get_module_cache();

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
        if (!read_remote(baseAddress, dosHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        bool is64Bit = false;

        uint32_t signature;
        if (!read_remote(baseAddress + dosHeader.e_lfanew, signature))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (signature != IMAGE_NT_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        IMAGE_FILE_HEADER fileHeader{};
        if (!read_remote(baseAddress + dosHeader.e_lfanew + sizeof(DWORD), fileHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        is64Bit = (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 || fileHeader.Machine == IMAGE_FILE_MACHINE_ARM64);

        DWORD importRva = 0;
        if (is64Bit)
        {
            IMAGE_NT_HEADERS64 ntHeaders64{};
            if (!read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders64))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            importRva = ntHeaders64.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        }
        else
        {
            IMAGE_NT_HEADERS32 ntHeaders32{};
            if (!read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders32))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            importRva = ntHeaders32.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT].VirtualAddress;
        }

        ModuleImportCache newCache{};

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
            if (!read_remote(importDescAddr, importDesc))
            {
                break;
            }

            if (importDesc.Name == 0)
            {
                break;
            }

            auto libraryNameOpt = read_remote_string(baseAddress + importDesc.Name);
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
                    if (!read_remote(thunkAddr, thunk))
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
                        if (read_remote(nameAddr, hint))
                        {
                            import.hint = hint;
                        }

                        auto funcNameOpt = read_remote_string(nameAddr + sizeof(uint16_t));
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
                    if (!read_remote(thunkAddr, thunk))
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
                        if (read_remote(nameAddr, hint))
                        {
                            import.hint = hint;
                        }

                        auto funcNameOpt = read_remote_string(nameAddr + sizeof(uint16_t));
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

    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_exports(const ModuleInformation* module, ModuleExport** exports, uint32_t* count)
    {
        if (!module || !exports || !count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const uint64_t baseAddress = module->baseAddress;
        ModuleCache& cache = get_module_cache();

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
        if (!read_remote(baseAddress, dosHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (dosHeader.e_magic != IMAGE_DOS_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        uint32_t signature;
        if (!read_remote(baseAddress + dosHeader.e_lfanew, signature))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        if (signature != IMAGE_NT_SIGNATURE)
        {
            return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
        }

        IMAGE_FILE_HEADER fileHeader{};
        if (!read_remote(baseAddress + dosHeader.e_lfanew + sizeof(DWORD), fileHeader))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        bool is64Bit = (fileHeader.Machine == IMAGE_FILE_MACHINE_AMD64 || fileHeader.Machine == IMAGE_FILE_MACHINE_ARM64);

        DWORD exportRva = 0;
        DWORD exportSize = 0;

        if (is64Bit)
        {
            IMAGE_NT_HEADERS64 ntHeaders{};
            if (!read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }
        else
        {
            IMAGE_NT_HEADERS32 ntHeaders{};
            if (!read_remote(baseAddress + dosHeader.e_lfanew, ntHeaders))
            {
                return StatusCode::STATUS_ERROR_MEMORY_READ;
            }
            exportRva = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            exportSize = ntHeaders.OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].Size;
        }

        ModuleExportCache newCache{};

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
        if (!read_remote(baseAddress + exportRva, exportDir))
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
        if (!read_remote_buffer(baseAddress + exportDir.AddressOfFunctions, functionRvas.data(), numFunctions * sizeof(DWORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::vector<DWORD> nameRvas(numNames);
        if (numNames > 0 && !read_remote_buffer(baseAddress + exportDir.AddressOfNames, nameRvas.data(), numNames * sizeof(DWORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::vector<WORD> ordinals(numNames);
        if (numNames > 0 && !read_remote_buffer(baseAddress + exportDir.AddressOfNameOrdinals, ordinals.data(), numNames * sizeof(WORD)))
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        std::unordered_map<DWORD, std::string> ordinalToName{};
        for (uint32_t i = 0; i < numNames; ++i)
        {
            auto nameOpt = read_remote_string(baseAddress + nameRvas[i]);
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
                auto forwarderOpt = read_remote_string(funcAddr);
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