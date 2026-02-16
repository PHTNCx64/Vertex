//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/debugger_internal.hh>
#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

#include <Windows.h>
#include <cstdint>
#include <cstring>

extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    void fill_registers_from_wow64_context(RegisterSet* registers, const WOW64_CONTEXT& ctx);
    void fill_registers_from_context(RegisterSet* registers, const CONTEXT& ctx);
}

extern "C"
{
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
            ThreadInternal::fill_registers_from_wow64_context(registers, ctx);
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
            ThreadInternal::fill_registers_from_context(registers, ctx);
        }
        else
        {
            CloseHandle(hThread);
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        return StatusCode::STATUS_OK;
    }
}
