//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <Windows.h>
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>
#include <unordered_map>

extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    struct RegisterInfo final
    {
        std::size_t offset;
        std::uint8_t size;
    };

    struct StringHash
    {
        [[nodiscard]] std::size_t operator()(const std::string_view sv) const noexcept
        {
            return std::hash<std::string_view>{}(sv);
        }

        [[nodiscard]] std::size_t operator()(const char* str) const noexcept
        {
            return std::hash<std::string_view>{}(std::string_view{str});
        }
    };

    struct StringEqual final
    {
        [[nodiscard]] bool operator()(const std::string_view lhs, const std::string_view rhs) const noexcept
        {
            return lhs == rhs;
        }
    };

    using RegisterMap = std::unordered_map<std::string_view, RegisterInfo, StringHash, StringEqual>;

    const RegisterMap& get_x86_register_map();
    const RegisterMap& get_x64_register_map();
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_read_register(uint32_t threadId, const char* name, void* out, const std::size_t size)
    {
        if (!name || !out || size == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE hThread = OpenThread(THREAD_GET_CONTEXT, FALSE, threadId);
        if (!hThread)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        const ProcessArchitecture arch = get_process_architecture();

        if (arch == ProcessArchitecture::X86)
        {
            const auto& regMap = ThreadInternal::get_x86_register_map();
            const auto it = regMap.find(name);
            if (it == regMap.end())
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_REGISTER_NOT_FOUND;
            }

            WOW64_CONTEXT ctx{};
            ctx.ContextFlags = WOW64_CONTEXT_FULL;

            if (!Wow64GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
            }

            const auto* valuePtr = reinterpret_cast<const std::uint8_t*>(&ctx) + it->second.offset;
            const std::size_t copySize = std::min(size, static_cast<std::size_t>(it->second.size));
            std::memcpy(out, valuePtr, copySize);

            CloseHandle(hThread);
            return StatusCode::STATUS_OK;
        }

        if (arch == ProcessArchitecture::X86_64)
        {
            const auto& regMap = ThreadInternal::get_x64_register_map();
            const auto it = regMap.find(name);
            if (it == regMap.end())
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_REGISTER_NOT_FOUND;
            }

            alignas(16) CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER;

            if (!GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
            }

            const auto* valuePtr = reinterpret_cast<const std::uint8_t*>(&ctx) + it->second.offset;
            const std::size_t copySize = std::min(size, static_cast<std::size_t>(it->second.size));
            std::memcpy(out, valuePtr, copySize);

            CloseHandle(hThread);
            return StatusCode::STATUS_OK;
        }

        CloseHandle(hThread);
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }
}
