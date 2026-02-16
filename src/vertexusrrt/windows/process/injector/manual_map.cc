//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <filesystem>

#include <vertexusrrt/native_handle.hh>

#include <sdk/api.h>

extern native_handle& get_native_handle();
extern Runtime* g_pluginRuntime;
extern std::optional<ProcessArchitecture> detect_dll_architecture(const std::filesystem::path& dllPath);
extern ProcessArchitecture get_process_architecture();

// TODO: Implement manual map injection.

StatusCode manual_map_inject(const char* path)
{
    std::ignore = path;
    return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
}