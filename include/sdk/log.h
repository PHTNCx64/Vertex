//
// Copyright (C) 2026 PHTNC<>.
// Licensed under Apache 2.0
//

#pragma once

#include "statuscode.h"
#include "macro.h"

#ifdef __cplusplus
extern "C" {
#endif

// ===============================================================================================================//
// LOG TYPES                                                                                                      //
// ===============================================================================================================//

typedef void* VertexLogHandle;

// ===============================================================================================================//
// LOG API FUNCTIONS                                                                                              //
// ===============================================================================================================//

VERTEX_EXPORT VertexLogHandle VERTEX_API vertex_log_get_instance();
VERTEX_EXPORT StatusCode VERTEX_API vertex_log_set_instance(VertexLogHandle handle);

VERTEX_EXPORT StatusCode VERTEX_API vertex_log_info(const char* msg, ...);
VERTEX_EXPORT StatusCode VERTEX_API vertex_log_warn(const char* msg, ...);
VERTEX_EXPORT StatusCode VERTEX_API vertex_log_error(const char* msg, ...);

#ifdef __cplusplus
}
#endif
