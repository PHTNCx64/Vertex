//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/process_internal.hh>

#include <array>

namespace
{
    constexpr std::array<InjectionMethod, 1> g_injectionMethods{{
        {
            "PTrace Injector",
            "Hijacks the target using PTrace and register manipulation to redirect code execution to the injected library using mmap, dlopen.",
            nullptr,
            {{".so"}},
            1
        }
    }};
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_injection_methods(VertexInjectionMethod** methods, std::uint32_t* count)
{
    *count = static_cast<std::uint32_t>(g_injectionMethods.size());
    if (methods)
    {
        *methods = const_cast<VertexInjectionMethod*>(g_injectionMethods.data());
    }
    return StatusCode::STATUS_OK;
}
