//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugger_internal.hh>

#include <sdk/api.h>

#include <Windows.h>
#include <algorithm>
#include <array>
#include <ranges>
#include <tlhelp32.h>
#include <unordered_map>

#include <string_view>

extern native_handle& get_native_handle();
extern ProcessArchitecture get_process_architecture();
extern std::uint32_t get_current_debug_thread_id();

namespace
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
} // namespace

namespace debugger
{
    DWORD suspend_thread(const HANDLE hThread)
    {
        if (hThread == GetCurrentThread())
        {
            return 0;
        }

        const ProcessArchitecture arch = get_process_architecture();
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
} // namespace debugger

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_threads(ThreadList* threadList)
    {
        if (!threadList)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const native_handle& processHandle = get_native_handle();
        if (!processHandle)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        const DWORD processId = GetProcessId(processHandle);
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_OPEN_INVALID;
        }

        ThreadList* internalList = get_thread_list();
        internalList->threadCount = 0;
        internalList->currentThreadId = 0;

        const HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (threadSnapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        THREADENTRY32 threadEntry{};
        threadEntry.dwSize = sizeof(THREADENTRY32);

        if (!Thread32First(threadSnapshot, &threadEntry))
        {
            CloseHandle(threadSnapshot);
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        do
        {
            if (threadEntry.th32OwnerProcessID != processId)
            {
                continue;
            }

            if (internalList->threadCount >= VERTEX_MAX_THREADS)
            {
                break;
            }
            const HANDLE hThread = OpenThread(
              THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);

            if (!hThread)
            {
                continue;
            }

            auto& [id, name, state, instructionPointer, stackPointer, entryPoint, priority, isCurrent] = internalList->threads[internalList->threadCount];
            id = threadEntry.th32ThreadID;
            name[0] = '\0';
            priority = GetThreadPriority(hThread);
            isCurrent = 0;
            entryPoint = 0;
            instructionPointer = 0;
            stackPointer = 0;
            state = VERTEX_THREAD_RUNNING;

            const DWORD suspendCount = debugger::suspend_thread(hThread);
            if (suspendCount != static_cast<DWORD>(-1))
            {
                if (suspendCount > 0)
                {
                    state = VERTEX_THREAD_SUSPENDED;
                }

                const ProcessArchitecture arch = get_process_architecture();
                if (arch == ProcessArchitecture::X86)
                {
                    WOW64_CONTEXT ctx{};
                    ctx.ContextFlags = WOW64_CONTEXT_CONTROL;

                    if (Wow64GetThreadContext(hThread, &ctx))
                    {
                        instructionPointer = ctx.Eip;
                        stackPointer = ctx.Esp;
                    }
                }
                else if (arch == ProcessArchitecture::X86_64)
                {
                    alignas(16) CONTEXT ctx{};
                    ctx.ContextFlags = CONTEXT_CONTROL;

                    if (GetThreadContext(hThread, &ctx))
                    {
                        instructionPointer = ctx.Rip;
                        stackPointer = ctx.Rsp;
                    }
                }

                debugger::resume_thread(hThread);
            }

            CloseHandle(hThread);
            internalList->threadCount++;

        } while (Thread32Next(threadSnapshot, &threadEntry));

        CloseHandle(threadSnapshot);

        std::memcpy(threadList, internalList, sizeof(ThreadList));

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_current_thread(const uint32_t* threadId)
    {
        if (threadId == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t currentId = get_current_debug_thread_id();
        if (currentId == 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
        }

        *const_cast<std::uint32_t*>(threadId) = currentId;
        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_suspend_thread(const uint32_t threadId)
    {
        const HANDLE hThread = OpenThread(
          THREAD_SUSPEND_RESUME, FALSE, threadId);

        if (!hThread)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        const DWORD result = debugger::suspend_thread(hThread);
        CloseHandle(hThread);

        if (result == static_cast<DWORD>(-1))
        {
            return StatusCode::STATUS_ERROR_THREAD_SUSPEND_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_resume_thread(const uint32_t threadId)
    {
        const HANDLE hThread = OpenThread(
          THREAD_SUSPEND_RESUME, FALSE, threadId);

        if (!hThread)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        const DWORD result = debugger::resume_thread(hThread);
        CloseHandle(hThread);

        if (result == static_cast<DWORD>(-1))
        {
            return StatusCode::STATUS_ERROR_THREAD_RESUME_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_registers(const uint32_t threadId, RegisterSet* registers)
    {
        if (!registers)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE hThread = OpenThread(
          THREAD_GET_CONTEXT | THREAD_QUERY_INFORMATION, FALSE, threadId);

        if (!hThread)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        std::memset(registers, 0, sizeof(RegisterSet));

        const ProcessArchitecture arch = get_process_architecture();

        if (arch == ProcessArchitecture::X86)
        {
            WOW64_CONTEXT ctx{};
            ctx.ContextFlags = WOW64_CONTEXT_FULL;

            if (!Wow64GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
            }

            CloseHandle(hThread);
            fill_registers_from_wow64_context(registers, ctx);
        }
        else if (arch == ProcessArchitecture::X86_64)
        {
            alignas(16) CONTEXT ctx{};
            ctx.ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_SEGMENTS;

            if (!GetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
            }

            CloseHandle(hThread);
            fill_registers_from_context(registers, ctx);
        }
        else
        {
            CloseHandle(hThread);
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        return StatusCode::STATUS_OK;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_call_stack(const uint32_t threadId, const CallStack* callStack)
    {
        (void)threadId;
        (void)callStack;
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_exception_info(const ExceptionInfo* exception)
    {
        (void)exception;
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }

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
            const auto& regMap = get_x86_register_map();
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
            const auto& regMap = get_x64_register_map();
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

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_write_register(const std::uint32_t threadId, const char* name, const void* in, const std::size_t size)
    {
        if (!name || !in || size == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const HANDLE hThread = OpenThread(THREAD_GET_CONTEXT | THREAD_SET_CONTEXT, FALSE, threadId);
        if (!hThread)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        StatusCode result{};
        const ProcessArchitecture arch = get_process_architecture();

        if (arch == ProcessArchitecture::X86)
        {
            const auto& regMap = get_x86_register_map();
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

            auto* destPtr = reinterpret_cast<std::uint8_t*>(&ctx) + it->second.offset;
            const std::size_t copySize = std::min(size, static_cast<std::size_t>(it->second.size));
            std::memcpy(destPtr, in, copySize);

            if (!Wow64SetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_REGISTER_WRITE_FAILED;
            }

            result = StatusCode::STATUS_OK;
        }
        else if (arch == ProcessArchitecture::X86_64)
        {
            const auto& regMap = get_x64_register_map();
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

            auto* destPtr = reinterpret_cast<std::uint8_t*>(&ctx) + it->second.offset;
            const std::size_t copySize = std::min(size, static_cast<std::size_t>(it->second.size));
            std::memcpy(destPtr, in, copySize);

            if (!SetThreadContext(hThread, &ctx))
            {
                CloseHandle(hThread);
                return StatusCode::STATUS_ERROR_REGISTER_WRITE_FAILED;
            }

            result = StatusCode::STATUS_OK;
        }
        else
        {
            CloseHandle(hThread);
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        CloseHandle(hThread);
        return result;
    }

    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_thread_priority_value_to_string(const std::int32_t priority, char** out, std::size_t* outSize)
    {
        if (out == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto entryIt = std::ranges::find_if(k_priority_entries, [priority](const PriorityEntry& entry)
            {
                return entry.priority == priority;
            });

        const PriorityEntry* selected = nullptr;
        if (entryIt != k_priority_entries.end())
        {
            selected = &(*entryIt);
        }
        else if (priority >= -15 && priority <= 15)
        {
            selected = &k_custom_priority;
        }
        else
        {
            selected = &k_invalid_priority;
        }

        *out = const_cast<char*>(selected->text.data());
        if (outSize != nullptr)
        {
            *outSize = selected->size;
        }

        return StatusCode::STATUS_OK;
    }
}