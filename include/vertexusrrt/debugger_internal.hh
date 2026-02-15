//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/debugloopcontext.hh>
#include <sdk/debugger.h>

#include <Windows.h>

#include <mutex>
#include <optional>
#include <unordered_map>
#include <vector>

namespace debugger
{
    constexpr DWORD STATUS_WX86_BREAKPOINT = 0x4000001F;
    constexpr DWORD STATUS_WX86_SINGLE_STEP = 0x4000001E;
    constexpr DWORD WAIT_TIMEOUT_MS = 100;
    constexpr DWORD EFLAGS_TRAP_FLAG = 0x100;
    constexpr std::uint8_t INT3_OPCODE = 0xCC;
    constexpr std::size_t MAX_INSTRUCTION_SIZE = 15;

    struct TempBreakpoint
    {
        std::uint64_t address{};
        std::uint8_t originalByte{};
        bool active{false};
    };

    struct BreakpointStepOver
    {
        std::uint64_t address{};
        bool active{false};
    };

    struct WatchpointStepOver
    {
        std::uint32_t watchpointId{};
        std::uint8_t registerIndex{};
        DWORD threadId{};
        bool active{false};
    };

    struct ThreadHandleCache
    {
        std::mutex mutex{};
        std::unordered_map<DWORD, HANDLE> handles{};
    };

    ThreadHandleCache& get_thread_handle_cache();
    void cache_thread_handle(DWORD threadId);
    void release_thread_handle(DWORD threadId);
    [[nodiscard]] HANDLE get_cached_thread_handle(DWORD threadId);
    void clear_thread_handle_cache();

    extern BreakpointStepOver g_breakpointStepOver;
    extern std::mutex g_breakpointStepOverMutex;

    extern std::unordered_map<DWORD, WatchpointStepOver> g_watchpointStepOvers;
    extern std::mutex g_watchpointStepOverMutex;

    extern TempBreakpoint g_tempBreakpoint;
    extern std::mutex g_tempBreakpointMutex;

    [[nodiscard]] bool is_paused_state(DebuggerState state);

    [[nodiscard]] std::uint64_t get_instruction_pointer(DWORD threadId, bool isWow64);
    [[nodiscard]] std::uint64_t get_stack_pointer(DWORD threadId, bool isWow64);

    [[nodiscard]] bool read_process_memory(std::uint64_t address, void* buffer, std::size_t size);
    [[nodiscard]] bool write_process_memory(std::uint64_t address, const void* buffer, std::size_t size);

    [[nodiscard]] bool set_trap_flag(DWORD threadId, bool isWow64, bool enable);
    [[nodiscard]] bool decrement_instruction_pointer(DWORD threadId, bool isWow64);

    [[nodiscard]] bool set_temp_breakpoint(std::uint64_t address);
    [[nodiscard]] bool remove_temp_breakpoint();
    [[nodiscard]] bool is_temp_breakpoint_hit(std::uint64_t address);

    [[nodiscard]] std::optional<std::uint64_t> get_step_over_target(DWORD threadId, bool isWow64);
    [[nodiscard]] std::optional<std::uint64_t> get_step_out_target(DWORD threadId, bool isWow64);

    [[nodiscard]] DWORD process_continue_command(const DebugLoopContext& ctx);
    [[nodiscard]] DWORD process_step_into_command(const DebugLoopContext& ctx, DWORD threadId, bool isWow64);
    [[nodiscard]] DWORD process_step_over_command(const DebugLoopContext& ctx, DWORD threadId, bool isWow64);
    [[nodiscard]] DWORD process_step_out_command(const DebugLoopContext& ctx, DWORD threadId, bool isWow64);
    [[nodiscard]] DWORD process_run_to_address_command(const DebugLoopContext& ctx);

    [[nodiscard]] DWORD handle_exception_breakpoint(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                       const std::stop_token& stopToken, bool& shouldWaitForCommand);
    [[nodiscard]] DWORD handle_exception_single_step(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                        const std::stop_token& stopToken, bool& shouldWaitForCommand);
    [[nodiscard]] DWORD handle_exception_general(const DebugLoopContext& ctx, const DEBUG_EVENT& event,
                                    const std::stop_token& stopToken, bool& shouldWaitForCommand);

