#pragma once

#include <gmock/gmock.h>
#include <string_view>
#include <vertex/runtime/iuiregistry.hh>

namespace Vertex::Testing::Mocks
{
    class MockIUIRegistry : public Runtime::IUIRegistry
    {
    public:
        ~MockIUIRegistry() override = default;

        MOCK_METHOD(StatusCode, register_panel, (const UIPanel& panel), (override));
        MOCK_METHOD(std::vector<Runtime::PanelSnapshot>, get_panels, (), (const, override));
        MOCK_METHOD(std::optional<Runtime::PanelSnapshot>, get_panel, (std::string_view panelId), (const, override));

        MOCK_METHOD(StatusCode, set_value, (std::string_view panelId, std::string_view fieldId, const UIValue& value), (override));
        MOCK_METHOD(std::optional<UIValue>, get_value, (std::string_view panelId, std::string_view fieldId), (const, override));

        MOCK_METHOD(void, clear, (), (override));
        MOCK_METHOD(bool, has_panels, (), (const, override));
    };
}
