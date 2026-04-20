//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/type_registry.hh>

#include <algorithm>
#include <mutex>
#include <utility>

namespace Vertex::Scanner
{
    void TypeRegistry::install_builtin(TypeSchema schema)
    {
        std::scoped_lock lock{m_mutex};
        m_byName[schema.name] = schema.id;
        m_types[schema.id] = std::move(schema);
    }

    TypeId TypeRegistry::register_type(TypeSchema schema, bool& outDuplicate)
    {
        outDuplicate = false;

        const auto allocatedId = static_cast<TypeId>(m_nextCustomId.fetch_add(1, std::memory_order_relaxed));
        schema.id = allocatedId;

        std::scoped_lock lock{m_mutex};
        if (m_byName.contains(schema.name))
        {
            outDuplicate = true;
            return TypeId::Invalid;
        }
        m_byName[schema.name] = allocatedId;
        m_types[allocatedId] = std::move(schema);
        return allocatedId;
    }

    std::optional<TypeSchema> TypeRegistry::unregister_type(TypeId id, bool& outBuiltin)
    {
        outBuiltin = false;

        std::scoped_lock lock{m_mutex};
        const auto it = m_types.find(id);
        if (it == m_types.end())
        {
            return std::nullopt;
        }
        if (it->second.kind != TypeKind::PluginDefined)
        {
            outBuiltin = true;
            return std::nullopt;
        }
        auto removed = std::move(it->second);
        m_byName.erase(removed.name);
        m_types.erase(it);
        return removed;
    }

    std::vector<TypeSchema> TypeRegistry::invalidate_plugin_types(std::size_t pluginIndex)
    {
        std::vector<TypeSchema> removed{};

        std::scoped_lock lock{m_mutex};
        for (auto it = m_types.begin(); it != m_types.end();)
        {
            if (it->second.sourcePluginIndex == pluginIndex &&
                it->second.kind == TypeKind::PluginDefined)
            {
                m_byName.erase(it->second.name);
                removed.push_back(std::move(it->second));
                it = m_types.erase(it);
            }
            else
            {
                ++it;
            }
        }
        return removed;
    }

    std::optional<TypeSchema> TypeRegistry::find(TypeId id) const
    {
        std::shared_lock lock{m_mutex};
        const auto it = m_types.find(id);
        if (it == m_types.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::optional<TypeId> TypeRegistry::find_by_name(std::string_view name) const
    {
        std::shared_lock lock{m_mutex};
        const auto it = m_byName.find(std::string{name});
        if (it == m_byName.end())
        {
            return std::nullopt;
        }
        return it->second;
    }

    std::vector<TypeSchema> TypeRegistry::snapshot() const
    {
        std::vector<TypeSchema> out{};
        {
            std::shared_lock lock{m_mutex};
            out.reserve(m_types.size());
            for (const auto& [id, schema] : m_types)
            {
                out.push_back(schema);
            }
        }
        std::ranges::sort(out,
                          [](const TypeSchema& a, const TypeSchema& b)
                          {
                              return static_cast<std::uint32_t>(a.id) < static_cast<std::uint32_t>(b.id);
                          });
        return out;
    }
}
