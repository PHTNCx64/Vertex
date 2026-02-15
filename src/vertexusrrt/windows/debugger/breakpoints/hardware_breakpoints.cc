//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/debugloopcontext.hh>

#include <Windows.h>
#include <ranges>
#include <format>

extern ProcessArchitecture get_process_architecture();

namespace debugger
{
    namespace
    {
        constexpr std::uint8_t DR7_LOCAL_ENABLE_SHIFT = 0;
        constexpr std::uint8_t DR7_CONDITION_SHIFT = 16;
        constexpr std::uint8_t DR7_SIZE_SHIFT = 18;
        constexpr std::uint8_t DR7_BITS_PER_REGISTER = 4;

        constexpr std::uint8_t DR7_BREAK_ON_EXECUTE = 0b00;
        constexpr std::uint8_t DR7_BREAK_ON_WRITE = 0b01;
        constexpr std::uint8_t DR7_BREAK_ON_READWRITE = 0b11;

        constexpr std::uint8_t DR7_SIZE_1_BYTE = 0b00;
        constexpr std::uint8_t DR7_SIZE_2_BYTES = 0b01;
        constexpr std::uint8_t DR7_SIZE_8_BYTES = 0b10;
        constexpr std::uint8_t DR7_SIZE_4_BYTES = 0b11;

        [[nodiscard]] std::uint8_t get_dr7_condition(const BreakpointType type)
        {
            switch (type)
            {
            case VERTEX_BP_EXECUTE:
                return DR7_BREAK_ON_EXECUTE;
            case VERTEX_BP_WRITE:
                return DR7_BREAK_ON_WRITE;
            case VERTEX_BP_READ:
            case VERTEX_BP_READWRITE:
                return DR7_BREAK_ON_READWRITE;
            default:
                return DR7_BREAK_ON_EXECUTE;
            }
        }

        [[nodiscard]] std::uint8_t get_dr7_size(const std::uint8_t size)
        {
            switch (size)
            {
            case 1:
                return DR7_SIZE_1_BYTE;
            case 2:
                return DR7_SIZE_2_BYTES;
            case 4:
                return DR7_SIZE_4_BYTES;
            case 8:
                return DR7_SIZE_8_BYTES;
            default:
                return DR7_SIZE_1_BYTE;
            }
        }

        [[nodiscard]] std::optional<std::uint8_t> allocate_hw_register()
        {
            auto& manager = get_breakpoint_manager();

            for (std::uint8_t i = 0; i < 4; ++i)
            {
                if (!manager.hwRegisterUsed[i])
                {
                    manager.hwRegisterUsed[i] = true;
                    return i;
                }
            }

            return std::nullopt;
        }

        void free_hw_register(const std::uint8_t index)
        {
            auto& manager = get_breakpoint_manager();

            if (index < 4)
            {
                manager.hwRegisterUsed[index] = false;
            }
        }

        bool apply_hw_breakpoint_to_thread_native(const HANDLE threadHandle, // NOLINT
                                                   const std::uint8_t registerIndex,
                                                   const std::uint64_t address,
                                                   const BreakpointType type,
                                                   const std::uint8_t size)
        {
            alignas(16) CONTEXT context{};
            context.ContextFlags = CONTEXT_DEBUG_REGISTERS;

            if (!GetThreadContext(threadHandle, &context))
            {
                return false;
            }

            switch (registerIndex)
            {
            case 0:
                context.Dr0 = address;
                break;
            case 1:
                context.Dr1 = address;
                break;
            case 2:
                context.Dr2 = address;
                break;
            case 3:
                context.Dr3 = address;
                break;
            default:
                return false;
            }

            const std::uint8_t condition = get_dr7_condition(type);
            const std::uint8_t sizeValue = get_dr7_size(size);

            const auto localEnableBit = static_cast<std::uint8_t>(1 << (DR7_LOCAL_ENABLE_SHIFT + registerIndex * 2));
            const auto conditionShift = static_cast<std::uint8_t>(DR7_CONDITION_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);
            const auto sizeShift = static_cast<std::uint8_t>(DR7_SIZE_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);

            context.Dr7 |= localEnableBit;
            context.Dr7 &= ~(static_cast<std::uint64_t>(0b11) << conditionShift);
            context.Dr7 |= static_cast<std::uint64_t>(condition) << conditionShift;
            context.Dr7 &= ~(static_cast<std::uint64_t>(0b11) << sizeShift);
            context.Dr7 |= static_cast<std::uint64_t>(sizeValue) << sizeShift;

            return SetThreadContext(threadHandle, &context) != FALSE;
        }

