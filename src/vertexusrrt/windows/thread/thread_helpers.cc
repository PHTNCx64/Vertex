//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <Windows.h>
#include <algorithm>
#include <array>
#include <string_view>
#include <unordered_map>

extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    struct PriorityEntry final
    {
        std::int32_t priority;
        std::string_view text;
        std::size_t size;
    };

    consteval PriorityEntry make_priority_entry(const int32_t priority, const std::string_view text)
    {
        return PriorityEntry{ priority, text, text.size() + 1U };
    }

    constexpr auto k_priority_entries = std::array{
        make_priority_entry(THREAD_PRIORITY_LOWEST, "Lowest"),
        make_priority_entry(THREAD_PRIORITY_BELOW_NORMAL, "Below Normal"),
        make_priority_entry(THREAD_PRIORITY_NORMAL, "Normal"),
        make_priority_entry(THREAD_PRIORITY_ABOVE_NORMAL, "Above Normal"),
        make_priority_entry(THREAD_PRIORITY_HIGHEST, "Highest"),
        make_priority_entry(THREAD_PRIORITY_TIME_CRITICAL, "Time Critical"),
        make_priority_entry(THREAD_PRIORITY_IDLE, "Idle")
    };

    consteval PriorityEntry make_special_entry(const std::string_view text)
    {
        return PriorityEntry{ 0, text, text.size() + 1U };
    }

    constexpr auto k_custom_priority = make_special_entry("Custom");
    constexpr auto k_invalid_priority = make_special_entry("Invalid Priority");

    ThreadList* get_thread_list()
    {
        static ThreadList threadList;
        return &threadList;
    }

    void set_register_name(char* dest, const std::string_view src) noexcept
    {
        constexpr std::size_t maxLen = sizeof(::Register::name) - 1;
        const auto copyLen = std::min(src.length(), maxLen);
        std::copy_n(src.data(), copyLen, dest);
        dest[copyLen] = '\0';
    }

    void fill_register(Register& reg, const char* name, RegisterCategory category, const std::uint64_t value, const std::uint8_t bitWidth)
    {
        set_register_name(reg.name, name);
        reg.category = category;
        reg.value = value;
        reg.previousValue = 0;
        reg.bitWidth = bitWidth;
        reg.modified = 0;
    }

    void fill_registers_from_wow64_context(RegisterSet* registers, const WOW64_CONTEXT& ctx)
    {
        registers->registerCount = 10;

        fill_register(registers->registers[0], "EAX", VERTEX_REG_GENERAL, ctx.Eax, 32);
        fill_register(registers->registers[1], "EBX", VERTEX_REG_GENERAL, ctx.Ebx, 32);
        fill_register(registers->registers[2], "ECX", VERTEX_REG_GENERAL, ctx.Ecx, 32);
        fill_register(registers->registers[3], "EDX", VERTEX_REG_GENERAL, ctx.Edx, 32);
        fill_register(registers->registers[4], "ESI", VERTEX_REG_GENERAL, ctx.Esi, 32);
        fill_register(registers->registers[5], "EDI", VERTEX_REG_GENERAL, ctx.Edi, 32);
        fill_register(registers->registers[6], "EBP", VERTEX_REG_GENERAL, ctx.Ebp, 32);
        fill_register(registers->registers[7], "ESP", VERTEX_REG_GENERAL, ctx.Esp, 32);
        fill_register(registers->registers[8], "EIP", VERTEX_REG_GENERAL, ctx.Eip, 32);
        fill_register(registers->registers[9], "EFLAGS", VERTEX_REG_FLAGS, ctx.EFlags, 32);

        registers->instructionPointer = ctx.Eip;
        registers->stackPointer = ctx.Esp;
        registers->basePointer = ctx.Ebp;
        registers->flagsRegister = ctx.EFlags;
    }

    void fill_registers_from_context(RegisterSet* registers, const CONTEXT& ctx)
    {
        registers->registerCount = 18;

        fill_register(registers->registers[0], "RAX", VERTEX_REG_GENERAL, ctx.Rax, 64);
        fill_register(registers->registers[1], "RBX", VERTEX_REG_GENERAL, ctx.Rbx, 64);
        fill_register(registers->registers[2], "RCX", VERTEX_REG_GENERAL, ctx.Rcx, 64);
        fill_register(registers->registers[3], "RDX", VERTEX_REG_GENERAL, ctx.Rdx, 64);
        fill_register(registers->registers[4], "RSI", VERTEX_REG_GENERAL, ctx.Rsi, 64);
        fill_register(registers->registers[5], "RDI", VERTEX_REG_GENERAL, ctx.Rdi, 64);
        fill_register(registers->registers[6], "RBP", VERTEX_REG_GENERAL, ctx.Rbp, 64);
        fill_register(registers->registers[7], "RSP", VERTEX_REG_GENERAL, ctx.Rsp, 64);
        fill_register(registers->registers[8], "R8", VERTEX_REG_GENERAL, ctx.R8, 64);
        fill_register(registers->registers[9], "R9", VERTEX_REG_GENERAL, ctx.R9, 64);
        fill_register(registers->registers[10], "R10", VERTEX_REG_GENERAL, ctx.R10, 64);
        fill_register(registers->registers[11], "R11", VERTEX_REG_GENERAL, ctx.R11, 64);
        fill_register(registers->registers[12], "R12", VERTEX_REG_GENERAL, ctx.R12, 64);
        fill_register(registers->registers[13], "R13", VERTEX_REG_GENERAL, ctx.R13, 64);
        fill_register(registers->registers[14], "R14", VERTEX_REG_GENERAL, ctx.R14, 64);
        fill_register(registers->registers[15], "R15", VERTEX_REG_GENERAL, ctx.R15, 64);
        fill_register(registers->registers[16], "RIP", VERTEX_REG_GENERAL, ctx.Rip, 64);
        fill_register(registers->registers[17], "RFLAGS", VERTEX_REG_FLAGS, ctx.EFlags, 64);

        registers->instructionPointer = ctx.Rip;
        registers->stackPointer = ctx.Rsp;
        registers->basePointer = ctx.Rbp;
        registers->flagsRegister = ctx.EFlags;
    }

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

    const RegisterMap& get_x86_register_map()
    {
        static const RegisterMap map = {
            {"EAX",    {offsetof(WOW64_CONTEXT, Eax),    sizeof(DWORD)}},
            {"EBX",    {offsetof(WOW64_CONTEXT, Ebx),    sizeof(DWORD)}},
            {"ECX",    {offsetof(WOW64_CONTEXT, Ecx),    sizeof(DWORD)}},
            {"EDX",    {offsetof(WOW64_CONTEXT, Edx),    sizeof(DWORD)}},
            {"ESI",    {offsetof(WOW64_CONTEXT, Esi),    sizeof(DWORD)}},
            {"EDI",    {offsetof(WOW64_CONTEXT, Edi),    sizeof(DWORD)}},
            {"EBP",    {offsetof(WOW64_CONTEXT, Ebp),    sizeof(DWORD)}},
            {"ESP",    {offsetof(WOW64_CONTEXT, Esp),    sizeof(DWORD)}},
            {"EIP",    {offsetof(WOW64_CONTEXT, Eip),    sizeof(DWORD)}},
            {"EFLAGS", {offsetof(WOW64_CONTEXT, EFlags), sizeof(DWORD)}}
        };
        return map;
    }

    const RegisterMap& get_x64_register_map()
    {
        static const RegisterMap map = {
            {"RAX",    {offsetof(CONTEXT, Rax),    sizeof(DWORD64)}},
            {"RBX",    {offsetof(CONTEXT, Rbx),    sizeof(DWORD64)}},
            {"RCX",    {offsetof(CONTEXT, Rcx),    sizeof(DWORD64)}},
            {"RDX",    {offsetof(CONTEXT, Rdx),    sizeof(DWORD64)}},
            {"RSI",    {offsetof(CONTEXT, Rsi),    sizeof(DWORD64)}},
            {"RDI",    {offsetof(CONTEXT, Rdi),    sizeof(DWORD64)}},
            {"RBP",    {offsetof(CONTEXT, Rbp),    sizeof(DWORD64)}},
            {"RSP",    {offsetof(CONTEXT, Rsp),    sizeof(DWORD64)}},
            {"R8",     {offsetof(CONTEXT, R8),     sizeof(DWORD64)}},
            {"R9",     {offsetof(CONTEXT, R9),     sizeof(DWORD64)}},
            {"R10",    {offsetof(CONTEXT, R10),    sizeof(DWORD64)}},
            {"R11",    {offsetof(CONTEXT, R11),    sizeof(DWORD64)}},
            {"R12",    {offsetof(CONTEXT, R12),    sizeof(DWORD64)}},
            {"R13",    {offsetof(CONTEXT, R13),    sizeof(DWORD64)}},
            {"R14",    {offsetof(CONTEXT, R14),    sizeof(DWORD64)}},
            {"R15",    {offsetof(CONTEXT, R15),    sizeof(DWORD64)}},
            {"RIP",    {offsetof(CONTEXT, Rip),    sizeof(DWORD64)}},
            {"RFLAGS", {offsetof(CONTEXT, EFlags), sizeof(DWORD)}}
        };
        return map;
    }
}

namespace debugger
{
    DWORD suspend_thread(const HANDLE hThread)
    {
        if (hThread == GetCurrentThread())
        {
            return 0;
        }

        const auto arch = get_process_architecture();
        if (arch == ProcessArchitecture::X86)
        {
            return Wow64SuspendThread(hThread);
        }
        return SuspendThread(hThread);
    }

    DWORD resume_thread(const HANDLE hThread)
    {
        if (hThread == GetCurrentThread())
        {
            return 0;
        }

        return ResumeThread(hThread);
    }
}
