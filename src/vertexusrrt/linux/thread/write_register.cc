//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <sys/ptrace.h>
#include <sys/user.h>

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
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_write_register(const std::uint32_t threadId, const char* name, const void* in, const std::size_t size)
    {
        if (!name || !in || size == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const ProcessArchitecture arch = get_process_architecture();
        const ThreadInternal::RegisterMap* regMapPtr = nullptr;

        if (arch == ProcessArchitecture::X86)
        {
            regMapPtr = &ThreadInternal::get_x86_register_map();
        }
        else if (arch == ProcessArchitecture::X86_64)
        {
            regMapPtr = &ThreadInternal::get_x64_register_map();
        }
        else
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        const auto it = regMapPtr->find(name);
        if (it == regMapPtr->end())
        {
            return StatusCode::STATUS_ERROR_REGISTER_NOT_FOUND;
        }

        user_regs_struct regs{};
        if (ptrace(PTRACE_GETREGS, static_cast<pid_t>(threadId), nullptr, &regs) != 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
        }

        auto* destPtr = reinterpret_cast<std::uint8_t*>(&regs) + it->second.offset;
        const std::size_t copySize = std::min(size, static_cast<std::size_t>(it->second.size));
        std::memcpy(destPtr, in, copySize);

        if (ptrace(PTRACE_SETREGS, static_cast<pid_t>(threadId), nullptr, &regs) != 0)
        {
            return StatusCode::STATUS_ERROR_REGISTER_WRITE_FAILED;
        }

        return StatusCode::STATUS_OK;
    }
}
