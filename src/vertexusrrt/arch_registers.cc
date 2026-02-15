//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/arch_registers.hh>
#include <vertexusrrt/util.hh>

#include <algorithm>
#include <format>
#include <string>
#include <string_view>

namespace PluginRuntime
{
    namespace
    {
        template<std::size_t N>
        constexpr void copy_to_fixed(char (&dest)[N], std::string_view src)
        {
            const auto len = std::min(src.size(), N - 1);
            std::copy_n(src.data(), len, dest);
            dest[len] = '\0';
        }

        void make_category(const Runtime* runtime, std::string_view id, std::string_view name,
                           std::uint32_t order, bool collapsed)
        {
            RegisterCategoryDef cat{};
            copy_to_fixed(cat.categoryId, id);
            copy_to_fixed(cat.displayName, name);
            cat.displayOrder = order;
            cat.collapsedByDefault = collapsed ? 1 : 0;
            runtime->vertex_register_category(&cat);
        }

        using RegWriteFunc = void(VERTEX_API*)(void* in, size_t size);
        using RegReadFunc = void(VERTEX_API*)(void* out, size_t size);

        void make_register(const Runtime* runtime, std::string_view catId, std::string_view regName,
                           std::string_view parent, std::uint8_t bits, std::uint8_t offset,
                           std::uint16_t flags, std::uint32_t order, std::uint32_t regId,
                           RegWriteFunc writeFunc = nullptr, RegReadFunc readFunc = nullptr)
        {
            RegisterDef reg{};
            copy_to_fixed(reg.categoryId, catId);
            copy_to_fixed(reg.name, regName);
            copy_to_fixed(reg.parentName, parent);
            reg.bitWidth = bits;
            reg.bitOffset = offset;
            reg.flags = flags;
            reg.displayOrder = order;
            reg.registerId = regId;
            reg.write_func = writeFunc;
            reg.read_func = readFunc;
            runtime->vertex_register_register(&reg);
        }

        void make_flag_bit(const Runtime* runtime, std::string_view flagsReg, std::string_view bitName,
                           std::string_view desc, std::uint8_t pos)
        {
            FlagBitDef fb{};
            copy_to_fixed(fb.flagsRegisterName, flagsReg);
            copy_to_fixed(fb.bitName, bitName);
            copy_to_fixed(fb.description, desc);
            fb.bitPosition = pos;
            runtime->vertex_register_flag_bit(&fb);
        }

        void make_exception(const Runtime* runtime, std::uint32_t code, std::string_view name,
                            std::string_view desc, bool fatal)
        {
            ExceptionTypeDef ex{};
            ex.exceptionCode = code;
            copy_to_fixed(ex.name, name);
            copy_to_fixed(ex.description, desc);
            ex.isFatal = fatal ? 1 : 0;
            runtime->vertex_register_exception_type(&ex);
        }

