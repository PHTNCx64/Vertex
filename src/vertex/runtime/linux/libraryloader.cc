//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/libraryloader.hh>

#include <dlfcn.h>
#include <ostream>

#include <string>

namespace Vertex::Runtime
{

    void* LibraryLoader::load_library(const std::string_view path)
    {
        const std::string nullTerminatedPath{path};
        dlerror();
        void* handle = dlopen(nullTerminatedPath.c_str(), RTLD_LAZY);
        return handle;
    }

    std::string LibraryLoader::last_error()
    {
        const char* err = dlerror();
        return err ? std::string{err} : std::string{};
    }

    bool LibraryLoader::unload_library(void* handle)
    {
        if (!handle)
        {
            return false;
        }

        return dlclose(handle) == 0;
    }

    void* LibraryLoader::resolve_address(void* libraryHandle, const std::string_view funcName)
    {
        if (!libraryHandle)
        {
            return nullptr;
        }

        const std::string nullTerminatedName{funcName};
        return dlsym(libraryHandle, nullTerminatedName.c_str());
    }

}
