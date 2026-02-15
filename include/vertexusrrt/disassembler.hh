//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#pragma once

#include <sdk/api.h>
#include <cstdint>
#include <span>

namespace PluginRuntime
{
    enum class DisasmMode
    {
        X86_32,
        X86_64,
        ARM64
    };

    [[nodiscard]] StatusCode init_disassembler(DisasmMode mode);

    void cleanup_disassembler();

    [[nodiscard]] bool is_disassembler_initialized();

    [[nodiscard]] const char* get_last_disassembler_error();

    [[nodiscard]] DisasmMode get_disassembler_mode();

    [[nodiscard]] StatusCode disassemble(
        uint64_t address,
        std::span<const uint8_t> code,
        DisassemblerResults* results
    );

    [[nodiscard]] uint32_t disassemble_single(
        uint64_t address,
        std::span<const uint8_t> code,
        DisassemblerResult* result
    );

    [[nodiscard]] BranchDirection compute_branch_direction(
        uint64_t current_address,
        uint64_t target_address,
        uint64_t function_start = 0,
        uint64_t function_end = 0
    );

} // namespace PluginRuntime
