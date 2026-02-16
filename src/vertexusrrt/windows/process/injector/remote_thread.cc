//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <filesystem>
#include <memory>

#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

extern native_handle& get_native_handle();
extern Runtime* g_pluginRuntime;
extern std::optional<ProcessArchitecture> detect_dll_architecture(const std::filesystem::path& dllPath);
extern ProcessArchitecture get_process_architecture();

StatusCode remote_thread_inject(const char* path)
{
    const StatusCode status = vertex_process_is_valid();
    if (status != StatusCode::STATUS_OK)
    {
        g_pluginRuntime->vertex_log_error("Process is not valid!");
        return status;
    }

    const auto dllPath = std::filesystem::absolute(path);
    if (std::filesystem::is_directory(dllPath))
    {
        g_pluginRuntime->vertex_log_error("Specified path is a directory, expected a DLL file: %s", dllPath.string().c_str());
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    const auto dllArch = detect_dll_architecture(dllPath);
    if (!dllArch.has_value())
    {
        g_pluginRuntime->vertex_log_error("Failed to detect dll architecture! %s", dllPath.string().c_str());
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    const auto processArch = get_process_architecture();
    if (processArch != dllArch.value())
    {
        g_pluginRuntime->vertex_log_error("DLL architecture (%s) does not match process architecture (%s)!", dllPath.string().c_str(),
                                           processArch == ProcessArchitecture::X86 ? "x86" :
                                           processArch == ProcessArchitecture::X86_64 ? "x86_64" :
                                           processArch == ProcessArchitecture::ARM64 ? "ARM64" : "Unknown");
        return StatusCode::STATUS_ERROR_LIBRARY_INVALID;
    }


    const auto dllPathStr = dllPath.c_str();
    const auto pathByteSize = (wcslen(dllPathStr) + 1) * sizeof(wchar_t);

    const auto remoteMemDeleter = [handle = get_native_handle()](void* ptr)
    {
        if (ptr)
        {
            if (!VirtualFreeEx(handle, ptr, 0, MEM_RELEASE))
            {
                g_pluginRuntime->vertex_log_error("VirtualFreeEx failed on the target! %d", GetLastError());
            }
        }
    };

    const auto remoteAlloc = std::unique_ptr<void, decltype(remoteMemDeleter)>{
        VirtualAllocEx(get_native_handle(), nullptr, pathByteSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE),
        remoteMemDeleter
    };

    if (!remoteAlloc)
    {
        g_pluginRuntime->vertex_log_error("VirtualAllocEx failed on the target! %d", GetLastError());
        return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
    }

    if (!WriteProcessMemory(get_native_handle(), remoteAlloc.get(), dllPathStr, pathByteSize, nullptr))
    {
        g_pluginRuntime->vertex_log_error("WriteProcessMemory failed on the target! %d", GetLastError());
        return StatusCode::STATUS_ERROR_MEMORY_WRITE_FAILED;
    }

    const auto loadLibraryAddr = reinterpret_cast<LPTHREAD_START_ROUTINE>(GetProcAddress(GetModuleHandle(TEXT("kernel32.dll")), "LoadLibraryW"));
    if (!loadLibraryAddr)
    {
        g_pluginRuntime->vertex_log_error("LoadLibraryW could not be located. Are you running some ancient Windows version?! %d", GetLastError());
        return StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND;
    }

    const auto remoteThread = std::unique_ptr<void, decltype(&CloseHandle)>{
        CreateRemoteThread(get_native_handle(), nullptr, 0, loadLibraryAddr, remoteAlloc.get(), 0, nullptr),
        &CloseHandle
    };

    if (!remoteThread)
    {
        g_pluginRuntime->vertex_log_error("CreateRemoteThread failed on the target! %d", GetLastError());
        return StatusCode::STATUS_ERROR_GENERAL;
    }

    WaitForSingleObject(remoteThread.get(), 5000);

    return StatusCode::STATUS_OK;
}