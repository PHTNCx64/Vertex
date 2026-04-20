//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/scanner/scanner_typeschema.hh>

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <shared_mutex>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Vertex::Scanner
{
    class TypeRegistry final
    {
      public:
        TypeRegistry() = default;
        ~TypeRegistry() = default;

        TypeRegistry(const TypeRegistry&) = delete;
        TypeRegistry& operator=(const TypeRegistry&) = delete;
        TypeRegistry(TypeRegistry&&) = delete;
        TypeRegistry& operator=(TypeRegistry&&) = delete;

        void install_builtin(TypeSchema schema);

        [[nodiscard]] TypeId register_type(TypeSchema schema, bool& outDuplicate);
        [[nodiscard]] std::optional<TypeSchema> unregister_type(TypeId id, bool& outBuiltin);

        [[nodiscard]] std::vector<TypeSchema> invalidate_plugin_types(std::size_t pluginIndex);

        [[nodiscard]] std::optional<TypeSchema> find(TypeId id) const;
        [[nodiscard]] std::optional<TypeId> find_by_name(std::string_view name) const;
        [[nodiscard]] std::vector<TypeSchema> snapshot() const;

      private:
        mutable std::shared_mutex m_mutex{};
        std::unordered_map<TypeId, TypeSchema> m_types{};
        std::unordered_map<std::string, TypeId> m_byName{};
        std::atomic<std::uint32_t> m_nextCustomId{FIRST_CUSTOM_TYPE_ID};
    };
}
