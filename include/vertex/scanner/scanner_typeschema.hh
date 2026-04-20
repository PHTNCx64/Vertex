//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <string>

#include <sdk/memory.h>
#include <vertex/runtime/function_registry.hh>
#include <vertex/scanner/valuetypes.hh>

namespace Vertex::Scanner
{
    enum class TypeId : std::uint32_t
    {
        Invalid = 0
    };

    inline constexpr std::uint32_t FIRST_CUSTOM_TYPE_ID{1000};

    enum class TypeKind : std::uint8_t
    {
        BuiltinNumeric,
        BuiltinString,
        PluginDefined
    };

    struct TypeSchema final
    {
        TypeId id{TypeId::Invalid};
        std::string name{};
        TypeKind kind{TypeKind::BuiltinNumeric};
        std::uint32_t valueSize{};
        const ::DataType* sdkType{nullptr};
        std::size_t sourcePluginIndex{std::numeric_limits<std::size_t>::max()};
        std::shared_ptr<Runtime::LibraryHandle> libraryKeepalive{};
    };

    [[nodiscard]] inline TypeId builtin_type_id(ValueType type) noexcept
    {
        return static_cast<TypeId>(static_cast<std::uint32_t>(type) + 1);
    }

    [[nodiscard]] inline std::shared_ptr<const TypeSchema> make_builtin_schema(ValueType type)
    {
        const auto& info = get_value_type_info(type);
        TypeSchema schema{};
        schema.id = builtin_type_id(type);
        schema.name = info.name;
        schema.kind = info.isString ? TypeKind::BuiltinString : TypeKind::BuiltinNumeric;
        schema.valueSize = static_cast<std::uint32_t>(info.size);
        return std::make_shared<const TypeSchema>(std::move(schema));
    }
}
