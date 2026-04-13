//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <vertexusrrt/debugloopcontext_common.hh>

#include <sys/types.h>
#include <cstdint>

namespace debugger
{
    using dbg_thread_id_t = pid_t;
    using dbg_process_id_t = pid_t;
    using dbg_continue_status_t = int;
    constexpr dbg_continue_status_t DBG_STATUS_CONTINUE{};
    constexpr dbg_continue_status_t DBG_STATUS_NOT_HANDLED{-1};

    struct TickState final
    {
        std::uint32_t attachedProcessId{};
        std::uint32_t currentThreadId{};
    };
}