        bool apply_hw_breakpoint_to_thread_wow64(const HANDLE threadHandle, // NOLINT
                                                  const std::uint8_t registerIndex,
                                                  const std::uint64_t address,
                                                  const BreakpointType type,
                                                  const std::uint8_t size)
        {
            WOW64_CONTEXT context{};
            context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;

            if (!Wow64GetThreadContext(threadHandle, &context))
            {
                return false;
            }

            const auto addr32 = static_cast<DWORD>(address);

            switch (registerIndex)
            {
            case 0:
                context.Dr0 = addr32;
                break;
            case 1:
                context.Dr1 = addr32;
                break;
            case 2:
                context.Dr2 = addr32;
                break;
            case 3:
                context.Dr3 = addr32;
                break;
            default:
                return false;
            }

            const std::uint8_t condition = get_dr7_condition(type);
            const std::uint8_t sizeValue = get_dr7_size(size);

            const auto localEnableBit = static_cast<std::uint8_t>(1 << (DR7_LOCAL_ENABLE_SHIFT + registerIndex * 2));
            const auto conditionShift = static_cast<std::uint8_t>(DR7_CONDITION_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);
            const auto sizeShift = static_cast<std::uint8_t>(DR7_SIZE_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);

            context.Dr7 |= localEnableBit;
            context.Dr7 &= ~(static_cast<DWORD>(0b11) << conditionShift);
            context.Dr7 |= static_cast<DWORD>(condition) << conditionShift;
            context.Dr7 &= ~(static_cast<DWORD>(0b11) << sizeShift);
            context.Dr7 |= static_cast<DWORD>(sizeValue) << sizeShift;

            return Wow64SetThreadContext(threadHandle, &context) != FALSE;
        }
    }

    StatusCode set_hardware_breakpoint(const std::uint64_t address,
                                        const BreakpointType type,
                                        const std::uint8_t size,
                                        std::uint32_t* breakpointId)
    {
        if (address == 0 || breakpointId == nullptr)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        if (size != 1 && size != 2 && size != 4 && size != 8)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        const auto alignmentMask = static_cast<std::uint64_t>(size - 1);
        if ((address & alignmentMask) != 0)
        {
            return STATUS_ERROR_BREAKPOINT_ADDRESS_MISALIGNED;
        }

        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto registerIndex = allocate_hw_register();
        if (!registerIndex.has_value())
        {
            return STATUS_ERROR_BREAKPOINT_LIMIT_REACHED;
        }

        const std::uint32_t id = manager.nextBreakpointId.fetch_add(1, std::memory_order_relaxed);

        HardwareBreakpointData bp{};
        bp.id = id;
        bp.address = address;
        bp.type = type;
        bp.state = VERTEX_BP_STATE_ENABLED;
        bp.size = size;
        bp.registerIndex = registerIndex.value();
        bp.hitCount = 0;

        manager.hardwareBreakpoints.emplace(id, bp);
        *breakpointId = id;

        return STATUS_OK;
    }

