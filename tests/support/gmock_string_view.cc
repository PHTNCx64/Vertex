//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gmock/gmock.h>
#include <string_view>

template class testing::Matcher<std::string_view>;
template class testing::internal::MatcherBase<std::string_view>;
