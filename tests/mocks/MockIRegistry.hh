#pragma once

#include <gmock/gmock.h>
#include <string_view>
#include <vertex/runtime/iregistry.hh>

namespace Vertex::Testing::Mocks
{
    class MockIRegistry : public Runtime::IRegistry
    {
    public:
        ~MockIRegistry() override = default;

        MOCK_METHOD(StatusCode, register_architecture, (const ArchitectureInfo& archInfo), (override));
        MOCK_METHOD(std::optional<Runtime::ArchInfo>, get_architecture, (), (const, override));

        MOCK_METHOD(StatusCode, register_category, (const RegisterCategoryDef& category), (override));
        MOCK_METHOD(StatusCode, unregister_category, (std::string_view categoryId), (override));
        MOCK_METHOD(std::vector<Runtime::RegisterCategoryInfo>, get_categories, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterCategoryInfo>, get_category, (std::string_view categoryId), (const, override));

        MOCK_METHOD(StatusCode, register_register, (const RegisterDef& reg), (override));
        MOCK_METHOD(StatusCode, unregister_register, (std::string_view registerName), (override));
        MOCK_METHOD(std::vector<Runtime::RegisterInfo>, get_registers, (), (const, override));
        MOCK_METHOD(std::vector<Runtime::RegisterInfo>, get_registers_by_category, (std::string_view categoryId), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterInfo>, get_register, (std::string_view registerName), (const, override));

        MOCK_METHOD(StatusCode, register_flag_bit, (const FlagBitDef& flagBit), (override));
        MOCK_METHOD(std::vector<Runtime::FlagBitInfo>, get_flag_bits, (std::string_view flagsRegisterName), (const, override));

        MOCK_METHOD(StatusCode, register_exception_type, (const ExceptionTypeDef& exceptionType), (override));
        MOCK_METHOD(std::vector<Runtime::ExceptionTypeInfo>, get_exception_types, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::ExceptionTypeInfo>, get_exception_type, (std::uint32_t code), (const, override));

        MOCK_METHOD(StatusCode, register_calling_convention, (const CallingConventionDef& callingConv), (override));
        MOCK_METHOD(std::vector<Runtime::CallingConventionInfo>, get_calling_conventions, (), (const, override));

        MOCK_METHOD(StatusCode, register_snapshot, (const RegistrySnapshot& snapshot), (override));
        MOCK_METHOD(void, clear, (), (override));

        MOCK_METHOD(bool, has_register, (std::string_view registerName), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterInfo>, get_program_counter, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterInfo>, get_stack_pointer, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterInfo>, get_frame_pointer, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::RegisterInfo>, get_flags_register, (), (const, override));
    };
}
