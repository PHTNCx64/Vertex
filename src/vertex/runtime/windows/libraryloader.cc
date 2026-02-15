//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/libraryloader.hh>

#include <Windows.h>
#include <string>

namespace Vertex::Runtime
{

    void* LibraryLoader::load_library(const std::string_view path)
    {
        const std::string nullTerminatedPath{path};
        return LoadLibraryA(nullTerminatedPath.c_str());
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
