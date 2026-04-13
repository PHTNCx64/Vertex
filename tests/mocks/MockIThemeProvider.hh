#pragma once

#include <gmock/gmock.h>
#include <vertex/gui/theme/ithemeprovider.hh>

namespace Vertex::Testing::Mocks
{
    class MockIThemeProvider : public Gui::IThemeProvider
    {
    public:
        ~MockIThemeProvider() override = default;

        MOCK_METHOD(const Gui::ColorPalette&, palette, (), (const, noexcept, override));
        MOCK_METHOD(bool, is_dark, (), (const, override));
        MOCK_METHOD(void, set_theme, (Theme theme), (override));
        MOCK_METHOD(Theme, current_theme, (), (const, noexcept, override));
        MOCK_METHOD(void, refresh, (), (override));
    };
} // namespace Vertex::Testing::Mocks
