//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/configuration/filesystem.hh>

#include <array>
#include <windows.h>

namespace Vertex::Configuration
{
    std::filesystem::path Filesystem::get_base_path()
    {
        std::array<wchar_t, MAX_PATH> buffer{};
        const auto length = GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
        if (length > 0 && length < buffer.size())
        {
            return std::filesystem::path{buffer.data()}.parent_path();
        }
        return std::filesystem::current_path();
    }
}
