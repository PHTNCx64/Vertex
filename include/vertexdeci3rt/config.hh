//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <charconv>
#include <filesystem>
#include <fstream>
#include <optional>
#include <string>

namespace DECI3
{
    struct Config final
    {
        int          targetNumber {};
        std::wstring modulePath   {};
        int processId {};

        [[nodiscard]] static std::optional<Config> load(
            const std::filesystem::path& directory = std::filesystem::current_path())
        {
            const std::filesystem::path iniPath = directory / "deci3config.ini";

            std::ifstream file { iniPath };
            if (!file)
            {
                return std::nullopt;
            }

            std::string numberLine {};
            std::string pathLine   {};
            if (!std::getline(file, numberLine) || !std::getline(file, pathLine))
            {
                return std::nullopt;
            }

            Config cfg {};
            auto [ptr, ec] = std::from_chars(
                numberLine.data(),
                numberLine.data() + numberLine.size(),
                cfg.targetNumber);

            if (ec != std::errc{})
            {
                return std::nullopt;
            }

            cfg.modulePath = std::wstring(pathLine.begin(), pathLine.end());
            return cfg;
        }
    };

} // namespace DECI3
