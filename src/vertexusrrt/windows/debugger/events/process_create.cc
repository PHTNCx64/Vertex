//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/native_handle.hh>
#include <Windows.h>
#include <Psapi.h>

extern native_handle& get_native_handle();
namespace debugger
{
    namespace
    {
        std::uint64_t query_module_size(const std::uint64_t baseAddress)
        {
            MODULEINFO modInfo{};
            const auto hModule = reinterpret_cast<HMODULE>(baseAddress);
            if (GetModuleInformation(get_native_handle(), hModule, &modInfo, sizeof(modInfo)))
            {
                return modInfo.SizeOfImage;
            }

            MEMORY_BASIC_INFORMATION mbi{};
            if (VirtualQueryEx(get_native_handle(), reinterpret_cast<LPCVOID>(baseAddress), &mbi, sizeof(mbi)) != 0)
            {
                return mbi.RegionSize;
            }

            return 0;
        }

        void fill_module_paths(HANDLE hFile, char* outName, char* outPath)
        {
            std::array<wchar_t, VERTEX_MAX_PATH_LENGTH> wbuf{};
            const DWORD len = GetFinalPathNameByHandleW(hFile, wbuf.data(),
                static_cast<DWORD>(wbuf.size() - 1), FILE_NAME_NORMALIZED);
            if (len == 0 || len >= wbuf.size())
            {
                return;
            }

            const std::wstring_view wpath{wbuf.data(), len};
            const int pathBytes = WideCharToMultiByte(CP_UTF8, 0, wpath.data(),
                static_cast<int>(wpath.size()), outPath,
                VERTEX_MAX_PATH_LENGTH - 1, nullptr, nullptr);
            outPath[pathBytes > 0 ? pathBytes : 0] = '\0';

            const auto sep = wpath.rfind(L'\\');
            const std::wstring_view wname = sep != std::wstring_view::npos
                ? wpath.substr(sep + 1) : wpath;
            const int nameBytes = WideCharToMultiByte(CP_UTF8, 0, wname.data(),
                static_cast<int>(wname.size()), outName,
                VERTEX_MAX_NAME_LENGTH - 1, nullptr, nullptr);
            outName[nameBytes > 0 ? nameBytes : 0] = '\0';
        }
    }

    TickEventResult handle_create_process(TickState& state, const DEBUG_EVENT& event)
    {
        cache_thread_handle(event.dwThreadId);

        state.attachedProcessId = event.dwProcessId;
        state.currentThreadId = event.dwThreadId;
        {
            std::scoped_lock lock{state.callbackMutex};
            if (state.callbacks.has_value())
            {
                const auto& cb = state.callbacks.value();
                if (cb.on_module_loaded != nullptr)
                {
                    const auto& info = event.u.CreateProcessInfo;
                    ModuleEvent moduleEvent{};
                    moduleEvent.baseAddress = reinterpret_cast<std::uint64_t>(info.lpBaseOfImage);
                    moduleEvent.size = query_module_size(moduleEvent.baseAddress);
                    moduleEvent.isMainModule = 1;
                    if (info.hFile != nullptr)
                    {
                        fill_module_paths(info.hFile, moduleEvent.moduleName, moduleEvent.modulePath);
                    }
                    cb.on_module_loaded(&moduleEvent, cb.user_data);
                }
            }
        }
        if (event.u.CreateProcessInfo.hFile != nullptr)
        {
            CloseHandle(event.u.CreateProcessInfo.hFile);
        }
        if (event.u.CreateProcessInfo.hProcess != nullptr)
        {
            CloseHandle(event.u.CreateProcessInfo.hProcess);
        }
        if (event.u.CreateProcessInfo.hThread != nullptr)
        {
            CloseHandle(event.u.CreateProcessInfo.hThread);
        }
        return TickEventResult{.continueStatus = DBG_CONTINUE};
    }
}
