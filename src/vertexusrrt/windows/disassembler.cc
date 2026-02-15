//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/disassembler.hh>

#include <capstone/capstone.h>
#include <algorithm>
#include <mutex>
#include <unordered_set>
#include <string_view>
#include <vector>

namespace PluginRuntime
{
    namespace
    {
        std::mutex g_capstone_mutex;

        csh g_capstone_handle{0};
        auto g_current_mode{DisasmMode::X86_64};
        bool g_initialized{false};
        cs_err g_last_capstone_error{CS_ERR_OK};

        const std::unordered_set<std::string_view> x86_unconditional_jumps{
            "jmp"
        };

        const std::unordered_set<std::string_view> x86_conditional_jumps{
            "jo", "jno",
            "js", "jns",
            "je", "jz", "jne", "jnz",
            "jl", "jnge", "jge", "jnl",
            "jle", "jng", "jg", "jnle",
            "jb", "jnae", "jc",
            "jnb", "jae", "jnc",
            "jbe", "jna",
            "ja", "jnbe",
            "jp", "jpe",
            "jnp", "jpo",
            "jcxz", "jecxz", "jrcxz"
        };

        const std::unordered_set<std::string_view> x86_loop_instructions{
            "loop", "loope", "loopz", "loopne", "loopnz"
        };

        const std::unordered_set<std::string_view> x86_call_instructions{
            "call"
        };

        const std::unordered_set<std::string_view> x86_return_instructions{
            "ret", "retn", "retf", "iret", "iretd", "iretq"
        };

        const std::unordered_set<std::string_view> x86_interrupt_instructions{
            "int", "int1", "int3", "into", "syscall", "sysenter", "sysexit", "sysret"
        };

        bool is_in_set(const char* mnem, const std::unordered_set<std::string_view>& set)
        {
            return mnem && set.contains(mnem);
        }

        template<std::size_t N>
        void copy_string(char (&dest)[N], const char* src)
        {
            if (src)
            {
                const std::string_view sv{src};
                const auto len = std::min(sv.size(), N - 1);
                std::copy_n(sv.data(), len, dest);
                dest[len] = '\0';
            }
            else
            {
                dest[0] = '\0';
            }
        }

