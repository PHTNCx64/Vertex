//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/configuration/filesystem.hh>

#include <system_error>

namespace Vertex::Configuration
{
    std::filesystem::path Filesystem::get_base_path()
    {
        std::error_code ec{};
        const auto exePath = std::filesystem::read_symlink("/proc/self/exe", ec);
        if (!ec)
        {
            return exePath.parent_path();
        }
        return std::filesystem::current_path();
    }
}
