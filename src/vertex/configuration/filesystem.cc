//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <filesystem>
#include <nlohmann/json.hpp>
#include <vertex/configuration/filesystem.hh>

#include <fstream>

namespace Vertex::Configuration
{
    static constexpr auto PLUGINS_PATH = "Plugins";
    static constexpr auto CONFIG_PATH = "Configuration";
    static constexpr auto LANG_PATH = "Language";
    static constexpr auto SCRIPT_PATH = "Scripts";
    static constexpr auto CRASH_DUMP_PATH = "Crashdumps";

    StatusCode Filesystem::construct_runtime_filesystem()
    {
        const auto& basePath = get_base_path();
        std::error_code ec{};

        auto makeDir = [&](const std::filesystem::path& path) -> StatusCode
        {
            const bool success = std::filesystem::create_directories(path, ec);
            if (!success)
            {
                return !ec || ec == std::errc::file_exists ? StatusCode::STATUS_OK : StatusCode::STATUS_ERROR_FS_DIR_CREATION_FAILED;
            }

            return StatusCode::STATUS_OK;
        };

        StatusCode status = makeDir(basePath / PLUGINS_PATH);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = makeDir(basePath / CONFIG_PATH);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = makeDir(basePath / LANG_PATH);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = makeDir(basePath / SCRIPT_PATH);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        status = makeDir(basePath / CRASH_DUMP_PATH);
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        return StatusCode::STATUS_OK;
    }

    std::filesystem::path Filesystem::get_configuration_path()
    {
        return get_base_path() / CONFIG_PATH;
    }

    std::filesystem::path Filesystem::get_language_path()
    {
        return get_base_path() / LANG_PATH;
    }

    std::filesystem::path Filesystem::get_plugin_path()
    {
        return get_base_path() / PLUGINS_PATH;
    }

    std::filesystem::path Filesystem::get_script_path()
    {
        return get_base_path() / SCRIPT_PATH;
    }

    std::filesystem::path Filesystem::get_crash_dump_path()
    {
        return get_base_path() / CRASH_DUMP_PATH;
    }

    std::filesystem::path Filesystem::resolve_path(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_absolute())
        {
            return path;
        }
        return get_base_path() / path;
    }

    std::filesystem::path Filesystem::make_relative(const std::filesystem::path& path)
    {
        if (path.empty() || path.is_relative())
        {
            return path;
        }

        const auto basePath = get_base_path();
        auto [baseIt, pathIt] = std::mismatch(basePath.begin(), basePath.end(), path.begin(), path.end());

        if (baseIt == basePath.end())
        {
            std::filesystem::path relative{};
            for (; pathIt != path.end(); ++pathIt)
            {
                relative /= *pathIt;
            }
            return relative;
        }

        return path;
    }

    StatusCode write_configuration_file(const std::filesystem::path& filePath, const nlohmann::json& settings)
    {
        std::filesystem::path absolutePath{};

        if (filePath.is_relative())
        {
            absolutePath = Filesystem::get_configuration_path() / filePath;
        }
        else
        {
            absolutePath = filePath;
        }

        if (std::filesystem::exists(absolutePath) && std::filesystem::is_directory(absolutePath))
        {
            return StatusCode::STATUS_ERROR_FS_UNEXPECTED_FILE_TYPE;
        }

        const auto parentPath = absolutePath.parent_path();
        if (!parentPath.empty() && !std::filesystem::exists(parentPath))
        {
            std::error_code ec{};
            const bool created = std::filesystem::create_directories(parentPath, ec);
            if (!created || ec)
            {
                return StatusCode::STATUS_ERROR_FS_DIR_CREATION_FAILED;
            }
        }

        std::ofstream file(absolutePath, std::ios::out | std::ios::trunc);
        if (!file.is_open())
        {
            return StatusCode::STATUS_ERROR_FS_FILE_OPEN_FAILED;
        }

        try
        {
            file << settings.dump(4);

            if (file.fail())
            {
                file.close();
                return StatusCode::STATUS_ERROR_FS_FILE_WRITE_FAILED;
            }

            file.close();

            if (file.fail())
            {
                return StatusCode::STATUS_ERROR_FS_FILE_WRITE_FAILED;
            }

            return StatusCode::STATUS_OK;
        }
        catch (const nlohmann::json::exception&)
        {
            file.close();
            return StatusCode::STATUS_ERROR_JSON_SERIALIZATION_FAILED;
        }
        catch (const std::ios_base::failure&)
        {
            file.close();
            return StatusCode::STATUS_ERROR_FS_FILE_WRITE_FAILED;
        }
        catch (const std::exception&)
        {
            file.close();
            return StatusCode::STATUS_ERROR_FS_FILE_WRITE_FAILED;
        }
    }
}