        InstructionCategory map_category(const cs_insn* insn, cs_arch arch)
        {
            const cs_detail* detail = insn->detail;
            if (!detail)
            {
                return VERTEX_INSTRUCTION_UNKNOWN;
            }

            for (std::uint8_t i = 0; i < detail->groups_count; ++i)
            {
                switch (detail->groups[i])
                {
                    case CS_GRP_JUMP:
                    case CS_GRP_CALL:
                    case CS_GRP_RET:
                    case CS_GRP_IRET:
                        return VERTEX_INSTRUCTION_CONTROL_FLOW;

                    case CS_GRP_INT:
                        return VERTEX_INSTRUCTION_SYSTEM;

                    case CS_GRP_PRIVILEGE:
                        return VERTEX_INSTRUCTION_PRIVILEGED;
                    default:
                        break;
                }
            }

            if (arch == CS_ARCH_X86)
            {
                const cs_x86& x86 = detail->x86;

                for (std::uint8_t i = 0; i < detail->groups_count; ++i)
                {
                    const std::uint8_t grp = detail->groups[i];
                    if (grp >= X86_GRP_SSE1 && grp <= X86_GRP_SSE42)
                    {
                        return VERTEX_INSTRUCTION_SIMD;
                    }
                    if (grp >= X86_GRP_AVX && grp <= X86_GRP_AVX512)
                    {
                        return VERTEX_INSTRUCTION_SIMD;
                    }
                    if (grp == X86_GRP_FPU)
                    {
                        return VERTEX_INSTRUCTION_FLOATING_POINT;
                    }
                    if (grp == X86_GRP_AES || grp == X86_GRP_SHA)
                    {
                        return VERTEX_INSTRUCTION_CRYPTO;
                    }
                }

                if (x86.prefix[0] == X86_PREFIX_REP || x86.prefix[0] == X86_PREFIX_REPE ||
                    x86.prefix[0] == X86_PREFIX_REPNE)
                {
                    return VERTEX_INSTRUCTION_STRING;
                }
            }
            else if (arch == CS_ARCH_ARM64)
            {
                for (std::uint8_t i = 0; i < detail->groups_count; ++i)
                {
                    const std::uint8_t grp = detail->groups[i];
                    if (grp == ARM64_GRP_CRYPTO)
                    {
                        return VERTEX_INSTRUCTION_CRYPTO;
                    }
                    if (grp == ARM64_GRP_NEON)
                    {
                        return VERTEX_INSTRUCTION_SIMD;
                    }
                    if (grp == ARM64_GRP_FPARMV8)
                    {
                        return VERTEX_INSTRUCTION_FLOATING_POINT;
                    }
                }
            }

            const char* mnem = insn->mnemonic;
            if (!mnem)
            {
                return VERTEX_INSTRUCTION_UNKNOWN;
            }

            const std::string_view sv{mnem};

            if (sv.starts_with("add") || sv.starts_with("sub") ||
                sv.starts_with("mul") || sv.starts_with("div") ||
                sv.starts_with("inc") || sv.starts_with("dec") ||
                sv.starts_with("neg") || sv.starts_with("adc") ||
                sv.starts_with("sbb") || sv.starts_with("imul") ||
                sv.starts_with("idiv"))
            {
                return VERTEX_INSTRUCTION_ARITHMETIC;
            }

            if (sv.starts_with("and") || sv.starts_with("or") ||
                sv.starts_with("xor") || sv.starts_with("not") ||
                sv.starts_with("shl") || sv.starts_with("shr") ||
                sv.starts_with("rol") || sv.starts_with("ror") ||
                sv.starts_with("sar") || sv.starts_with("sal"))
            {
                return VERTEX_INSTRUCTION_LOGIC;
            }

            if (sv.starts_with("mov") || sv.starts_with("lea") ||
                sv.starts_with("push") || sv.starts_with("pop") ||
                sv.starts_with("xchg") || sv.starts_with("ldr") ||
                sv.starts_with("str") || sv.starts_with("ldp") ||
                sv.starts_with("stp"))
            {
                return VERTEX_INSTRUCTION_DATA_TRANSFER;
            }

            if (sv.starts_with("cmp") || sv.starts_with("test") ||
                sv.starts_with("bt"))
            {
                return VERTEX_INSTRUCTION_COMPARISON;
            }

            if (sv.starts_with("movs") || sv.starts_with("stos") ||
                sv.starts_with("lods") || sv.starts_with("cmps") ||
                sv.starts_with("scas"))
            {
                return VERTEX_INSTRUCTION_STRING;
            }

            if (sv.starts_with("int") || sv.starts_with("syscall") ||
                sv.starts_with("sysenter") || sv.starts_with("cpuid") ||
                sv.starts_with("svc") || sv.starts_with("hvc"))
            {
                return VERTEX_INSTRUCTION_SYSTEM;
            }

            return VERTEX_INSTRUCTION_UNKNOWN;
        }