    StatusCode remove_hardware_breakpoint(const std::uint32_t breakpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.hardwareBreakpoints.find(breakpointId);
        if (it == manager.hardwareBreakpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        const auto registerIndex = it->second.registerIndex;
        free_hw_register(registerIndex);
        manager.hardwareBreakpoints.erase(it);

        return STATUS_OK;
    }

    StatusCode enable_hardware_breakpoint(const std::uint32_t breakpointId, const bool enable)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.hardwareBreakpoints.find(breakpointId);
        if (it == manager.hardwareBreakpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        it->second.state = enable ? VERTEX_BP_STATE_ENABLED : VERTEX_BP_STATE_DISABLED;
        return STATUS_OK;
    }

    StatusCode apply_all_hw_breakpoints_to_thread(const std::uint32_t threadId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const HANDLE threadHandle = get_cached_thread_handle(threadId);
        if (threadHandle == nullptr)
        {
            return STATUS_ERROR_THREAD_NOT_FOUND;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        DWORD(__stdcall *suspend_thread)(HANDLE) = isWow64 ? &Wow64SuspendThread : &SuspendThread;

        if (suspend_thread(threadHandle) == static_cast<DWORD>(-1))
        {
            return STATUS_ERROR_THREAD_NOT_FOUND;
        }

        for (const auto& bp : manager.hardwareBreakpoints | std::views::values)
        {
            if (bp.state != VERTEX_BP_STATE_ENABLED)
            {
                continue;
            }

            const bool success = isWow64
                ? apply_hw_breakpoint_to_thread_wow64(threadHandle, bp.registerIndex, bp.address, bp.type, bp.size)
                : apply_hw_breakpoint_to_thread_native(threadHandle, bp.registerIndex, bp.address, bp.type, bp.size);

            if (!success)
            {
                ResumeThread(threadHandle);
                return STATUS_ERROR_BREAKPOINT_SET_FAILED;
            }
        }

        for (const auto& wp : manager.watchpoints | std::views::values)
        {
            if (!wp.enabled || wp.temporarilyDisabled)
            {
                continue;
            }

            const auto bpType = convert_watchpoint_type_to_breakpoint(wp.type);
            const auto wpSize = static_cast<std::uint8_t>(wp.size);

            const bool success = isWow64
                ? apply_hw_breakpoint_to_thread_wow64(threadHandle, wp.registerIndex, wp.address, bpType, wpSize)
                : apply_hw_breakpoint_to_thread_native(threadHandle, wp.registerIndex, wp.address, bpType, wpSize);

            if (!success)
            {
                ResumeThread(threadHandle);
                return STATUS_ERROR_BREAKPOINT_SET_FAILED;
            }
        }

        ResumeThread(threadHandle);
        return STATUS_OK;
    }

    bool is_hardware_breakpoint_hit(const std::uint64_t address, std::uint32_t* breakpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        for (auto& [id, bp] : manager.hardwareBreakpoints)
        {
            if (bp.address == address && bp.state == VERTEX_BP_STATE_ENABLED)
            {
                bp.hitCount++;
                if (breakpointId != nullptr)
                {
                    *breakpointId = id;
                }
                return true;
            }
        }

        return false;
    }

    StatusCode apply_watchpoint_to_all_threads(const std::uint32_t watchpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        const auto& wp = it->second;
        if (!wp.enabled)
        {
            return STATUS_OK;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        const auto bpType = convert_watchpoint_type_to_breakpoint(wp.type);
        const auto wpSize = static_cast<std::uint8_t>(wp.size);
        DWORD(__stdcall *suspend_thread)(HANDLE) = isWow64 ? &Wow64SuspendThread : &SuspendThread;

        auto& cache = get_thread_handle_cache();
        std::scoped_lock cacheLock{cache.mutex};

        for (const auto& [threadId, threadHandle] : cache.handles)
        {
            if (suspend_thread(threadHandle) == static_cast<DWORD>(-1))
            {
                continue;
            }

            const bool applied = isWow64
                ? apply_hw_breakpoint_to_thread_wow64(threadHandle, wp.registerIndex, wp.address, bpType, wpSize)
                : apply_hw_breakpoint_to_thread_native(threadHandle, wp.registerIndex, wp.address, bpType, wpSize);

            if (!applied)
            {
                auto logMsg = std::format("[Vertex] apply_watchpoint: failed for thread {} error={}\n",
                                         threadId, GetLastError());
                OutputDebugStringA(logMsg.c_str());
            }

            ResumeThread(threadHandle);
        }

        return STATUS_OK;
    }

    StatusCode temporarily_disable_watchpoint_on_all_threads(const std::uint32_t watchpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        auto& wp = it->second;
        wp.temporarilyDisabled = true;

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        DWORD(__stdcall *suspend_thread)(HANDLE) = isWow64 ? &Wow64SuspendThread : &SuspendThread;

        auto& cache = get_thread_handle_cache();
        std::scoped_lock cacheLock{cache.mutex};

        for (const auto& [threadId, threadHandle] : cache.handles)
        {
            if (suspend_thread(threadHandle) == static_cast<DWORD>(-1))
            {
                continue;
            }

            if (isWow64)
            {
                WOW64_CONTEXT context{};
                context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;
                if (Wow64GetThreadContext(threadHandle, &context))
                {
                    const auto localEnableBit = static_cast<DWORD>(1 << (wp.registerIndex * 2));
                    context.Dr7 &= ~localEnableBit;
                    Wow64SetThreadContext(threadHandle, &context);
                }
            }
            else
            {
                alignas(16) CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(threadHandle, &context))
                {
                    const auto localEnableBit = static_cast<std::uint64_t>(1) << (wp.registerIndex * 2);
                    context.Dr7 &= ~localEnableBit;
                    SetThreadContext(threadHandle, &context);
                }
            }

            ResumeThread(threadHandle);
        }

        return STATUS_OK;
    }

    StatusCode re_enable_watchpoint_on_all_threads(const std::uint32_t watchpointId)
    {
        auto& manager = get_breakpoint_manager();
        std::scoped_lock lock{manager.mutex};

        const auto it = manager.watchpoints.find(watchpointId);
        if (it == manager.watchpoints.end())
        {
            return STATUS_ERROR_BREAKPOINT_NOT_FOUND;
        }

        auto& wp = it->second;
        wp.temporarilyDisabled = false;

        if (!wp.enabled)
        {
            return STATUS_OK;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        DWORD(__stdcall *suspend_thread)(HANDLE) = isWow64 ? &Wow64SuspendThread : &SuspendThread;

        auto& cache = get_thread_handle_cache();
        std::scoped_lock cacheLock{cache.mutex};

        for (const auto& [threadId, threadHandle] : cache.handles)
        {
            if (suspend_thread(threadHandle) == static_cast<DWORD>(-1))
            {
                continue;
            }

            if (isWow64)
            {
                WOW64_CONTEXT context{};
                context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;
                if (Wow64GetThreadContext(threadHandle, &context))
                {
                    const auto localEnableBit = static_cast<DWORD>(1 << (wp.registerIndex * 2));
                    context.Dr7 |= localEnableBit;
                    Wow64SetThreadContext(threadHandle, &context);
                }
            }
            else
            {
                alignas(16) CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(threadHandle, &context))
                {
                    const auto localEnableBit = static_cast<std::uint64_t>(1) << (wp.registerIndex * 2);
                    context.Dr7 |= localEnableBit;
                    SetThreadContext(threadHandle, &context);
                }
            }

            ResumeThread(threadHandle);
        }

        return STATUS_OK;
    }

    StatusCode clear_hw_register_on_all_threads(const std::uint8_t registerIndex)
    {
        if (registerIndex >= 4)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        const bool isWow64 = get_process_architecture() == ProcessArchitecture::X86;
        DWORD(__stdcall *suspend_thread)(HANDLE) = isWow64 ? &Wow64SuspendThread : &SuspendThread;

        auto& cache = get_thread_handle_cache();
        std::scoped_lock cacheLock{cache.mutex};

        for (const auto& [threadId, threadHandle] : cache.handles)
        {
            if (suspend_thread(threadHandle) == static_cast<DWORD>(-1))
            {
                continue;
            }

            if (isWow64)
            {
                WOW64_CONTEXT context{};
                context.ContextFlags = WOW64_CONTEXT_DEBUG_REGISTERS;
                if (Wow64GetThreadContext(threadHandle, &context))
                {
                    switch (registerIndex)
                    {
                    case 0: context.Dr0 = 0; break;
                    case 1: context.Dr1 = 0; break;
                    case 2: context.Dr2 = 0; break;
                    case 3: context.Dr3 = 0; break;
                    default: std::unreachable();
                    }

                    const auto enableBit = static_cast<DWORD>(1 << (registerIndex * 2));
                    const auto conditionShift = static_cast<std::uint8_t>(DR7_CONDITION_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);
                    const auto sizeShift = static_cast<std::uint8_t>(DR7_SIZE_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);

                    context.Dr7 &= ~enableBit;
                    context.Dr7 &= ~(static_cast<DWORD>(0b11) << conditionShift);
                    context.Dr7 &= ~(static_cast<DWORD>(0b11) << sizeShift);

                    Wow64SetThreadContext(threadHandle, &context);
                }
            }
            else
            {
                alignas(16) CONTEXT context{};
                context.ContextFlags = CONTEXT_DEBUG_REGISTERS;
                if (GetThreadContext(threadHandle, &context))
                {
                    switch (registerIndex)
                    {
                    case 0: context.Dr0 = 0; break;
                    case 1: context.Dr1 = 0; break;
                    case 2: context.Dr2 = 0; break;
                    case 3: context.Dr3 = 0; break;
                    default: std::unreachable();
                    }

                    const auto enableBit = static_cast<std::uint64_t>(1) << (registerIndex * 2);
                    const auto conditionShift = static_cast<std::uint8_t>(DR7_CONDITION_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);
                    const auto sizeShift = static_cast<std::uint8_t>(DR7_SIZE_SHIFT + registerIndex * DR7_BITS_PER_REGISTER);

                    context.Dr7 &= ~enableBit;
                    context.Dr7 &= ~(static_cast<std::uint64_t>(0b11) << conditionShift);
                    context.Dr7 &= ~(static_cast<std::uint64_t>(0b11) << sizeShift);

                    SetThreadContext(threadHandle, &context);
                }
            }

            ResumeThread(threadHandle);
        }

        return STATUS_OK;
    }
}
