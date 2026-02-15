//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/iregistry.hh>

#include <mutex>
#include <unordered_map>

namespace Vertex::Runtime
{
    class Registry final : public IRegistry
    {
    public:
        Registry() = default;
        ~Registry() override = default;

        StatusCode register_architecture(const ArchitectureInfo& archInfo) override;
        [[nodiscard]] std::optional<ArchInfo> get_architecture() const override;

        StatusCode register_category(const RegisterCategoryDef& category) override;
        StatusCode unregister_category(std::string_view categoryId) override;
        [[nodiscard]] std::vector<RegisterCategoryInfo> get_categories() const override;
        [[nodiscard]] std::optional<RegisterCategoryInfo> get_category(std::string_view categoryId) const override;

        StatusCode register_register(const RegisterDef& reg) override;
        StatusCode unregister_register(std::string_view registerName) override;
        [[nodiscard]] std::vector<RegisterInfo> get_registers() const override;
        [[nodiscard]] std::vector<RegisterInfo> get_registers_by_category(std::string_view categoryId) const override;
        [[nodiscard]] std::optional<RegisterInfo> get_register(std::string_view registerName) const override;

        StatusCode register_flag_bit(const FlagBitDef& flagBit) override;
        [[nodiscard]] std::vector<FlagBitInfo> get_flag_bits(std::string_view flagsRegisterName) const override;

        StatusCode register_exception_type(const ExceptionTypeDef& exceptionType) override;
        [[nodiscard]] std::vector<ExceptionTypeInfo> get_exception_types() const override;
        [[nodiscard]] std::optional<ExceptionTypeInfo> get_exception_type(std::uint32_t code) const override;

        StatusCode register_calling_convention(const CallingConventionDef& callingConv) override;
        [[nodiscard]] std::vector<CallingConventionInfo> get_calling_conventions() const override;

        StatusCode register_snapshot(const RegistrySnapshot& snapshot) override;
        void clear() override;

        [[nodiscard]] bool has_register(std::string_view registerName) const override;
        [[nodiscard]] std::optional<RegisterInfo> get_program_counter() const override;
        [[nodiscard]] std::optional<RegisterInfo> get_stack_pointer() const override;
        [[nodiscard]] std::optional<RegisterInfo> get_frame_pointer() const override;
        [[nodiscard]] std::optional<RegisterInfo> get_flags_register() const override;

    private:
        mutable std::mutex m_mutex{};

        std::optional<ArchInfo> m_archInfo{};
        std::unordered_map<std::string, RegisterCategoryInfo> m_categories{};
        std::unordered_map<std::string, RegisterInfo> m_registers{};
        std::unordered_map<std::string, std::vector<FlagBitInfo>> m_flagBits{};
        std::unordered_map<std::uint32_t, ExceptionTypeInfo> m_exceptionTypes{};
        std::vector<CallingConventionInfo> m_callingConventions{};

        mutable std::optional<std::string> m_programCounterName{};
        mutable std::optional<std::string> m_stackPointerName{};
        mutable std::optional<std::string> m_framePointerName{};
        mutable std::optional<std::string> m_flagsRegisterName{};
        mutable bool m_specialRegistersCached{};

        void cache_special_registers() const;
    };
}
