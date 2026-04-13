//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>
#include <sched.h>

namespace debugger
{
    int suspend_thread(pid_t tid);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_suspend_thread(const uint32_t threadId)
    {
        const int result = debugger::suspend_thread(static_cast<pid_t>(threadId));

        if (result == -1)
        {
            return StatusCode::STATUS_ERROR_THREAD_SUSPEND_FAILED;
        }

        return StatusCode::STATUS_OK;
    }
}