        BranchType map_branch_type(const cs_insn* insn, cs_arch arch)
        {
            const cs_detail* detail = insn->detail;
            const char* mnem = insn->mnemonic;

            if (arch == CS_ARCH_X86 && mnem)
            {
                if (is_in_set(mnem, x86_return_instructions))
                {
                    return VERTEX_BRANCH_RETURN;
                }

                if (is_in_set(mnem, x86_loop_instructions))
                {
                    return VERTEX_BRANCH_LOOP;
                }

                if (is_in_set(mnem, x86_interrupt_instructions))
                {
                    const std::string_view mnemSv{mnem};
                    if (mnemSv == "int3" || mnemSv == "into")
                    {
                        return VERTEX_BRANCH_EXCEPTION;
                    }
                    return VERTEX_BRANCH_INTERRUPT;
                }

                if (is_in_set(mnem, x86_call_instructions))
                {
                    if (detail && detail->x86.op_count > 0)
                    {
                        const cs_x86_op& op = detail->x86.operands[0];
                        if (op.type == X86_OP_REG || op.type == X86_OP_MEM)
                        {
                            return VERTEX_BRANCH_INDIRECT_CALL;
                        }
                    }
                    return VERTEX_BRANCH_CALL;
                }

                if (is_in_set(mnem, x86_conditional_jumps))
                {
                    if (detail && detail->x86.op_count > 0)
                    {
                        const cs_x86_op& op = detail->x86.operands[0];
                        if (op.type == X86_OP_REG || op.type == X86_OP_MEM)
                        {
                            return VERTEX_BRANCH_INDIRECT_JUMP;
                        }
                    }
                    return VERTEX_BRANCH_CONDITIONAL;
                }

                if (is_in_set(mnem, x86_unconditional_jumps))
                {
                    if (detail && detail->x86.op_count > 0)
                    {
                        const cs_x86_op& op = detail->x86.operands[0];
                        if (op.type == X86_OP_REG || op.type == X86_OP_MEM)
                        {
                            return VERTEX_BRANCH_INDIRECT_JUMP;
                        }
                    }
                    return VERTEX_BRANCH_UNCONDITIONAL;
                }

                if (std::string_view{mnem}.starts_with("cmov"))
                {
                    return VERTEX_BRANCH_CONDITIONAL_MOVE;
                }

                if (std::string_view{mnem} == "ud2")
                {
                    return VERTEX_BRANCH_EXCEPTION;
                }
            }

            if (!detail)
            {
                return VERTEX_BRANCH_NONE;
            }

            bool isJump{false};
            bool isCall{false};
            bool isRet{false};
            bool isInt{false};
            bool isIndirect{false};

            for (std::uint8_t i = 0; i < detail->groups_count; ++i)
            {
                switch (detail->groups[i])
                {
                    case CS_GRP_JUMP: isJump = true; break;
                    case CS_GRP_CALL: isCall = true; break;
                    case CS_GRP_RET:
                    case CS_GRP_IRET: isRet = true; break;
                    case CS_GRP_INT: isInt = true; break;
                    case CS_GRP_BRANCH_RELATIVE: isJump = true; break;
                    default:
                        break;
                }
            }

            if (arch == CS_ARCH_ARM64)
            {
                const cs_arm64& arm64 = detail->arm64;
                if (arm64.op_count > 0)
                {
                    const cs_arm64_op& op = arm64.operands[0];
                    if (op.type == ARM64_OP_REG)
                    {
                        isIndirect = true;
                    }
                }
            }

            if (isRet)
            {
                return VERTEX_BRANCH_RETURN;
            }

            if (isInt)
            {
                if (mnem && std::string_view{mnem} == "brk")
                {
                    return VERTEX_BRANCH_EXCEPTION;
                }
                return VERTEX_BRANCH_INTERRUPT;
            }

            if (isCall)
            {
                return isIndirect ? VERTEX_BRANCH_INDIRECT_CALL : VERTEX_BRANCH_CALL;
            }

            if (isJump)
            {
                if (isIndirect)
                {
                    return VERTEX_BRANCH_INDIRECT_JUMP;
                }

                if (arch == CS_ARCH_ARM64 && mnem)
                {
                    const std::string_view mnemSv{mnem};
                    if (mnemSv.starts_with("b.") ||
                        mnemSv.starts_with("cb") ||
                        mnemSv.starts_with("tb"))
                    {
                        return VERTEX_BRANCH_CONDITIONAL;
                    }
                }

                return VERTEX_BRANCH_UNCONDITIONAL;
            }

            return VERTEX_BRANCH_NONE;
        }

