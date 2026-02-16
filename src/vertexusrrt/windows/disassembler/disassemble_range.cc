//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/disassembler.hh>
#include <sdk/api.h>

#include <vector>

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_process_disassemble_range(std::uint64_t address, std::uint32_t size, DisassemblerResults* results)
{
    if (!results || size == 0)
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    if (!PluginRuntime::is_disassembler_initialized())
    {
        return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
    }

    std::vector<std::uint8_t> buffer(size);
    StatusCode status = vertex_memory_read_process(address, size, reinterpret_cast<char*>(buffer.data()));
    if (status != StatusCode::STATUS_OK)
    {
        return status;
    }

    return PluginRuntime::disassemble(address, std::span<const std::uint8_t>(buffer.data(), buffer.size()), results);
}
