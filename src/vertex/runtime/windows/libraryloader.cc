//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/libraryloader.hh>

#include <windows.h>
#include <string>
#include <fmt/format.h>

namespace Vertex::Runtime
{

    void* LibraryLoader::load_library(const std::string_view path)
    {
        const std::string nullTerminatedPath{path};
        return LoadLibraryA(nullTerminatedPath.c_str());
    }

    std::string LibraryLoader::last_error()
    {
        const DWORD code = GetLastError();
        if (code == 0)
        {
            return {};
        }

        LPSTR buffer{};
        const DWORD size = FormatMessageA(
            FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
            nullptr,
            code,
            MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
            reinterpret_cast<LPSTR>(&buffer),
            0,
            nullptr);

        std::string message;
        if (size && buffer)
        {
            message.assign(buffer, size);
            while (!message.empty() && (message.back() == '\r' || message.back() == '\n' || message.back() == ' '))
            {
                message.pop_back();
            }
        }
        if (buffer)
        {
            LocalFree(buffer);
        }

        return fmt::format("({}) {}", code, message);
    }

    bool LibraryLoader::unload_library(void* handle)
    {
        if (!handle)
        {
            return false;
        }

        return FreeLibrary(static_cast<HMODULE>(handle)) != 0;
    }

    void* LibraryLoader::resolve_address(void* libraryHandle, const std::string_view funcName)
    {
        if (!libraryHandle)
        {
            return nullptr;
        }

        const std::string nullTerminatedName{funcName};
        return reinterpret_cast<void*>(GetProcAddress(static_cast<HMODULE>(libraryHandle), nullTerminatedName.c_str()));
    }

}
