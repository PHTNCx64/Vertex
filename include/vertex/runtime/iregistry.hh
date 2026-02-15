//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <sdk/registry.h>
#include <sdk/statuscode.h>

#include <string>
#include <vector>
#include <optional>

namespace Vertex::Runtime
{
    struct RegisterCategoryInfo final
    {
        std::string categoryId;
        std::string displayName;
        std::uint32_t displayOrder{};
        bool collapsedByDefault{};
    };

    using RegisterWriteFunc = void(VERTEX_API*)(void* in, std::size_t size);
    using RegisterReadFunc = void(VERTEX_API*)(void* out, std::size_t size);

    struct RegisterInfo final
    {
        std::string categoryId;
        std::string name;
        std::string parentName;
        std::uint8_t bitWidth{64};
        std::uint8_t bitOffset{};
        std::uint16_t flags{};
        std::uint32_t displayOrder{};
        std::uint32_t registerId{};
        RegisterWriteFunc writeFunc{};
        RegisterReadFunc readFunc{};
    };

    struct FlagBitInfo final
    {
        std::string flagsRegisterName;
        std::string bitName;
        std::string description;
        std::uint8_t bitPosition{};
    };

    struct ExceptionTypeInfo final
    {
        std::uint32_t exceptionCode{};
        std::string name;
        std::string description;
        bool isFatal{};
    };

    struct CallingConventionInfo final
    {
        std::string name;
        std::vector<std::string> parameterRegisters;
        std::string returnRegister;
        bool stackCleanupByCallee{};
    };

    struct ArchInfo final
    {
        Endianness endianness{VERTEX_ENDIAN_LITTLE};
        DisasmSyntax preferredSyntax{VERTEX_DISASM_SYNTAX_INTEL};
        std::uint8_t addressWidth{64};
        std::uint8_t maxHardwareBreakpoints{4};
        bool stackGrowsDown{true};
        std::string architectureName;
    };

    class IRegistry
    {
    public:
        virtual ~IRegistry() = default;

        virtual StatusCode register_architecture(const ArchitectureInfo& archInfo) = 0;
        [[nodiscard]] virtual std::optional<ArchInfo> get_architecture() const = 0;

        virtual StatusCode register_category(const RegisterCategoryDef& category) = 0;
        virtual StatusCode unregister_category(std::string_view categoryId) = 0;
        [[nodiscard]] virtual std::vector<RegisterCategoryInfo> get_categories() const = 0;
        [[nodiscard]] virtual std::optional<RegisterCategoryInfo> get_category(std::string_view categoryId) const = 0;

        virtual StatusCode register_register(const RegisterDef& reg) = 0;
        virtual StatusCode unregister_register(std::string_view registerName) = 0;
        [[nodiscard]] virtual std::vector<RegisterInfo> get_registers() const = 0;
        [[nodiscard]] virtual std::vector<RegisterInfo> get_registers_by_category(std::string_view categoryId) const = 0;
        [[nodiscard]] virtual std::optional<RegisterInfo> get_register(std::string_view registerName) const = 0;

        virtual StatusCode register_flag_bit(const FlagBitDef& flagBit) = 0;
        [[nodiscard]] virtual std::vector<FlagBitInfo> get_flag_bits(std::string_view flagsRegisterName) const = 0;

        virtual StatusCode register_exception_type(const ExceptionTypeDef& exceptionType) = 0;
        [[nodiscard]] virtual std::vector<ExceptionTypeInfo> get_exception_types() const = 0;
        [[nodiscard]] virtual std::optional<ExceptionTypeInfo> get_exception_type(std::uint32_t code) const = 0;

        virtual StatusCode register_calling_convention(const CallingConventionDef& callingConv) = 0;
        [[nodiscard]] virtual std::vector<CallingConventionInfo> get_calling_conventions() const = 0;

        virtual StatusCode register_snapshot(const RegistrySnapshot& snapshot) = 0;
        virtual void clear() = 0;

        [[nodiscard]] virtual bool has_register(std::string_view registerName) const = 0;
        [[nodiscard]] virtual std::optional<RegisterInfo> get_program_counter() const = 0;
        [[nodiscard]] virtual std::optional<RegisterInfo> get_stack_pointer() const = 0;
        [[nodiscard]] virtual std::optional<RegisterInfo> get_frame_pointer() const = 0;
        [[nodiscard]] virtual std::optional<RegisterInfo> get_flags_register() const = 0;
    };
}
