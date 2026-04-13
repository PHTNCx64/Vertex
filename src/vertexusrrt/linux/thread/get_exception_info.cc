//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

namespace debugger
{
    [[nodiscard]] StatusCode fill_exception_info(ExceptionInfo* out);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_exception_info(ExceptionInfo* exception)
    {
        return debugger::fill_exception_info(exception);
    }
}
