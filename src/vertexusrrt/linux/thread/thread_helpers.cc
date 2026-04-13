//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <sys/ptrace.h>
#include <sys/user.h>
#include <sys/wait.h>
#include <sys/syscall.h>
#include <signal.h>
#include <unistd.h>
#include <cerrno>

#include <algorithm>
#include <array>
#include <cstdint>
#include <string_view>
#include <unordered_map>

extern ProcessArchitecture get_process_architecture();
extern native_handle& get_native_handle();

namespace ThreadInternal
{
    constexpr std::int32_t LINUX_NICE_TIME_CRITICAL = -20;
    constexpr std::int32_t LINUX_NICE_HIGHEST = -10;
    constexpr std::int32_t LINUX_NICE_ABOVE_NORMAL = -5;
    constexpr std::int32_t LINUX_NICE_NORMAL = 0;
    constexpr std::int32_t LINUX_NICE_BELOW_NORMAL = 5;
    constexpr std::int32_t LINUX_NICE_LOWEST = 10;
    constexpr std::int32_t LINUX_NICE_IDLE = 19;

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
        make_priority_entry(LINUX_NICE_TIME_CRITICAL, "Time Critical"),
        make_priority_entry(LINUX_NICE_HIGHEST, "Highest"),
        make_priority_entry(LINUX_NICE_ABOVE_NORMAL, "Above Normal"),
        make_priority_entry(LINUX_NICE_NORMAL, "Normal"),
        make_priority_entry(LINUX_NICE_BELOW_NORMAL, "Below Normal"),
        make_priority_entry(LINUX_NICE_LOWEST, "Lowest"),
        make_priority_entry(LINUX_NICE_IDLE, "Idle")
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

