//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/process_internal.hh>

extern void clear_process_architecture();

native_handle& get_native_handle()
{
    static native_handle handle;
    return handle;
}

namespace ProcessInternal
{
    ModuleCache& get_module_cache()
    {
        static ModuleCache cache;
        return cache;
    }

    ProcessInformation* opened_process_info()
    {
        static ProcessInformation info{};
        return &info;
    }

    StatusCode invalidate_handle()
    {
        native_handle& handle = get_native_handle();
        CloseHandle(handle);
        handle = INVALID_HANDLE_VALUE;

        clear_process_architecture();

        opened_process_info()->processId = 0;
        std::fill_n(opened_process_info()->processName, VERTEX_MAX_NAME_LENGTH, 0);
        std::fill_n(opened_process_info()->processOwner, VERTEX_MAX_OWNER_LENGTH, 0);

        return StatusCode::STATUS_OK;
    }
}

extern "C"
{
    void clear_module_cache()
    {
        auto& [importCache, exportCache, cacheMutex] = ProcessInternal::get_module_cache();
        std::scoped_lock lock{cacheMutex};
        importCache.clear();
        exportCache.clear();
    }
}
