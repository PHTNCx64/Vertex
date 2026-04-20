//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include <vertex/viewmodel/mainviewmodel.hh>

namespace
{
    using Vertex::Scanner::TypeId;
    using Vertex::Scanner::ValueType;
    using Vertex::Scanner::builtin_type_id;
    using Vertex::ViewModel::SavedAddress;
}

TEST(SavedAddressCompatTest, MissingTypeIdFallsBackToBuiltinFromValueTypeIndex)
{
    SavedAddress saved{};
    saved.valueTypeIndex = static_cast<int>(ValueType::Int32);
    EXPECT_EQ(saved.typeId, TypeId::Invalid);

    const auto effective = saved.effective_type_id();
    EXPECT_EQ(effective, builtin_type_id(ValueType::Int32));
    EXPECT_NE(effective, TypeId::Invalid);
}

TEST(SavedAddressCompatTest, ExplicitTypeIdIsPreserved)
{
    SavedAddress saved{};
    saved.valueTypeIndex = static_cast<int>(ValueType::Int8);
    saved.typeId = static_cast<TypeId>(1234);

    EXPECT_EQ(saved.effective_type_id(), static_cast<TypeId>(1234));
}

TEST(SavedAddressCompatTest, DefaultConstructedMapsToFirstBuiltin)
{
    SavedAddress saved{};
    
    EXPECT_EQ(saved.effective_type_id(), builtin_type_id(ValueType::Int8));
}

TEST(SavedAddressCompatTest, StringTypeIndexMapsToStringBuiltin)
{
    SavedAddress saved{};
    saved.valueTypeIndex = static_cast<int>(ValueType::StringUTF8);
    EXPECT_EQ(saved.effective_type_id(), builtin_type_id(ValueType::StringUTF8));
}
