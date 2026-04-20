//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/runtime/pluginruntimeservice.hh>

#include "../../mocks/MockILog.hh"
#include "../../mocks/MockILoader.hh"

#include <atomic>
#include <chrono>
#include <filesystem>
#include <memory>
#include <mutex>
#include <vector>

using ::testing::_;
using ::testing::NiceMock;
using ::testing::Return;
using ::testing::ReturnRef;

namespace pl = Vertex::Runtime::PluginRuntime;

class PluginRuntimeServiceTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        m_loader = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILoader>>();
        m_log = std::make_unique<NiceMock<Vertex::Testing::Mocks::MockILog>>();

        ON_CALL(*m_loader, get_plugins()).WillByDefault(ReturnRef(m_plugins));

        m_service = std::make_unique<pl::PluginRuntimeService>(*m_loader, *m_log);
    }

    void add_plugin(const std::filesystem::path& path)
    {
        Vertex::Runtime::Plugin p{*m_log};
        p.set_path(path);
        m_plugins.emplace_back(std::move(p));
    }

    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILoader>> m_loader;
    std::unique_ptr<NiceMock<Vertex::Testing::Mocks::MockILog>> m_log;
    std::vector<Vertex::Runtime::Plugin> m_plugins{};
    std::unique_ptr<pl::PluginRuntimeService> m_service;
};

TEST_F(PluginRuntimeServiceTest, CmdLoadFiresStateChangedAndCompletesOk)
{
    const std::filesystem::path path{"/tmp/test-plugin.so"};
    add_plugin(path);

    ON_CALL(*m_loader, load_plugin(_)).WillByDefault(Return(STATUS_OK));

    std::atomic<bool> sawChange{false};
    std::size_t stateIndex{static_cast<std::size_t>(-1)};
    bool stateLoaded{false};
    const auto sub = m_service->subscribe(
        static_cast<pl::PluginEventKindMask>(pl::PluginEventKind::PluginStateChanged),
        [&](const pl::PluginEvent& evt)
        {
            const auto* info = std::get_if<pl::PluginStateInfo>(&evt.detail);
            if (!info) return;
            stateIndex = info->index;
            stateLoaded = info->loaded;
            sawChange = true;
        });

    const auto id = m_service->send_command(pl::engine::CmdLoad{.path = path}, std::chrono::seconds{1});
    const auto result = m_service->await_result(id, std::chrono::seconds{1});
    EXPECT_EQ(result.code, STATUS_OK);
    EXPECT_EQ(result.index, 0u);
    EXPECT_TRUE(sawChange.load());
    EXPECT_EQ(stateIndex, 0u);
    EXPECT_TRUE(stateLoaded);
    m_service->unsubscribe(sub);
}

TEST_F(PluginRuntimeServiceTest, CmdLoadFailureFiresLoadErrorAndReturnsCode)
{
    const std::filesystem::path path{"/tmp/broken.so"};
    ON_CALL(*m_loader, load_plugin(_))
        .WillByDefault(Return(StatusCode::STATUS_ERROR_PLUGIN_LOAD_FAILED));

    std::atomic<bool> sawErr{false};
    const auto sub = m_service->subscribe(
        static_cast<pl::PluginEventKindMask>(pl::PluginEventKind::PluginLoadError),
        [&](const pl::PluginEvent&) { sawErr = true; });

    const auto id = m_service->send_command(pl::engine::CmdLoad{.path = path}, std::chrono::seconds{1});
    const auto result = m_service->await_result(id, std::chrono::seconds{1});
    EXPECT_EQ(result.code, StatusCode::STATUS_ERROR_PLUGIN_LOAD_FAILED);
    EXPECT_TRUE(sawErr.load());
    m_service->unsubscribe(sub);
}

TEST_F(PluginRuntimeServiceTest, CmdActivateFiresActiveChanged)
{
    add_plugin("/tmp/a.so");
    ON_CALL(*m_loader, set_active_plugin(::testing::Matcher<std::size_t>(_)))
        .WillByDefault(Return(STATUS_OK));

    std::atomic<bool> sawActive{false};
    std::size_t activeIndex{static_cast<std::size_t>(-1)};
    const auto sub = m_service->subscribe(
        static_cast<pl::PluginEventKindMask>(pl::PluginEventKind::ActivePluginChanged),
        [&](const pl::PluginEvent& evt)
        {
            const auto* info = std::get_if<pl::ActivePluginChangedInfo>(&evt.detail);
            if (!info) return;
            activeIndex = info->index;
            sawActive = true;
        });

    const auto id = m_service->send_command(pl::engine::CmdActivate{.index = 0}, std::chrono::seconds{1});
    const auto result = m_service->await_result(id, std::chrono::seconds{1});
    EXPECT_EQ(result.code, STATUS_OK);
    EXPECT_TRUE(sawActive.load());
    EXPECT_EQ(activeIndex, 0u);

    m_service->unsubscribe(sub);
}

TEST_F(PluginRuntimeServiceTest, CmdDeactivateClearsActiveSlot)
{
    add_plugin("/tmp/a.so");
    ON_CALL(*m_loader, set_active_plugin(::testing::Matcher<std::size_t>(_)))
        .WillByDefault(Return(STATUS_OK));

    (void) m_service->await_result(
        m_service->send_command(pl::engine::CmdActivate{.index = 0}, std::chrono::seconds{1}), std::chrono::seconds{1});

    EXPECT_TRUE(m_service->snapshot_active().has_value());

    (void) m_service->await_result(
        m_service->send_command(pl::engine::CmdDeactivate{}, std::chrono::seconds{1}), std::chrono::seconds{1});

    EXPECT_FALSE(m_service->snapshot_active().has_value());
}

TEST_F(PluginRuntimeServiceTest, SnapshotPluginsReflectsLoaderAndActiveState)
{
    add_plugin("/tmp/a.so");
    add_plugin("/tmp/b.so");
    ON_CALL(*m_loader, set_active_plugin(::testing::Matcher<std::size_t>(_)))
        .WillByDefault(Return(STATUS_OK));

    (void) m_service->await_result(
        m_service->send_command(pl::engine::CmdActivate{.index = 1}, std::chrono::seconds{1}), std::chrono::seconds{1});

    const auto snap = m_service->snapshot_plugins();
    ASSERT_EQ(snap.size(), 2u);
    EXPECT_EQ(snap[0].index, 0u);
    EXPECT_FALSE(snap[0].active);
    EXPECT_EQ(snap[1].index, 1u);
    EXPECT_TRUE(snap[1].active);
}

TEST_F(PluginRuntimeServiceTest, CmdUnloadOutOfBoundsReturnsError)
{
    const auto id = m_service->send_command(pl::engine::CmdUnload{.index = 42}, std::chrono::seconds{1});
    const auto result = m_service->await_result(id, std::chrono::seconds{1});
    EXPECT_EQ(result.code, StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS);
}

TEST_F(PluginRuntimeServiceTest, ShutdownCompletesPendingWithShutdownStatus)
{
    m_service->shutdown();
    const auto id = m_service->send_command(pl::engine::CmdDeactivate{}, std::chrono::seconds{1});
    EXPECT_EQ(id, Vertex::Runtime::INVALID_COMMAND_ID);
}
