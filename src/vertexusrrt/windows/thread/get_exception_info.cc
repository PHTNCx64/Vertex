//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

#include <tuple>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_exception_info(const ExceptionInfo* exception)
    {
        std::ignore = exception;
        return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
    }
}
