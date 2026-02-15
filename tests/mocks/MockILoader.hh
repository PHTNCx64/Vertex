//
// Mock for ILoader interface
//

#pragma once

#include <gmock/gmock.h>
#include <vertex/runtime/iloader.hh>
#include <sdk/event.h>
#include "MockIRegistry.hh"

namespace Vertex::Testing::Mocks
{
    class MockILoader : public Runtime::ILoader
    {
    public:
        ~MockILoader() override = default;

        MOCK_METHOD(StatusCode, load_plugins, (std::filesystem::path& path), (override));
        MOCK_METHOD(StatusCode, load_plugin, (std::filesystem::path path), (override));
        MOCK_METHOD(StatusCode, unload_plugin, (std::size_t pluginIndex), (override));
        MOCK_METHOD(StatusCode, resolve_functions, (Runtime::Plugin& plugin), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (Runtime::Plugin& plugin), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (std::size_t index), (override));
        MOCK_METHOD(StatusCode, set_active_plugin, (const std::filesystem::path& path), (override));
        MOCK_METHOD(StatusCode, has_plugin_loaded, (), (override));
        MOCK_METHOD(StatusCode, get_plugins_from_fs, (const std::vector<std::filesystem::path>& paths, std::vector<Runtime::Plugin>& pluginStates), (override));
        MOCK_METHOD(const std::vector<Runtime::Plugin>&, get_plugins, (), (noexcept, override));
        MOCK_METHOD(std::optional<std::reference_wrapper<Runtime::Plugin>>, get_active_plugin, (), (override));

        // Registry access - returns reference to internal mock registry
        Runtime::IRegistry& get_registry() override { return m_mockRegistry; }
        const Runtime::IRegistry& get_registry() const override { return m_mockRegistry; }

        // Event dispatching
        MOCK_METHOD(StatusCode, dispatch_event, (VertexEvent event, const void* data), (override));

        // Expose mock registry for test setup
        MockIRegistry& mock_registry() { return m_mockRegistry; }

    private:
        MockIRegistry m_mockRegistry;
    };
} // namespace Vertex::Testing::Mocks
