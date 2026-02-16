//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/memory_internal.hh>

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_memory_construct_attribute_filters(MemoryAttributeOption** options, std::uint32_t* count)
{
    if (!options || !count)
    {
        return STATUS_ERROR_INVALID_PARAMETER;
    }

    *count = MemoryInternal::g_memoryAttributeOptionsSize;
    *options = MemoryInternal::g_memoryProtectionOptions.data();

    return STATUS_OK;
}
