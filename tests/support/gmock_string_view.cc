#include <gmock/gmock.h>
#include <string_view>

template class testing::Matcher<std::string_view>;
template class testing::internal::MatcherBase<std::string_view>;