    [[nodiscard]] DWORD handle_create_process(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_exit_process(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_create_thread(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_exit_thread(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_load_dll(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_unload_dll(const DebugLoopContext& ctx, const DEBUG_EVENT& event);
    [[nodiscard]] DWORD handle_output_string(const DebugLoopContext& ctx, const DEBUG_EVENT& event);

    [[nodiscard]] DebugCommand wait_for_command(const DebugLoopContext& ctx, const std::stop_token& stopToken);

    [[nodiscard]] StatusCode set_software_breakpoint(std::uint64_t address, std::uint32_t* breakpointId);
    [[nodiscard]] StatusCode remove_software_breakpoint(std::uint32_t breakpointId);
    [[nodiscard]] StatusCode enable_software_breakpoint(std::uint32_t breakpointId, bool enable);
    [[nodiscard]] bool is_user_breakpoint_hit(std::uint64_t address, std::uint32_t* breakpointId);
    [[nodiscard]] StatusCode restore_breakpoint_byte(std::uint64_t address);
    [[nodiscard]] StatusCode reapply_breakpoint_byte(std::uint64_t address);

    [[nodiscard]] StatusCode set_hardware_breakpoint(std::uint64_t address, ::BreakpointType type, std::uint8_t size, std::uint32_t* breakpointId);
    [[nodiscard]] StatusCode remove_hardware_breakpoint(std::uint32_t breakpointId);
    [[nodiscard]] StatusCode enable_hardware_breakpoint(std::uint32_t breakpointId, bool enable);
    [[nodiscard]] bool is_hardware_breakpoint_hit(std::uint64_t address, std::uint32_t* breakpointId);

    [[nodiscard]] StatusCode set_watchpoint(std::uint64_t address, std::uint32_t size, ::WatchpointType type, std::uint32_t* watchpointId);
    [[nodiscard]] StatusCode remove_watchpoint(std::uint32_t watchpointId);
    [[nodiscard]] StatusCode enable_watchpoint(std::uint32_t watchpointId, bool enable);
    [[nodiscard]] bool is_watchpoint_hit(std::uint64_t dr6Value, std::uint32_t* watchpointId,
                                          ::WatchpointType* type, std::uint64_t* watchedAddress, std::uint32_t* watchedSize);
    [[nodiscard]] StatusCode get_watchpoint_info(std::uint32_t watchpointId, WatchpointData* outData);
    [[nodiscard]] StatusCode get_all_watchpoints(std::vector<WatchpointData>& outWatchpoints);
    [[nodiscard]] StatusCode reset_watchpoint_hit_count(std::uint32_t watchpointId);
    [[nodiscard]] StatusCode apply_watchpoint_to_all_threads(std::uint32_t watchpointId);
    [[nodiscard]] StatusCode temporarily_disable_watchpoint_on_all_threads(std::uint32_t watchpointId);
    [[nodiscard]] StatusCode re_enable_watchpoint_on_all_threads(std::uint32_t watchpointId);
    [[nodiscard]] StatusCode clear_hw_register_on_all_threads(std::uint8_t registerIndex);

    void set_breakpoint_step_over(std::uint64_t address);
    void clear_breakpoint_step_over();
    [[nodiscard]] bool is_stepping_over_breakpoint(std::uint64_t* address);

    void set_watchpoint_step_over(std::uint32_t watchpointId, std::uint8_t registerIndex, DWORD threadId);
    void clear_watchpoint_step_over(DWORD threadId);
    [[nodiscard]] bool is_stepping_over_watchpoint(DWORD threadId, std::uint32_t* watchpointId);

    [[nodiscard]] bool set_trap_flag(HANDLE threadHandle, bool isWow64, bool enable);

    [[nodiscard]] StatusCode disable_watchpoint_on_thread(HANDLE threadHandle, std::uint8_t registerIndex, bool isWow64);
    [[nodiscard]] StatusCode disable_watchpoint_on_thread(DWORD threadId, std::uint8_t registerIndex, bool isWow64);

    [[nodiscard]] StatusCode enable_watchpoint_on_thread(HANDLE threadHandle, std::uint8_t registerIndex, bool isWow64);
    [[nodiscard]] StatusCode enable_watchpoint_on_thread(DWORD threadId, std::uint8_t registerIndex, bool isWow64);

    [[nodiscard]] DWORD suspend_thread(HANDLE hThread);
    [[nodiscard]] DWORD resume_thread(HANDLE hThread);
}
