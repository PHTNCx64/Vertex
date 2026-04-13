//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <cstdint>
#include <tuple>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_call_stack(const uint32_t threadId, const CallStack* callStack)
    {
        std::ignore = threadId;
        std::ignore = callStack;
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }
}
