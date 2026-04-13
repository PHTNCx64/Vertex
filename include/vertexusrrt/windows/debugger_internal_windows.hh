//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/debugger_internal_common.hh>

#include <windows.h>

namespace debugger
{
    [[nodiscard]] dbg_continue_status_t execute_resume(TickState& state);

    constexpr DWORD STATUS_WX86_BREAKPOINT = 0x4000001F;
    constexpr DWORD STATUS_WX86_SINGLE_STEP = 0x4000001E;
    constexpr DWORD WAIT_TIMEOUT_MS = 100;

    struct ThreadHandleCache final
    {
        std::mutex mutex{};
        std::unordered_map<DWORD, HANDLE> handles{};
    };

    ThreadHandleCache& get_thread_handle_cache();
    void cache_thread_handle(DWORD threadId);
    void release_thread_handle(DWORD threadId);
    [[nodiscard]] HANDLE get_cached_thread_handle(DWORD threadId);
    void clear_thread_handle_cache();

    [[nodiscard]] TickEventResult handle_exception_breakpoint(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_exception_single_step(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_exception_general(TickState& state, const DEBUG_EVENT& event);

    [[nodiscard]] TickEventResult handle_create_process(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_exit_process(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_create_thread(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_exit_thread(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_load_dll(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_unload_dll(TickState& state, const DEBUG_EVENT& event);
    [[nodiscard]] TickEventResult handle_output_string(TickState& state, const DEBUG_EVENT& event);

    [[nodiscard]] ExceptionCode map_windows_exception_code(DWORD code);

    [[nodiscard]] bool set_trap_flag(HANDLE threadHandle, bool enable);

    [[nodiscard]] StatusCode disable_watchpoint_on_thread(HANDLE threadHandle, std::uint8_t registerIndex);
    [[nodiscard]] StatusCode enable_watchpoint_on_thread(HANDLE threadHandle, std::uint8_t registerIndex);

    [[nodiscard]] DWORD suspend_thread(HANDLE hThread);
    [[nodiscard]] DWORD resume_thread(HANDLE hThread);
}
