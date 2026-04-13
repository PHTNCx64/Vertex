//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <sys/ptrace.h>
#include <sys/user.h>

#include <cstdint>
#include <cstring>
#include <sched.h>

extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    void fill_registers_from_user_regs_x86(RegisterSet* registers, const user_regs_struct& regs);
    void fill_registers_from_user_regs(RegisterSet* registers, const user_regs_struct& regs);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_registers(const uint32_t threadId, RegisterSet* registers)
    {
        if (!registers)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::memset(registers, 0, sizeof(RegisterSet));

        user_regs_struct regs{};
        if (ptrace(PTRACE_GETREGS, static_cast<pid_t>(threadId), nullptr, &regs) != 0)
        {
            return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
        }

        const ProcessArchitecture arch = get_process_architecture();

        if (arch == ProcessArchitecture::X86)
        {
            ThreadInternal::fill_registers_from_user_regs_x86(registers, regs);
        }
        else if (arch == ProcessArchitecture::X86_64)
        {
            ThreadInternal::fill_registers_from_user_regs(registers, regs);
        }
        else
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }

        return StatusCode::STATUS_OK;
    }
}
