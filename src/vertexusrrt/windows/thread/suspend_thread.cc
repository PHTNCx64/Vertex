//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <Windows.h>
#include <cstdint>

namespace debugger
{
    DWORD suspend_thread(HANDLE hThread);
}

extern "C"
{
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
}
