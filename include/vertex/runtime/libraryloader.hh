//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <string_view>

namespace Vertex::Runtime
{
    class LibraryLoader final
    {
      public:
        LibraryLoader() = delete;

        [[nodiscard]] static void* load_library(std::string_view path);
        [[nodiscard]] static bool unload_library(void* handle);
        [[nodiscard]] static void* resolve_address(void* libraryHandle, std::string_view funcName);
    };

}