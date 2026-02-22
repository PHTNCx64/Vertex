//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_module_exports(const ModuleInformation* module, ModuleExport** exports, uint32_t* count)
    {
        return StatusCode::STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED;
    }
}
