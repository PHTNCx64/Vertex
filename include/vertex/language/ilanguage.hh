//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <filesystem>
#include <string>
#include <string_view>
#include <unordered_map>
#include <sdk/statuscode.h>

namespace Vertex::Language
{
    class ILanguage
    {
    public:
        virtual ~ILanguage() = default;
        virtual StatusCode load_translation(const std::filesystem::path& path) = 0;
        [[nodiscard]] virtual const std::string& fetch_translation(std::string_view path) = 0;
        [[nodiscard]] virtual std::unordered_map<std::string, std::filesystem::path> fetch_all_languages() = 0;
        [[nodiscard]] virtual StatusCode is_active_language(const std::filesystem::path& path) const noexcept = 0;
    };
}