        std::uint32_t build_flags(const cs_insn* insn, BranchType branch_type, cs_arch arch)
        {
            std::uint32_t flags{VERTEX_FLAG_NONE};
            const cs_detail* detail = insn->detail;

            if (!detail)
            {
                return flags;
            }

            switch (branch_type)
            {
                case VERTEX_BRANCH_UNCONDITIONAL:
                case VERTEX_BRANCH_LOOP:
                case VERTEX_BRANCH_INDIRECT_JUMP:
                case VERTEX_BRANCH_TABLE_SWITCH:
                    flags |= VERTEX_FLAG_BRANCH;
                    break;
                case VERTEX_BRANCH_CONDITIONAL:
                    flags |= VERTEX_FLAG_BRANCH | VERTEX_FLAG_CONDITIONAL;
                    break;
                case VERTEX_BRANCH_CALL:
                case VERTEX_BRANCH_INDIRECT_CALL:
                    flags |= VERTEX_FLAG_CALL;
                    break;
                case VERTEX_BRANCH_RETURN:
                    flags |= VERTEX_FLAG_RETURN;
                    break;
                case VERTEX_BRANCH_INTERRUPT:
                case VERTEX_BRANCH_EXCEPTION:
                    flags |= VERTEX_FLAG_DANGEROUS;
                    break;
                default:
                    break;
            }

            if (branch_type == VERTEX_BRANCH_INDIRECT_JUMP || branch_type == VERTEX_BRANCH_INDIRECT_CALL)
            {
                flags |= VERTEX_FLAG_INDIRECT;
            }

            if (arch == CS_ARCH_X86)
            {
                const cs_x86& x86 = detail->x86;
                for (std::uint8_t i = 0; i < x86.op_count; ++i)
                {
                    if (x86.operands[i].type == X86_OP_MEM)
                    {
                        if (i == 0)
                        {
                            flags |= VERTEX_FLAG_MEMORY_WRITE;
                        }
                        else
                        {
                            flags |= VERTEX_FLAG_MEMORY_READ;
                        }

                        const cs_x86_op& op = x86.operands[i];
                        if (op.mem.base == X86_REG_RSP || op.mem.base == X86_REG_ESP ||
                            op.mem.base == X86_REG_RBP || op.mem.base == X86_REG_EBP)
                        {
                            flags |= VERTEX_FLAG_STACK_OP;
                        }
                    }
                }

                const char* mnem = insn->mnemonic;
                if (mnem)
                {
                    const std::string_view mnemSv{mnem};
                    if (mnemSv.starts_with("push") || mnemSv.starts_with("pop"))
                    {
                        flags |= VERTEX_FLAG_STACK_OP;
                    }
                }
            }
            else if (arch == CS_ARCH_ARM64)
            {
                const cs_arm64& arm64 = detail->arm64;
                for (std::uint8_t i = 0; i < arm64.op_count; ++i)
                {
                    if (arm64.operands[i].type == ARM64_OP_MEM)
                    {
                        const char* mnem = insn->mnemonic;
                        if (mnem)
                        {
                            if (mnem[0] == 'l')
                            {
                                flags |= VERTEX_FLAG_MEMORY_READ;
                            }
                            else if (mnem[0] == 's')
                            {
                                flags |= VERTEX_FLAG_MEMORY_WRITE;
                            }
                        }

                        if (arm64.operands[i].mem.base == ARM64_REG_SP)
                        {
                            flags |= VERTEX_FLAG_STACK_OP;
                        }
                    }
                }
            }

            for (std::uint8_t i = 0; i < detail->groups_count; ++i)
            {
                if (detail->groups[i] == CS_GRP_PRIVILEGE)
                {
                    flags |= VERTEX_FLAG_PRIVILEGED;
                    break;
                }
            }

            return flags;
        }

        std::uint64_t extract_target_address(const cs_insn* insn, cs_arch arch)
        {
            const cs_detail* detail = insn->detail;
            if (!detail)
            {
                return 0;
            }

            if (arch == CS_ARCH_X86)
            {
                const cs_x86& x86 = detail->x86;
                for (std::uint8_t i = 0; i < x86.op_count; ++i)
                {
                    const cs_x86_op& op = x86.operands[i];

                    if (op.type == X86_OP_IMM)
                    {
                        return static_cast<std::uint64_t>(op.imm);
                    }

                    if (op.type == X86_OP_MEM && op.mem.base == X86_REG_RIP)
                    {
                        const std::uint64_t rip_after = insn->address + insn->size;
                        const std::int64_t disp = op.mem.disp;
                        return static_cast<std::uint64_t>(static_cast<std::int64_t>(rip_after) + disp);
                    }
                }
            }
            else if (arch == CS_ARCH_ARM64)
            {
                const cs_arm64& arm64 = detail->arm64;
                for (std::uint8_t i = 0; i < arm64.op_count; ++i)
                {
                    const cs_arm64_op& op = arm64.operands[i];

                    if (op.type == ARM64_OP_IMM)
                    {
                        return static_cast<std::uint64_t>(op.imm);
                    }
                }
            }

            return 0;
        }

    }

