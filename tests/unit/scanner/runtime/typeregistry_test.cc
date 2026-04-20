//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/scanner/type_registry.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <algorithm>
#include <string>

namespace
{
    using Vertex::Scanner::TypeId;
    using Vertex::Scanner::TypeKind;
    using Vertex::Scanner::TypeRegistry;
    using Vertex::Scanner::TypeSchema;
    using Vertex::Scanner::FIRST_CUSTOM_TYPE_ID;

    [[nodiscard]] TypeSchema make_plugin_schema(std::string name, std::size_t pluginIndex = 0, std::uint32_t valueSize = 4)
    {
        TypeSchema s{};
        s.name = std::move(name);
        s.kind = TypeKind::PluginDefined;
        s.valueSize = valueSize;
        s.sourcePluginIndex = pluginIndex;
        return s;
    }

    [[nodiscard]] TypeSchema make_builtin_schema(TypeId id, std::string name)
    {
        TypeSchema s{};
        s.id = id;
        s.name = std::move(name);
        s.kind = TypeKind::BuiltinNumeric;
        s.valueSize = 4;
        s.sourcePluginIndex = std::numeric_limits<std::size_t>::max();
        return s;
    }
}

TEST(TypeRegistryTest, RegisterAssignsIncreasingCustomIds)
{
    TypeRegistry registry{};
    bool dup{false};

    const auto first = registry.register_type(make_plugin_schema("A"), dup);
    EXPECT_FALSE(dup);
    EXPECT_EQ(static_cast<std::uint32_t>(first), FIRST_CUSTOM_TYPE_ID);

    const auto second = registry.register_type(make_plugin_schema("B"), dup);
    EXPECT_FALSE(dup);
    EXPECT_EQ(static_cast<std::uint32_t>(second), FIRST_CUSTOM_TYPE_ID + 1);
}

TEST(TypeRegistryTest, RegisterDuplicateNameRejected)
{
    TypeRegistry registry{};
    bool dup{false};

    const auto first = registry.register_type(make_plugin_schema("DupName"), dup);
    ASSERT_NE(first, TypeId::Invalid);
    EXPECT_FALSE(dup);

    const auto second = registry.register_type(make_plugin_schema("DupName"), dup);
    EXPECT_TRUE(dup);
    EXPECT_EQ(second, TypeId::Invalid);
}

TEST(TypeRegistryTest, UnregisterPluginTypeRemovesByIdAndName)
{
    TypeRegistry registry{};
    bool dup{false};
    const auto id = registry.register_type(make_plugin_schema("RemoveMe"), dup);
    ASSERT_NE(id, TypeId::Invalid);

    bool builtin{false};
    const auto removed = registry.unregister_type(id, builtin);
    ASSERT_TRUE(removed.has_value());
    EXPECT_EQ(removed->name, "RemoveMe");
    EXPECT_FALSE(builtin);

    EXPECT_FALSE(registry.find(id).has_value());
    EXPECT_FALSE(registry.find_by_name("RemoveMe").has_value());
}

TEST(TypeRegistryTest, UnregisterBuiltinRejected)
{
    TypeRegistry registry{};
    registry.install_builtin(make_builtin_schema(static_cast<TypeId>(1), "Int8"));

    bool builtin{false};
    const auto removed = registry.unregister_type(static_cast<TypeId>(1), builtin);
    EXPECT_FALSE(removed.has_value());
    EXPECT_TRUE(builtin);
    EXPECT_TRUE(registry.find(static_cast<TypeId>(1)).has_value());
}

TEST(TypeRegistryTest, UnregisterUnknownReturnsEmpty)
{
    TypeRegistry registry{};
    bool builtin{false};
    EXPECT_FALSE(registry.unregister_type(static_cast<TypeId>(9999), builtin).has_value());
    EXPECT_FALSE(builtin);
}

TEST(TypeRegistryTest, InvalidatePluginTypesRemovesOnlyMatching)
{
    TypeRegistry registry{};
    bool dup{false};
    registry.install_builtin(make_builtin_schema(static_cast<TypeId>(1), "Int8"));
    (void) registry.register_type(make_plugin_schema("A", 2), dup);
    (void) registry.register_type(make_plugin_schema("B", 2), dup);
    (void) registry.register_type(make_plugin_schema("C", 7), dup);

    const auto removed = registry.invalidate_plugin_types(2);
    EXPECT_EQ(removed.size(), 2u);

    const auto snapshot = registry.snapshot();
    
    EXPECT_EQ(snapshot.size(), 2u);
    EXPECT_TRUE(registry.find_by_name("Int8").has_value());
    EXPECT_TRUE(registry.find_by_name("C").has_value());
    EXPECT_FALSE(registry.find_by_name("A").has_value());
    EXPECT_FALSE(registry.find_by_name("B").has_value());
}

TEST(TypeRegistryTest, InvalidatePluginTypesSparesBuiltinsEvenWithMatchingIndex)
{
    TypeRegistry registry{};
    TypeSchema misconfiguredBuiltin{};
    misconfiguredBuiltin.id = static_cast<TypeId>(5);
    misconfiguredBuiltin.name = "FakeBuiltin";
    misconfiguredBuiltin.kind = TypeKind::BuiltinNumeric;
    misconfiguredBuiltin.sourcePluginIndex = 2;
    registry.install_builtin(misconfiguredBuiltin);

    const auto removed = registry.invalidate_plugin_types(2);
    EXPECT_TRUE(removed.empty());
    EXPECT_TRUE(registry.find(static_cast<TypeId>(5)).has_value());
}

TEST(TypeRegistryTest, SnapshotSortedById)
{
    TypeRegistry registry{};
    bool dup{false};
    registry.install_builtin(make_builtin_schema(static_cast<TypeId>(2), "Int16"));
    registry.install_builtin(make_builtin_schema(static_cast<TypeId>(1), "Int8"));
    (void) registry.register_type(make_plugin_schema("P"), dup);

    const auto snapshot = registry.snapshot();
    ASSERT_GE(snapshot.size(), 3u);
    for (std::size_t i = 1; i < snapshot.size(); ++i)
    {
        EXPECT_LT(static_cast<std::uint32_t>(snapshot[i - 1].id),
                  static_cast<std::uint32_t>(snapshot[i].id));
    }
}

TEST(TypeRegistryTest, FindByNameReturnsExactId)
{
    TypeRegistry registry{};
    bool dup{false};
    const auto id = registry.register_type(make_plugin_schema("Lookup"), dup);
    ASSERT_NE(id, TypeId::Invalid);

    const auto found = registry.find_by_name("Lookup");
    ASSERT_TRUE(found.has_value());
    EXPECT_EQ(*found, id);
}