    void fill_registers_from_user_regs_x86(RegisterSet* registers, const user_regs_struct& regs)
    {
        registers->registerCount = 10;

        fill_register(registers->registers[0], "EAX", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rax), 32);
        fill_register(registers->registers[1], "EBX", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rbx), 32);
        fill_register(registers->registers[2], "ECX", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rcx), 32);
        fill_register(registers->registers[3], "EDX", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rdx), 32);
        fill_register(registers->registers[4], "ESI", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rsi), 32);
        fill_register(registers->registers[5], "EDI", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rdi), 32);
        fill_register(registers->registers[6], "EBP", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rbp), 32);
        fill_register(registers->registers[7], "ESP", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rsp), 32);
        fill_register(registers->registers[8], "EIP", VERTEX_REG_GENERAL, static_cast<std::uint32_t>(regs.rip), 32);
        fill_register(registers->registers[9], "EFLAGS", VERTEX_REG_FLAGS, static_cast<std::uint32_t>(regs.eflags), 32);

        registers->instructionPointer = static_cast<std::uint32_t>(regs.rip);
        registers->stackPointer = static_cast<std::uint32_t>(regs.rsp);
        registers->basePointer = static_cast<std::uint32_t>(regs.rbp);
        registers->flagsRegister = static_cast<std::uint32_t>(regs.eflags);
    }

    void fill_registers_from_user_regs(RegisterSet* registers, const user_regs_struct& regs)
    {
        registers->registerCount = 18;

        fill_register(registers->registers[0], "RAX", VERTEX_REG_GENERAL, regs.rax, 64);
        fill_register(registers->registers[1], "RBX", VERTEX_REG_GENERAL, regs.rbx, 64);
        fill_register(registers->registers[2], "RCX", VERTEX_REG_GENERAL, regs.rcx, 64);
        fill_register(registers->registers[3], "RDX", VERTEX_REG_GENERAL, regs.rdx, 64);
        fill_register(registers->registers[4], "RSI", VERTEX_REG_GENERAL, regs.rsi, 64);
        fill_register(registers->registers[5], "RDI", VERTEX_REG_GENERAL, regs.rdi, 64);
        fill_register(registers->registers[6], "RBP", VERTEX_REG_GENERAL, regs.rbp, 64);
        fill_register(registers->registers[7], "RSP", VERTEX_REG_GENERAL, regs.rsp, 64);
        fill_register(registers->registers[8], "R8", VERTEX_REG_GENERAL, regs.r8, 64);
        fill_register(registers->registers[9], "R9", VERTEX_REG_GENERAL, regs.r9, 64);
        fill_register(registers->registers[10], "R10", VERTEX_REG_GENERAL, regs.r10, 64);
        fill_register(registers->registers[11], "R11", VERTEX_REG_GENERAL, regs.r11, 64);
        fill_register(registers->registers[12], "R12", VERTEX_REG_GENERAL, regs.r12, 64);
        fill_register(registers->registers[13], "R13", VERTEX_REG_GENERAL, regs.r13, 64);
        fill_register(registers->registers[14], "R14", VERTEX_REG_GENERAL, regs.r14, 64);
        fill_register(registers->registers[15], "R15", VERTEX_REG_GENERAL, regs.r15, 64);
        fill_register(registers->registers[16], "RIP", VERTEX_REG_GENERAL, regs.rip, 64);
        fill_register(registers->registers[17], "RFLAGS", VERTEX_REG_FLAGS, regs.eflags, 64);

        registers->instructionPointer = regs.rip;
        registers->stackPointer = regs.rsp;
        registers->basePointer = regs.rbp;
        registers->flagsRegister = regs.eflags;
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
            {"EAX",    {offsetof(user_regs_struct, rax),    sizeof(std::uint32_t)}},
            {"EBX",    {offsetof(user_regs_struct, rbx),    sizeof(std::uint32_t)}},
            {"ECX",    {offsetof(user_regs_struct, rcx),    sizeof(std::uint32_t)}},
            {"EDX",    {offsetof(user_regs_struct, rdx),    sizeof(std::uint32_t)}},
            {"ESI",    {offsetof(user_regs_struct, rsi),    sizeof(std::uint32_t)}},
            {"EDI",    {offsetof(user_regs_struct, rdi),    sizeof(std::uint32_t)}},
            {"EBP",    {offsetof(user_regs_struct, rbp),    sizeof(std::uint32_t)}},
            {"ESP",    {offsetof(user_regs_struct, rsp),    sizeof(std::uint32_t)}},
            {"EIP",    {offsetof(user_regs_struct, rip),    sizeof(std::uint32_t)}},
            {"EFLAGS", {offsetof(user_regs_struct, eflags), sizeof(std::uint32_t)}}
        };
        return map;
    }

    const RegisterMap& get_x64_register_map()
    {
        static const RegisterMap map = {
            {"RAX",    {offsetof(user_regs_struct, rax),    sizeof(std::uint64_t)}},
            {"RBX",    {offsetof(user_regs_struct, rbx),    sizeof(std::uint64_t)}},
            {"RCX",    {offsetof(user_regs_struct, rcx),    sizeof(std::uint64_t)}},
            {"RDX",    {offsetof(user_regs_struct, rdx),    sizeof(std::uint64_t)}},
            {"RSI",    {offsetof(user_regs_struct, rsi),    sizeof(std::uint64_t)}},
            {"RDI",    {offsetof(user_regs_struct, rdi),    sizeof(std::uint64_t)}},
            {"RBP",    {offsetof(user_regs_struct, rbp),    sizeof(std::uint64_t)}},
            {"RSP",    {offsetof(user_regs_struct, rsp),    sizeof(std::uint64_t)}},
            {"R8",     {offsetof(user_regs_struct, r8),     sizeof(std::uint64_t)}},
            {"R9",     {offsetof(user_regs_struct, r9),     sizeof(std::uint64_t)}},
            {"R10",    {offsetof(user_regs_struct, r10),    sizeof(std::uint64_t)}},
            {"R11",    {offsetof(user_regs_struct, r11),    sizeof(std::uint64_t)}},
            {"R12",    {offsetof(user_regs_struct, r12),    sizeof(std::uint64_t)}},
            {"R13",    {offsetof(user_regs_struct, r13),    sizeof(std::uint64_t)}},
            {"R14",    {offsetof(user_regs_struct, r14),    sizeof(std::uint64_t)}},
            {"R15",    {offsetof(user_regs_struct, r15),    sizeof(std::uint64_t)}},
            {"RIP",    {offsetof(user_regs_struct, rip),    sizeof(std::uint64_t)}},
            {"RFLAGS", {offsetof(user_regs_struct, eflags), sizeof(std::uint64_t)}}
        };
        return map;
    }
}

namespace debugger
{
    int suspend_thread(const pid_t tid)
    {
        const pid_t pid = get_native_handle();
        if (pid <= 0)
        {
            return -1;
        }

        if (syscall(SYS_tgkill, pid, tid, SIGSTOP) != 0)
        {
            return -1;
        }

        int status{};
        if (waitpid(tid, &status, __WALL) == -1)
        {
            if (errno != ECHILD)
            {
                return -1;
            }
        }

        return 0;
    }

    int resume_thread(const pid_t tid)
    {
        if (ptrace(PTRACE_CONT, tid, nullptr, nullptr) == 0)
        {
            return 0;
        }

        const pid_t pid = get_native_handle();
        if (pid <= 0)
        {
            return -1;
        }

        if (syscall(SYS_tgkill, pid, tid, SIGCONT) != 0)
        {
            return -1;
        }

        return 0;
    }
}