    StatusCode init_disassembler(const DisasmMode mode)
    {
        std::scoped_lock lock{g_capstone_mutex};

        if (g_initialized)
        {
            if (g_current_mode == mode)
            {
                return STATUS_OK;
            }

            if (g_capstone_handle)
            {
                cs_close(&g_capstone_handle);
                g_capstone_handle = 0;
                g_initialized = false;
            }
        }

        cs_arch arch;
        cs_mode cs_m;

        switch (mode)
        {
            case DisasmMode::X86_32:
                arch = CS_ARCH_X86;
                cs_m = CS_MODE_32;
                break;
            case DisasmMode::X86_64:
                arch = CS_ARCH_X86;
                cs_m = CS_MODE_64;
                break;
            case DisasmMode::ARM64:
                arch = CS_ARCH_ARM64;
                cs_m = CS_MODE_ARM;
                break;
            default:
                return STATUS_ERROR_INVALID_PARAMETER;
        }

        const cs_err err = cs_open(arch, cs_m, &g_capstone_handle);
        if (err != CS_ERR_OK)
        {
            g_last_capstone_error = err;
            return STATUS_ERROR_GENERAL;
        }

        cs_option(g_capstone_handle, CS_OPT_DETAIL, CS_OPT_ON);
        cs_option(g_capstone_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

        g_current_mode = mode;
        g_initialized = true;
        return STATUS_OK;
    }

    void cleanup_disassembler()
    {
        std::scoped_lock lock{g_capstone_mutex};

        if (g_initialized && g_capstone_handle)
        {
            cs_close(&g_capstone_handle);
            g_capstone_handle = 0;
            g_initialized = false;
        }
    }

    bool is_disassembler_initialized()
    {
        std::scoped_lock lock{g_capstone_mutex};
        return g_initialized;
    }

    const char* get_last_disassembler_error()
    {
        std::scoped_lock lock{g_capstone_mutex};
        return cs_strerror(g_last_capstone_error);
    }

    DisasmMode get_disassembler_mode()
    {
        std::scoped_lock lock{g_capstone_mutex};
        return g_current_mode;
    }

    StatusCode disassemble(std::uint64_t address, std::span<const std::uint8_t> code, DisassemblerResults* results)
    {
        if (!results)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        std::scoped_lock lock{g_capstone_mutex};

        if (!g_initialized)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        if (code.empty())
        {
            results->count = 0;
            return STATUS_OK;
        }

        cs_insn* insn{nullptr};
        const std::size_t count = cs_disasm(g_capstone_handle, code.data(), code.size(), address, 0, &insn);

        if (count == 0)
        {
            results->count = 0;
            return STATUS_OK;
        }

        const cs_arch arch = (g_current_mode == DisasmMode::ARM64) ? CS_ARCH_ARM64 : CS_ARCH_X86;

        const std::uint32_t to_copy = std::min(static_cast<std::uint32_t>(count), results->capacity);
        results->count = to_copy;
        results->startAddress = address;
        results->totalSize = 0;

        for (std::uint32_t i = 0; i < to_copy; ++i)
        {
            DisassemblerResult& res = results->results[i];
            const cs_insn& ins = insn[i];

            res.address = ins.address;
            res.physicalAddress = 0;
            res.size = static_cast<std::uint32_t>(ins.size);

            std::memset(res.rawBytes, 0, VERTEX_MAX_BYTES_LENGTH);
            std::copy_n(ins.bytes, std::min(static_cast<std::size_t>(ins.size),
                        static_cast<std::size_t>(VERTEX_MAX_BYTES_LENGTH)), res.rawBytes);

            copy_string(res.mnemonic, ins.mnemonic);
            copy_string(res.operands, ins.op_str);
            res.comment[0] = '\0';

            res.category = map_category(&ins, arch);
            res.branchType = map_branch_type(&ins, arch);
            res.flags = build_flags(&ins, res.branchType, arch);

            if (res.branchType != VERTEX_BRANCH_NONE && res.branchType != VERTEX_BRANCH_RETURN)
            {
                res.targetAddress = extract_target_address(&ins, arch);
                res.branchDirection = compute_branch_direction(ins.address, res.targetAddress);
            }
            else
            {
                res.targetAddress = 0;
                res.branchDirection = VERTEX_DIRECTION_NONE;
            }

            res.fallthroughAddress = ins.address + ins.size;

            res.targetSymbol[0] = '\0';
            res.sectionName[0] = '\0';
            res.executionCount = 0;
            res.timestamp = 0;
            res.xrefCount = 0;
            res.functionStart = 0;
            res.instructionIndex = i;

            results->totalSize += ins.size;
        }

        results->endAddress = address + results->totalSize;

        cs_free(insn, count);
        return STATUS_OK;
    }

    std::uint32_t disassemble_single(std::uint64_t address, std::span<const std::uint8_t> code, DisassemblerResult* result)
    {
        if (!result || code.empty())
        {
            return 0;
        }

        std::scoped_lock lock{g_capstone_mutex};

        if (!g_initialized)
        {
            return 0;
        }

        cs_insn* insn{nullptr};
        const std::size_t count = cs_disasm(g_capstone_handle, code.data(), code.size(), address, 1, &insn);

        if (count == 0)
        {
            return 0;
        }

        cs_arch arch = (g_current_mode == DisasmMode::ARM64) ? CS_ARCH_ARM64 : CS_ARCH_X86;
        const cs_insn& ins = *insn;

        result->address = ins.address;
        result->physicalAddress = 0;
        result->size = static_cast<std::uint32_t>(ins.size);

        std::memset(result->rawBytes, 0, VERTEX_MAX_BYTES_LENGTH);
        std::copy_n(ins.bytes, std::min(static_cast<std::size_t>(ins.size),
                    static_cast<std::size_t>(VERTEX_MAX_BYTES_LENGTH)), result->rawBytes);

        copy_string(result->mnemonic, ins.mnemonic);
        copy_string(result->operands, ins.op_str);
        result->comment[0] = '\0';

        result->category = map_category(&ins, arch);
        result->branchType = map_branch_type(&ins, arch);
        result->flags = build_flags(&ins, result->branchType, arch);

        if (result->branchType != VERTEX_BRANCH_NONE && result->branchType != VERTEX_BRANCH_RETURN)
        {
            result->targetAddress = extract_target_address(&ins, arch);
            result->branchDirection = compute_branch_direction(ins.address, result->targetAddress);
        }
        else
        {
            result->targetAddress = 0;
            result->branchDirection = VERTEX_DIRECTION_NONE;
        }

        result->fallthroughAddress = ins.address + ins.size;
        result->targetSymbol[0] = '\0';
        result->sectionName[0] = '\0';
        result->executionCount = 0;
        result->timestamp = 0;
        result->xrefCount = 0;
        result->functionStart = 0;
        result->instructionIndex = 0;

        const std::uint32_t size = result->size;
        cs_free(insn, 1);
        return size;
    }

    BranchDirection compute_branch_direction(std::uint64_t current_address, std::uint64_t target_address, std::uint64_t function_start, std::uint64_t function_end)
    {
        if (target_address == 0)
        {
            return VERTEX_DIRECTION_UNKNOWN;
        }

        if (target_address == current_address)
        {
            return VERTEX_DIRECTION_SELF;
        }

        if (function_start != 0 && function_end != 0)
        {
            if (target_address < function_start || target_address >= function_end)
            {
                return VERTEX_DIRECTION_OUT_OF_FUNC;
            }
        }

        if (target_address > current_address)
        {
            return VERTEX_DIRECTION_FORWARD;
        }
        else
        {
            return VERTEX_DIRECTION_BACKWARD;
        }
    }

}

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