        StatusCode register_x86(const Runtime* runtime)
        {
            ArchitectureInfo archInfo{};
            archInfo.endianness = VERTEX_ENDIAN_LITTLE;
            archInfo.preferredSyntax = VERTEX_DISASM_SYNTAX_INTEL;
            archInfo.addressWidth = 32;
            archInfo.maxHardwareBreakpoints = 4;
            archInfo.stackGrowsDown = true;
            copy_to_fixed(archInfo.architectureName, "x86");
            runtime->vertex_register_architecture(&archInfo);

            make_category(runtime, "gp", "General Purpose", 0, false);
            make_category(runtime, "seg", "Segment", 1, false);
            make_category(runtime, "flags", "Flags", 2, false);
            make_category(runtime, "fpu", "FPU", 3, true);
            make_category(runtime, "sse", "SSE", 4, true);
            make_category(runtime, "debug", "Debug", 5, true);

            make_register(runtime, "gp", "EAX", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 0, 0);
            make_register(runtime, "gp", "EBX", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 1, 1);
            make_register(runtime, "gp", "ECX", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 2, 2);
            make_register(runtime, "gp", "EDX", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 3, 3);
            make_register(runtime, "gp", "ESI", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 4, 4);
            make_register(runtime, "gp", "EDI", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_NONE, 5, 5);
            make_register(runtime, "gp", "EBP", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_FRAME_POINTER, 6, 6);
            make_register(runtime, "gp", "ESP", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_STACK_POINTER, 7, 7);
            make_register(runtime, "gp", "EIP", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_PROGRAM_COUNTER, 8, 8);

            make_register(runtime, "gp", "AX", "EAX", 16, 0, VERTEX_REG_FLAG_HIDDEN, 10, 10);
            make_register(runtime, "gp", "BX", "EBX", 16, 0, VERTEX_REG_FLAG_HIDDEN, 11, 11);
            make_register(runtime, "gp", "CX", "ECX", 16, 0, VERTEX_REG_FLAG_HIDDEN, 12, 12);
            make_register(runtime, "gp", "DX", "EDX", 16, 0, VERTEX_REG_FLAG_HIDDEN, 13, 13);
            make_register(runtime, "gp", "SI", "ESI", 16, 0, VERTEX_REG_FLAG_HIDDEN, 14, 14);
            make_register(runtime, "gp", "DI", "EDI", 16, 0, VERTEX_REG_FLAG_HIDDEN, 15, 15);
            make_register(runtime, "gp", "BP", "EBP", 16, 0, VERTEX_REG_FLAG_HIDDEN, 16, 16);
            make_register(runtime, "gp", "SP", "ESP", 16, 0, VERTEX_REG_FLAG_HIDDEN, 17, 17);

            make_register(runtime, "gp", "AL", "EAX", 8, 0, VERTEX_REG_FLAG_HIDDEN, 20, 20);
            make_register(runtime, "gp", "AH", "EAX", 8, 8, VERTEX_REG_FLAG_HIDDEN, 21, 21);
            make_register(runtime, "gp", "BL", "EBX", 8, 0, VERTEX_REG_FLAG_HIDDEN, 22, 22);
            make_register(runtime, "gp", "BH", "EBX", 8, 8, VERTEX_REG_FLAG_HIDDEN, 23, 23);
            make_register(runtime, "gp", "CL", "ECX", 8, 0, VERTEX_REG_FLAG_HIDDEN, 24, 24);
            make_register(runtime, "gp", "CH", "ECX", 8, 8, VERTEX_REG_FLAG_HIDDEN, 25, 25);
            make_register(runtime, "gp", "DL", "EDX", 8, 0, VERTEX_REG_FLAG_HIDDEN, 26, 26);
            make_register(runtime, "gp", "DH", "EDX", 8, 8, VERTEX_REG_FLAG_HIDDEN, 27, 27);

            make_register(runtime, "seg", "CS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 0, 30);
            make_register(runtime, "seg", "DS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 1, 31);
            make_register(runtime, "seg", "ES", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 2, 32);
            make_register(runtime, "seg", "FS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 3, 33);
            make_register(runtime, "seg", "GS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 4, 34);
            make_register(runtime, "seg", "SS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 5, 35);

            make_register(runtime, "flags", "EFLAGS", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_FLAGS_REGISTER, 0, 40);
            make_flag_bit(runtime, "EFLAGS", "CF", "Carry Flag", 0);
            make_flag_bit(runtime, "EFLAGS", "PF", "Parity Flag", 2);
            make_flag_bit(runtime, "EFLAGS", "AF", "Auxiliary Carry Flag", 4);
            make_flag_bit(runtime, "EFLAGS", "ZF", "Zero Flag", 6);
            make_flag_bit(runtime, "EFLAGS", "SF", "Sign Flag", 7);
            make_flag_bit(runtime, "EFLAGS", "TF", "Trap Flag", 8);
            make_flag_bit(runtime, "EFLAGS", "IF", "Interrupt Enable Flag", 9);
            make_flag_bit(runtime, "EFLAGS", "DF", "Direction Flag", 10);
            make_flag_bit(runtime, "EFLAGS", "OF", "Overflow Flag", 11);

            for (int i = 0; i < 8; ++i)
            {
                make_register(runtime, "sse", std::format("XMM{}", i), EMPTY_STRING, 128, 0, VERTEX_REG_FLAG_VECTOR, i, 50 + i);
            }

            make_register(runtime, "debug", "DR0", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 0, 70);
            make_register(runtime, "debug", "DR1", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 1, 71);
            make_register(runtime, "debug", "DR2", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 2, 72);
            make_register(runtime, "debug", "DR3", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 3, 73);
            make_register(runtime, "debug", "DR6", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 4, 76);
            make_register(runtime, "debug", "DR7", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_HIDDEN, 5, 77);

            return STATUS_OK;
        }

        StatusCode register_x86_64(const Runtime* runtime)
        {
            ArchitectureInfo archInfo{};
            archInfo.endianness = VERTEX_ENDIAN_LITTLE;
            archInfo.preferredSyntax = VERTEX_DISASM_SYNTAX_INTEL;
            archInfo.addressWidth = 64;
            archInfo.maxHardwareBreakpoints = 4;
            archInfo.stackGrowsDown = true;
            copy_to_fixed(archInfo.architectureName, "x86-64");
            runtime->vertex_register_architecture(&archInfo);

            make_category(runtime, "gp", "General Purpose", 0, false);
            make_category(runtime, "seg", "Segment", 1, false);
            make_category(runtime, "flags", "Flags", 2, false);
            make_category(runtime, "fpu", "FPU", 3, true);
            make_category(runtime, "sse", "SSE/AVX", 4, true);
            make_category(runtime, "debug", "Debug", 5, true);

            make_register(runtime, "gp", "RAX", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 0, 0);
            make_register(runtime, "gp", "RBX", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 1, 1);
            make_register(runtime, "gp", "RCX", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 2, 2);
            make_register(runtime, "gp", "RDX", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 3, 3);
            make_register(runtime, "gp", "RSI", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 4, 4);
            make_register(runtime, "gp", "RDI", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 5, 5);
            make_register(runtime, "gp", "RBP", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_FRAME_POINTER, 6, 6);
            make_register(runtime, "gp", "RSP", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_STACK_POINTER, 7, 7);
            for (int i = 8; i <= 15; ++i)
            {
                make_register(runtime, "gp", std::format("R{}", i), EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, i, i);
            }
            make_register(runtime, "gp", "RIP", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_PROGRAM_COUNTER, 16, 16);

            make_register(runtime, "gp", "EAX", "RAX", 32, 0, VERTEX_REG_FLAG_HIDDEN, 20, 20);
            make_register(runtime, "gp", "EBX", "RBX", 32, 0, VERTEX_REG_FLAG_HIDDEN, 21, 21);
            make_register(runtime, "gp", "ECX", "RCX", 32, 0, VERTEX_REG_FLAG_HIDDEN, 22, 22);
            make_register(runtime, "gp", "EDX", "RDX", 32, 0, VERTEX_REG_FLAG_HIDDEN, 23, 23);
            make_register(runtime, "gp", "ESI", "RSI", 32, 0, VERTEX_REG_FLAG_HIDDEN, 24, 24);
            make_register(runtime, "gp", "EDI", "RDI", 32, 0, VERTEX_REG_FLAG_HIDDEN, 25, 25);
            make_register(runtime, "gp", "EBP", "RBP", 32, 0, VERTEX_REG_FLAG_HIDDEN, 26, 26);
            make_register(runtime, "gp", "ESP", "RSP", 32, 0, VERTEX_REG_FLAG_HIDDEN, 27, 27);
            for (int i = 8; i <= 15; ++i)
            {
                make_register(runtime, "gp", std::format("R{}D", i), std::format("R{}", i), 32, 0, VERTEX_REG_FLAG_HIDDEN, 20 + i, 20 + i);
            }

            make_register(runtime, "seg", "CS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 0, 50);
            make_register(runtime, "seg", "DS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 1, 51);
            make_register(runtime, "seg", "ES", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 2, 52);
            make_register(runtime, "seg", "FS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 3, 53);
            make_register(runtime, "seg", "GS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 4, 54);
            make_register(runtime, "seg", "SS", EMPTY_STRING, 16, 0, VERTEX_REG_FLAG_SEGMENT, 5, 55);

            make_register(runtime, "flags", "RFLAGS", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_FLAGS_REGISTER, 0, 60);
            make_flag_bit(runtime, "RFLAGS", "CF", "Carry Flag", 0);
            make_flag_bit(runtime, "RFLAGS", "PF", "Parity Flag", 2);
            make_flag_bit(runtime, "RFLAGS", "AF", "Auxiliary Carry Flag", 4);
            make_flag_bit(runtime, "RFLAGS", "ZF", "Zero Flag", 6);
            make_flag_bit(runtime, "RFLAGS", "SF", "Sign Flag", 7);
            make_flag_bit(runtime, "RFLAGS", "TF", "Trap Flag", 8);
            make_flag_bit(runtime, "RFLAGS", "IF", "Interrupt Enable Flag", 9);
            make_flag_bit(runtime, "RFLAGS", "DF", "Direction Flag", 10);
            make_flag_bit(runtime, "RFLAGS", "OF", "Overflow Flag", 11);

            for (int i = 0; i < 16; ++i)
            {
                make_register(runtime, "sse", std::format("XMM{}", i), EMPTY_STRING, 128, 0, VERTEX_REG_FLAG_VECTOR, i, 70 + i);
            }

            make_register(runtime, "debug", "DR0", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 0, 100);
            make_register(runtime, "debug", "DR1", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 1, 101);
            make_register(runtime, "debug", "DR2", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 2, 102);
            make_register(runtime, "debug", "DR3", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 3, 103);
            make_register(runtime, "debug", "DR6", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 4, 106);
            make_register(runtime, "debug", "DR7", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_HIDDEN, 5, 107);

            return STATUS_OK;
        }

        StatusCode register_arm64(const Runtime* runtime)
        {
            ArchitectureInfo archInfo{};
            archInfo.endianness = VERTEX_ENDIAN_LITTLE;
            archInfo.preferredSyntax = VERTEX_DISASM_SYNTAX_CUSTOM;
            archInfo.addressWidth = 64;
            archInfo.maxHardwareBreakpoints = 6;
            archInfo.stackGrowsDown = true;
            copy_to_fixed(archInfo.architectureName, "ARM64");
            runtime->vertex_register_architecture(&archInfo);

            make_category(runtime, "gp", "General Purpose", 0, false);
            make_category(runtime, "special", "Special", 1, false);
            make_category(runtime, "flags", "Flags", 2, false);
            make_category(runtime, "simd", "SIMD/FP", 3, true);

            for (int i = 0; i <= 28; ++i)
            {
                make_register(runtime, "gp", std::format("X{}", i), EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, i, i);
            }
            make_register(runtime, "gp", "X29", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_FRAME_POINTER, 29, 29);
            make_register(runtime, "gp", "FP", "X29", 64, 0, VERTEX_REG_FLAG_HIDDEN | VERTEX_REG_FLAG_FRAME_POINTER, 30, 129);
            make_register(runtime, "gp", "X30", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_NONE, 31, 30);
            make_register(runtime, "gp", "LR", "X30", 64, 0, VERTEX_REG_FLAG_HIDDEN, 32, 130);

            make_register(runtime, "special", "SP", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_STACK_POINTER, 0, 31);
            make_register(runtime, "special", "PC", EMPTY_STRING, 64, 0, VERTEX_REG_FLAG_PROGRAM_COUNTER, 1, 32);

            for (int i = 0; i <= 30; ++i)
            {
                make_register(runtime, "gp", std::format("W{}", i), std::format("X{}", i), 32, 0, VERTEX_REG_FLAG_HIDDEN, 40 + i, 40 + i);
            }

            make_register(runtime, "flags", "NZCV", EMPTY_STRING, 32, 0, VERTEX_REG_FLAG_FLAGS_REGISTER, 0, 80);
            make_flag_bit(runtime, "NZCV", "N", "Negative Flag", 31);
            make_flag_bit(runtime, "NZCV", "Z", "Zero Flag", 30);
            make_flag_bit(runtime, "NZCV", "C", "Carry Flag", 29);
            make_flag_bit(runtime, "NZCV", "V", "Overflow Flag", 28);

            for (int i = 0; i < 32; ++i)
            {
                make_register(runtime, "simd", std::format("V{}", i), EMPTY_STRING, 128, 0, VERTEX_REG_FLAG_VECTOR | VERTEX_REG_FLAG_FLOATING_POINT, i, 100 + i);
            }

            return STATUS_OK;
        }

    } // anonymous namespace

#ifdef _WIN32
    ProcessArchitecture detect_process_architecture(HANDLE processHandle)
    {
        if (!processHandle || processHandle == INVALID_HANDLE_VALUE)
        {
            return ProcessArchitecture::Unknown;
        }

        using IsWow64Process2Fn = BOOL(WINAPI*)(HANDLE, USHORT*, USHORT*);
        static auto pIsWow64Process2 = reinterpret_cast<IsWow64Process2Fn>(
            GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "IsWow64Process2"));

        if (pIsWow64Process2)
        {
            USHORT processMachine = IMAGE_FILE_MACHINE_UNKNOWN;
            USHORT nativeMachine = IMAGE_FILE_MACHINE_UNKNOWN;

            if (pIsWow64Process2(processHandle, &processMachine, &nativeMachine))
            {
                if (processMachine == IMAGE_FILE_MACHINE_UNKNOWN)
                {
                    switch (nativeMachine)
                    {
                        case IMAGE_FILE_MACHINE_AMD64: return ProcessArchitecture::X86_64;
                        case IMAGE_FILE_MACHINE_ARM64: return ProcessArchitecture::ARM64;
                        case IMAGE_FILE_MACHINE_I386:  return ProcessArchitecture::X86;
                        default: return ProcessArchitecture::Unknown;
                    }
                }
                else
                {
                    if (processMachine == IMAGE_FILE_MACHINE_I386)
                    {
                        return nativeMachine == IMAGE_FILE_MACHINE_ARM64
                            ? ProcessArchitecture::ARM64_X86
                            : ProcessArchitecture::X86;
                    }
                    return ProcessArchitecture::Unknown;
                }
            }
        }

        BOOL isWow64 = FALSE;
        if (IsWow64Process(processHandle, &isWow64))
        {
            if (isWow64)
            {
                return ProcessArchitecture::X86;
            }

            SYSTEM_INFO sysInfo;
            GetNativeSystemInfo(&sysInfo);
            switch (sysInfo.wProcessorArchitecture)
            {
                case PROCESSOR_ARCHITECTURE_AMD64: return ProcessArchitecture::X86_64;
                case PROCESSOR_ARCHITECTURE_ARM64: return ProcessArchitecture::ARM64;
                case PROCESSOR_ARCHITECTURE_INTEL: return ProcessArchitecture::X86;
                default: return ProcessArchitecture::Unknown;
            }
        }

        return ProcessArchitecture::Unknown;
    }
#endif

    const char* get_architecture_name(ProcessArchitecture arch)
    {
        switch (arch)
        {
            case ProcessArchitecture::X86:       return "x86 (32-bit)";
            case ProcessArchitecture::X86_64:    return "x86-64 (64-bit)";
            case ProcessArchitecture::ARM64:     return "ARM64";
            case ProcessArchitecture::ARM64_X86: return "x86 on ARM64";
            default:                             return "Unknown";
        }
    }

    StatusCode register_architecture(const Runtime* runtime, ProcessArchitecture arch)
    {
        if (!runtime)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        StatusCode status;
        switch (arch)
        {
            case ProcessArchitecture::X86:
            case ProcessArchitecture::ARM64_X86:
                status = register_x86(runtime);
                break;
            case ProcessArchitecture::X86_64:
                status = register_x86_64(runtime);
                break;
            case ProcessArchitecture::ARM64:
                status = register_arm64(runtime);
                break;
            default:
                status = register_x86_64(runtime);
                break;
        }

        if (status == STATUS_OK)
        {
            status = register_windows_exceptions(runtime);
        }

        return status;
    }

    StatusCode register_windows_exceptions(const Runtime* runtime)
    {
        if (!runtime)
        {
            return STATUS_ERROR_INVALID_PARAMETER;
        }

        make_exception(runtime, 0xC0000005, "ACCESS_VIOLATION", "Memory access violation", true);
        make_exception(runtime, 0xC000001D, "ILLEGAL_INSTRUCTION", "Illegal instruction executed", true);
        make_exception(runtime, 0xC0000094, "INT_DIVIDE_BY_ZERO", "Integer divide by zero", true);
        make_exception(runtime, 0xC0000095, "INT_OVERFLOW", "Integer overflow", true);
        make_exception(runtime, 0xC000008C, "ARRAY_BOUNDS_EXCEEDED", "Array bounds exceeded", true);
        make_exception(runtime, 0xC000008D, "FLT_DENORMAL_OPERAND", "Floating-point denormal operand", true);
        make_exception(runtime, 0xC000008E, "FLT_DIVIDE_BY_ZERO", "Floating-point divide by zero", true);
        make_exception(runtime, 0xC000008F, "FLT_INEXACT_RESULT", "Floating-point inexact result", false);
        make_exception(runtime, 0xC0000090, "FLT_INVALID_OPERATION", "Floating-point invalid operation", true);
        make_exception(runtime, 0xC0000091, "FLT_OVERFLOW", "Floating-point overflow", true);
        make_exception(runtime, 0xC0000092, "FLT_STACK_CHECK", "Floating-point stack check", true);
        make_exception(runtime, 0xC0000093, "FLT_UNDERFLOW", "Floating-point underflow", false);
        make_exception(runtime, 0xC00000FD, "STACK_OVERFLOW", "Stack overflow", true);
        make_exception(runtime, 0x80000001, "GUARD_PAGE", "Guard page accessed", false);
        make_exception(runtime, 0x80000003, "BREAKPOINT", "Software breakpoint", false);
        make_exception(runtime, 0x80000004, "SINGLE_STEP", "Single step", false);
        make_exception(runtime, 0x406D1388, "MS_VC_EXCEPTION", "Visual C++ SetThreadName", false);

        return STATUS_OK;
    }

} // namespace PluginRuntime
