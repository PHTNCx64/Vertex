//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/statuscode.h>
#include <filesystem>

namespace Vertex::Configuration
{
    class Filesystem final
    {
    public:
        [[nodiscard]] static StatusCode construct_runtime_filesystem();

        [[nodiscard]] static std::filesystem::path get_configuration_path();
        [[nodiscard]] static std::filesystem::path get_language_path();
        [[nodiscard]] static std::filesystem::path get_plugin_path();
        [[nodiscard]] static std::filesystem::path get_script_path();
        [[nodiscard]] static std::filesystem::path get_crash_dump_path();

        [[nodiscard]] static std::filesystem::path resolve_path(const std::filesystem::path& path);
        [[nodiscard]] static std::filesystem::path make_relative(const std::filesystem::path& path);

    private:
        [[nodiscard]] static std::filesystem::path get_base_path();
    };
}
