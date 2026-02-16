//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>
#include <sdk/process.h>

#include <Windows.h>

#include <algorithm>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

native_handle& get_native_handle();
extern "C" void clear_module_cache();

namespace ProcessInternal
{
    struct ModuleImportCache final
    {
        std::vector<ModuleImport> imports{};
        std::vector<std::string> stringStorage{};
    };

    struct ModuleExportCache final
    {
        std::vector<ModuleExport> exports{};
        std::vector<std::string> stringStorage{};
    };

    struct ModuleCache final
    {
        std::unordered_map<std::uint64_t, ModuleImportCache> importCache{};
        std::unordered_map<std::uint64_t, ModuleExportCache> exportCache{};
        std::mutex cacheMutex;
    };

    ModuleCache& get_module_cache();
    ProcessInformation* opened_process_info();
    StatusCode invalidate_handle();

    template <class T>
    bool read_remote(const std::uint64_t address, T& out)
    {
        return vertex_memory_read_process(address, sizeof(T), reinterpret_cast<char*>(&out)) == STATUS_OK;
    }

    inline bool read_remote_buffer(const std::uint64_t address, void* buffer, const std::size_t size)
    {
        return vertex_memory_read_process(address, size, static_cast<char*>(buffer)) == STATUS_OK;
    }

    inline std::optional<std::string> read_remote_string(const std::uint64_t address, std::size_t maxLen = 256)
    {
        std::string result;
        result.reserve(maxLen);

        char c;
        for (std::size_t i = 0; i < maxLen; ++i)
        {
            if (!read_remote(address + i, c))
            {
                return std::nullopt;
            }
            if (c == '\0')
            {
                break;
            }
            result.push_back(c);
        }

        return result;
    }

    inline std::optional<std::string> wchar_to_utf8(const WCHAR* str) noexcept
    {
        if (!str)
        {
            return std::nullopt;
        }

        const int len = WideCharToMultiByte(CP_UTF8, 0, str, -1, nullptr, 0, nullptr, nullptr);
        if (len <= 0)
        {
            return std::nullopt;
        }

        std::string utf8_str(len - 1, '\0');
        const int result = WideCharToMultiByte(CP_UTF8, 0, str, -1, utf8_str.data(), len, nullptr, nullptr);
        return (result > 0) ? std::make_optional(std::move(utf8_str)) : std::nullopt;
    }

    inline std::optional<std::wstring> utf8_to_wchar(const char* utf8Str) noexcept
    {
        if (!utf8Str)
        {
            return std::nullopt;
        }

        const int len = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, nullptr, 0);
        if (len <= 0)
        {
            return std::nullopt;
        }

        std::wstring wide_str(len - 1, L'\0');
        const int result = MultiByteToWideChar(CP_UTF8, 0, utf8Str, -1, wide_str.data(), len);

        return (result > 0) ? std::make_optional(std::move(wide_str)) : std::nullopt;
    }

    inline void vertex_cpy(char* dst, const std::string_view src, const std::size_t max_len)
    {
        if (!dst || max_len == 0)
        {
            return;
        }

        const std::size_t cpy_len = std::min(src.length(), max_len - 1);
        std::copy_n(src.data(), cpy_len, dst);
        dst[cpy_len] = '\0';
    }
}
