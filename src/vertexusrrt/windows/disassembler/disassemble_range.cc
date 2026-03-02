//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/disassembler.hh>
#include <vertexusrrt/process_internal.hh>
#include <sdk/api.h>

#include <span>
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

    status = PluginRuntime::disassemble(address, std::span<const std::uint8_t>(buffer.data(), buffer.size()), results);

    if (status == StatusCode::STATUS_OK && results->count > 0)
    {
        auto resolved = ProcessInternal::resolve_module_sections(address);
        if (resolved.has_value())
        {
            for (auto& res : std::span{results->results, results->count})
            {
                if (res.sectionName[0] != '\0')
                {
                    continue;
                }

                const auto rva = res.address - resolved->baseAddress;
                const auto* name = ProcessInternal::find_section_for_rva(resolved->sections, rva);
                if (name)
                {
                    ProcessInternal::vertex_cpy(res.sectionName, name, VERTEX_MAX_SECTION_LENGTH);
                }
            }
        }
    }

    return status;
}
