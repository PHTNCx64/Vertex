//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>
#include <sched.h>

namespace debugger
{
    int resume_thread(pid_t tid);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_resume_thread(const uint32_t threadId)
    {
        const int result = debugger::resume_thread(static_cast<pid_t>(threadId));

        if (result == -1)
        {
            return StatusCode::STATUS_ERROR_THREAD_RESUME_FAILED;
        }

        return StatusCode::STATUS_OK;
    }
}
