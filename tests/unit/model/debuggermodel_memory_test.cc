//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>
#include <gmock/gmock.h>

#include <vertex/model/debuggermodel.hh>
#include <vertex/runtime/plugin.hh>

#include "../../mocks/MockISettings.hh"
#include "../../mocks/MockILoader.hh"
#include "../../mocks/MockILog.hh"
#include "../../mocks/MockIThreadDispatcher.hh"

#include <wx/app.h>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <expected>
#include <future>
#include <limits>
#include <optional>
#include <span>
#include <vector>

using namespace Vertex::Debugger;
using namespace Vertex::Model;
using namespace Vertex::Testing::Mocks;
using namespace Vertex::Thread;
using namespace testing;

namespace
{
    struct MemoryStubContext final
    {
        std::size_t readCalls{};
        std::vector<std::uint64_t> readAddresses{};
        std::vector<std::uint64_t> readSizes{};
        std::vector<std::uint8_t> readCallBiases{};
        std::vector<std::uint64_t> failingReadAddresses{};
        std::uint64_t lastWriteAddress{};
        std::vector<std::uint8_t> lastWritePayload{};
        std::size_t writeCalls{};
        std::size_t bulkReadCalls{};
        std::vector<std::uint32_t> bulkBatchSizes{};
        std::vector<std::uint64_t> bulkReadAddresses{};
        std::vector<std::uint64_t> bulkReadSizes{};
        std::vector<std::uint64_t> failingBulkReadAddresses{};
        std::uint32_t bulkRequestLimit{};
        StatusCode bulkReadCallStatus{StatusCode::STATUS_OK};
        std::size_t queryRegionsCalls{};
        std::vector<MemoryRegion> queryRegions{};
        StatusCode queryRegionsStatus{StatusCode::STATUS_OK};
        bool failReads{};
        StatusCode writeStatus{StatusCode::STATUS_OK};
    };

    MemoryStubContext* g_memoryStubContext{};

    StatusCode VERTEX_API stub_memory_read_pattern(
        const std::uint64_t address,
        const std::uint64_t size,
        char* buffer)
    {
        if (g_memoryStubContext == nullptr || buffer == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        g_memoryStubContext->readAddresses.push_back(address);
        g_memoryStubContext->readSizes.push_back(size);
        const auto readCallIndex = g_memoryStubContext->readCalls++;

        std::uint8_t callBias{};
        if (readCallIndex < g_memoryStubContext->readCallBiases.size())
        {
            callBias = g_memoryStubContext->readCallBiases[readCallIndex];
        }

        if (g_memoryStubContext->failReads)
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }
        if (std::find(
                g_memoryStubContext->failingReadAddresses.begin(),
                g_memoryStubContext->failingReadAddresses.end(),
                address) != g_memoryStubContext->failingReadAddresses.end())
        {
            return StatusCode::STATUS_ERROR_MEMORY_READ;
        }

        for (std::uint64_t i{}; i < size; ++i)
        {
            buffer[i] = static_cast<char>((address + i + callBias) & 0xFFu);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode VERTEX_API stub_memory_write_capture(
        const std::uint64_t address,
        const std::uint64_t size,
        const char* buffer)
    {
        if (g_memoryStubContext == nullptr || buffer == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        g_memoryStubContext->writeCalls++;
        g_memoryStubContext->lastWriteAddress = address;
        g_memoryStubContext->lastWritePayload.assign(
            reinterpret_cast<const std::uint8_t*>(buffer),
            reinterpret_cast<const std::uint8_t*>(buffer) + static_cast<std::ptrdiff_t>(size));

        return g_memoryStubContext->writeStatus;
    }

    StatusCode VERTEX_API stub_memory_get_bulk_request_limit(std::uint32_t* maxRequestCount)
    {
        if (g_memoryStubContext == nullptr || maxRequestCount == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        *maxRequestCount = g_memoryStubContext->bulkRequestLimit;
        return StatusCode::STATUS_OK;
    }

    StatusCode VERTEX_API stub_memory_read_bulk(
        const BulkReadRequest* requests,
        BulkReadResult* results,
        const std::uint32_t count)
    {
        if (g_memoryStubContext == nullptr || requests == nullptr || results == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        g_memoryStubContext->bulkReadCalls++;
        g_memoryStubContext->bulkBatchSizes.push_back(count);

        for (std::uint32_t i{}; i < count; ++i)
        {
            const auto& request = requests[i];
            g_memoryStubContext->bulkReadAddresses.push_back(request.address);
            g_memoryStubContext->bulkReadSizes.push_back(request.size);

            if (request.buffer == nullptr && request.size != 0)
            {
                results[i].status = StatusCode::STATUS_ERROR_INVALID_PARAMETER;
                continue;
            }

            if (std::find(
                    g_memoryStubContext->failingBulkReadAddresses.begin(),
                    g_memoryStubContext->failingBulkReadAddresses.end(),
                    request.address) != g_memoryStubContext->failingBulkReadAddresses.end())
            {
                results[i].status = StatusCode::STATUS_ERROR_MEMORY_READ;
                continue;
            }

            auto* buffer = static_cast<char*>(request.buffer);
            for (std::uint64_t j{}; j < request.size; ++j)
            {
                buffer[j] = static_cast<char>((request.address + j) & 0xFFu);
            }
            results[i].status = StatusCode::STATUS_OK;
        }

        return g_memoryStubContext->bulkReadCallStatus;
    }

    StatusCode VERTEX_API stub_memory_query_regions(
        MemoryRegion** regions,
        std::uint64_t* count)
    {
        if (g_memoryStubContext == nullptr || count == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        g_memoryStubContext->queryRegionsCalls++;

        if (g_memoryStubContext->queryRegionsStatus != StatusCode::STATUS_OK)
        {
            return g_memoryStubContext->queryRegionsStatus;
        }

        *count = static_cast<std::uint64_t>(g_memoryStubContext->queryRegions.size());
        if (regions == nullptr)
        {
            return StatusCode::STATUS_OK;
        }

        if (g_memoryStubContext->queryRegions.empty())
        {
            *regions = nullptr;
        }
        else
        {
            const auto regionCount = g_memoryStubContext->queryRegions.size();
            auto* allocated = static_cast<MemoryRegion*>(
                std::malloc(sizeof(MemoryRegion) * regionCount));
            if (allocated == nullptr)
            {
                return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
            }
            std::copy_n(g_memoryStubContext->queryRegions.data(), regionCount, allocated);
            *regions = allocated;
        }

        return StatusCode::STATUS_OK;
    }
}

class DebuggerModelMemoryTest : public ::testing::Test
{
protected:
    static void SetUpTestSuite()
    {
        if (!wxTheApp)
        {
            wxInitialize();
        }
    }

    static void TearDownTestSuite()
    {
        if (wxTheApp)
        {
            wxUninitialize();
        }
    }

    void SetUp() override
    {
        m_settings = std::make_unique<NiceMock<MockISettings>>();
        m_loader = std::make_unique<NiceMock<MockILoader>>();
        m_logger = std::make_unique<NiceMock<MockILog>>();
        m_dispatcher = std::make_unique<NiceMock<MockIThreadDispatcher>>();

        ON_CALL(*m_dispatcher, dispatch_with_priority(_, _, _))
            .WillByDefault(Invoke(
                []([[maybe_unused]] ThreadChannel channel,
                   [[maybe_unused]] DispatchPriority priority,
                   std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));

        ON_CALL(*m_dispatcher, dispatch(_, _))
            .WillByDefault(Invoke(
                []([[maybe_unused]] ThreadChannel channel,
                   std::packaged_task<StatusCode()>&& task)
                    -> std::expected<std::future<StatusCode>, StatusCode>
                {
                    auto future = task.get_future();
                    task();
                    return future;
                }));

        ON_CALL(*m_dispatcher, schedule_recurring(_, _, _, _, _, _))
            .WillByDefault(Return(std::expected<RecurringTaskHandle, StatusCode>{RecurringTaskHandle{1}}));
        ON_CALL(*m_dispatcher, schedule_recurring_persistent(_, _, _, _, _, _))
            .WillByDefault(Return(std::expected<RecurringTaskHandle, StatusCode>{RecurringTaskHandle{2}}));
        ON_CALL(*m_dispatcher, cancel_recurring(_))
            .WillByDefault(Return(StatusCode::STATUS_OK));

        m_model = std::make_unique<DebuggerModel>(
            *m_settings, *m_loader, *m_logger, *m_dispatcher);

        m_model->set_event_handler(
            [this](const DirtyFlags flags, [[maybe_unused]] const EngineSnapshot& snapshot)
            {
                m_dirtyEvents.push_back(flags);
            });

        g_memoryStubContext = &m_stubContext;
    }

    void TearDown() override
    {
        g_memoryStubContext = nullptr;
        m_model.reset();
    }

    void flush_pending_wx_events() const
    {
        if (wxTheApp)
        {
            wxTheApp->ProcessPendingEvents();
        }
    }

    void setup_plugin(Vertex::Runtime::Plugin& plugin) const
    {
        ON_CALL(*m_loader, has_plugin_loaded())
            .WillByDefault(Return(StatusCode::STATUS_OK));
        ON_CALL(*m_loader, get_active_plugin())
            .WillByDefault(Return(std::optional<std::reference_wrapper<Vertex::Runtime::Plugin>>{std::ref(plugin)}));
    }

    [[nodiscard]] std::size_t memory_event_count() const
    {
        return std::count_if(m_dirtyEvents.begin(), m_dirtyEvents.end(), [](const DirtyFlags flags)
        {
            return (flags & DirtyFlags::Memory) != DirtyFlags::None;
        });
    }

    std::unique_ptr<NiceMock<MockISettings>> m_settings;
    std::unique_ptr<NiceMock<MockILoader>> m_loader;
    std::unique_ptr<NiceMock<MockILog>> m_logger;
    std::unique_ptr<NiceMock<MockIThreadDispatcher>> m_dispatcher;
    std::unique_ptr<DebuggerModel> m_model;

    MemoryStubContext m_stubContext{};
    std::vector<DirtyFlags> m_dirtyEvents{};
};

TEST_F(DebuggerModelMemoryTest, RequestMemoryBuildsFullySizedReadableAndModifiedVectors)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    setup_plugin(plugin);

    m_model->request_memory(0x401000, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401000u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(block.readable.size(), 64u);
    ASSERT_EQ(block.modified.size(), 64u);

    ASSERT_EQ(m_stubContext.readAddresses.size(), 1u);
    ASSERT_EQ(m_stubContext.readSizes.size(), 1u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x401000u);
    EXPECT_EQ(m_stubContext.readSizes[0], 64u);

    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.modified, [](const bool value)
    {
        return !value;
    }));

    ASSERT_GE(block.data.size(), 2u);
    EXPECT_EQ(block.data[0], 0x00u);
    EXPECT_EQ(block.data[1], 0x01u);
    EXPECT_EQ(memory_event_count(), 1u);
}

TEST_F(DebuggerModelMemoryTest, RequestMemorySplitsReadsAtPageBoundary)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    setup_plugin(plugin);

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(block.readable.size(), 64u);
    ASSERT_EQ(block.modified.size(), 64u);

    ASSERT_EQ(m_stubContext.readAddresses.size(), 2u);
    ASSERT_EQ(m_stubContext.readSizes.size(), 2u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x401FF0u);
    EXPECT_EQ(m_stubContext.readSizes[0], 16u);
    EXPECT_EQ(m_stubContext.readAddresses[1], 0x402000u);
    EXPECT_EQ(m_stubContext.readSizes[1], 48u);

    EXPECT_EQ(block.data[0], 0xF0u);
    EXPECT_EQ(block.data[15], 0xFFu);
    EXPECT_EQ(block.data[16], 0x00u);
    EXPECT_EQ(block.data[63], 0x2Fu);
    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.modified, [](const bool value)
    {
        return !value;
    }));
    EXPECT_EQ(memory_event_count(), 1u);
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryFailedReadKeepsUnreadableFlagsAndZeroData)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    setup_plugin(plugin);

    m_stubContext.failReads = true;

    m_model->request_memory(0x9000, 32);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x9000u);
    ASSERT_EQ(block.data.size(), 32u);
    ASSERT_EQ(block.readable.size(), 32u);
    ASSERT_EQ(block.modified.size(), 32u);

    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return !value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.modified, [](const bool value)
    {
        return !value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.data, [](const std::uint8_t byte)
    {
        return byte == 0;
    }));
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryPartialPageFailureMarksOnlyFailedSpanUnreadable)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    setup_plugin(plugin);

    m_stubContext.failingReadAddresses.push_back(0x402000);

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(block.readable.size(), 64u);

    ASSERT_EQ(m_stubContext.readAddresses.size(), 2u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x401FF0u);
    EXPECT_EQ(m_stubContext.readSizes[0], 16u);
    EXPECT_EQ(m_stubContext.readAddresses[1], 0x402000u);
    EXPECT_EQ(m_stubContext.readSizes[1], 48u);

    for (std::size_t i{}; i < 16; ++i)
    {
        EXPECT_TRUE(block.readable[i]);
        EXPECT_EQ(block.data[i], static_cast<std::uint8_t>(0xF0u + i));
    }

    for (std::size_t i{16}; i < block.data.size(); ++i)
    {
        EXPECT_FALSE(block.readable[i]);
        EXPECT_EQ(block.data[i], 0u);
    }
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryRegionMapSkipsUnmappedPages)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = nullptr,
        .baseAddress = 0x401FF0u,
        .regionSize = 16u});

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(block.readable.size(), 64u);
    ASSERT_EQ(m_stubContext.queryRegionsCalls, 1u);
    ASSERT_EQ(m_stubContext.readAddresses.size(), 1u);
    ASSERT_EQ(m_stubContext.readSizes.size(), 1u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x401FF0u);
    EXPECT_EQ(m_stubContext.readSizes[0], 16u);

    for (std::size_t i{}; i < 16; ++i)
    {
        EXPECT_TRUE(block.readable[i]);
        EXPECT_EQ(block.data[i], static_cast<std::uint8_t>(0xF0u + i));
    }

    for (std::size_t i{16}; i < block.data.size(); ++i)
    {
        EXPECT_FALSE(block.readable[i]);
        EXPECT_EQ(block.data[i], 0u);
    }
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryPublishesClippedRegionMetadata)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = "moduleA",
        .baseAddress = 0x401FE0u,
        .regionSize = 0x20u});
    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = "moduleB",
        .baseAddress = 0x402000u,
        .regionSize = 0x40u});

    m_model->request_memory(0x401FF0, 0x30);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 0x30u);
    ASSERT_EQ(block.regions.size(), 2u);

    EXPECT_EQ(block.regions[0].moduleName, "moduleA");
    EXPECT_EQ(block.regions[0].startAddress, 0x401FF0u);
    EXPECT_EQ(block.regions[0].endAddress, 0x402000u);

    EXPECT_EQ(block.regions[1].moduleName, "moduleB");
    EXPECT_EQ(block.regions[1].startAddress, 0x402000u);
    EXPECT_EQ(block.regions[1].endAddress, 0x402020u);
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryMergesAdjacentRegionSlicesWithSameModule)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = "moduleMerged",
        .baseAddress = 0x5000u,
        .regionSize = 0x10u});
    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = "moduleMerged",
        .baseAddress = 0x5010u,
        .regionSize = 0x10u});

    m_model->request_memory(0x5000, 0x20);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x5000u);
    ASSERT_EQ(block.data.size(), 0x20u);
    ASSERT_EQ(block.regions.size(), 1u);
    EXPECT_EQ(block.regions[0].moduleName, "moduleMerged");
    EXPECT_EQ(block.regions[0].startAddress, 0x5000u);
    EXPECT_EQ(block.regions[0].endAddress, 0x5020u);
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryRegionMapWithoutOverlapSkipsAllReads)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = nullptr,
        .baseAddress = 0x500000u,
        .regionSize = 4096u});

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(m_stubContext.queryRegionsCalls, 1u);
    EXPECT_EQ(m_stubContext.readCalls, 0u);
    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return !value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.data, [](const std::uint8_t byte)
    {
        return byte == 0u;
    }));
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryRegionQueryFailureFallsBackToPageReads)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.queryRegionsStatus = StatusCode::STATUS_ERROR_GENERAL;

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(m_stubContext.queryRegionsCalls, 1u);
    ASSERT_EQ(m_stubContext.readAddresses.size(), 2u);
    ASSERT_EQ(m_stubContext.readSizes.size(), 2u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x401FF0u);
    EXPECT_EQ(m_stubContext.readSizes[0], 16u);
    EXPECT_EQ(m_stubContext.readAddresses[1], 0x402000u);
    EXPECT_EQ(m_stubContext.readSizes[1], 48u);
    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return value;
    }));
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryBulkReadRespectsRegionMapOverlaps)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_read_process_bulk = stub_memory_read_bulk;
    plugin.internal_vertex_memory_get_bulk_request_limit = stub_memory_get_bulk_request_limit;
    plugin.internal_vertex_memory_query_regions = stub_memory_query_regions;
    setup_plugin(plugin);

    m_stubContext.bulkRequestLimit = 16;
    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = nullptr,
        .baseAddress = 0x401FF4u,
        .regionSize = 4u});
    m_stubContext.queryRegions.push_back(MemoryRegion{
        .baseModuleName = nullptr,
        .baseAddress = 0x402000u,
        .regionSize = 8u});

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(m_stubContext.queryRegionsCalls, 1u);
    EXPECT_EQ(m_stubContext.readCalls, 0u);
    EXPECT_EQ(m_stubContext.bulkReadCalls, 1u);
    ASSERT_EQ(m_stubContext.bulkReadAddresses.size(), 2u);
    ASSERT_EQ(m_stubContext.bulkReadSizes.size(), 2u);
    EXPECT_EQ(m_stubContext.bulkReadAddresses[0], 0x401FF4u);
    EXPECT_EQ(m_stubContext.bulkReadSizes[0], 4u);
    EXPECT_EQ(m_stubContext.bulkReadAddresses[1], 0x402000u);
    EXPECT_EQ(m_stubContext.bulkReadSizes[1], 8u);

    for (std::size_t i{}; i < block.readable.size(); ++i)
    {
        const bool expectedReadable = (i >= 4 && i < 8) || (i >= 16 && i < 24);
        EXPECT_EQ(block.readable[i], expectedReadable);
    }
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryUsesBulkReadWhenAvailable)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_read_process_bulk = stub_memory_read_bulk;
    plugin.internal_vertex_memory_get_bulk_request_limit = stub_memory_get_bulk_request_limit;
    setup_plugin(plugin);

    m_stubContext.bulkRequestLimit = 16;

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(block.readable.size(), 64u);
    ASSERT_EQ(block.modified.size(), 64u);

    EXPECT_EQ(m_stubContext.readCalls, 0u);
    EXPECT_EQ(m_stubContext.bulkReadCalls, 1u);
    ASSERT_EQ(m_stubContext.bulkReadAddresses.size(), 2u);
    ASSERT_EQ(m_stubContext.bulkReadSizes.size(), 2u);
    EXPECT_EQ(m_stubContext.bulkReadAddresses[0], 0x401FF0u);
    EXPECT_EQ(m_stubContext.bulkReadSizes[0], 16u);
    EXPECT_EQ(m_stubContext.bulkReadAddresses[1], 0x402000u);
    EXPECT_EQ(m_stubContext.bulkReadSizes[1], 48u);
    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.modified, [](const bool value)
    {
        return !value;
    }));
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryBulkReadFailureMarksOnlyFailedEntryUnreadable)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_read_process_bulk = stub_memory_read_bulk;
    plugin.internal_vertex_memory_get_bulk_request_limit = stub_memory_get_bulk_request_limit;
    setup_plugin(plugin);

    m_stubContext.bulkRequestLimit = 16;
    m_stubContext.failingBulkReadAddresses.push_back(0x402000u);

    m_model->request_memory(0x401FF0, 64);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x401FF0u);
    ASSERT_EQ(block.data.size(), 64u);
    ASSERT_EQ(m_stubContext.readCalls, 0u);
    ASSERT_EQ(m_stubContext.bulkReadCalls, 1u);

    for (std::size_t i{}; i < 16; ++i)
    {
        EXPECT_TRUE(block.readable[i]);
        EXPECT_EQ(block.data[i], static_cast<std::uint8_t>(0xF0u + i));
    }

    for (std::size_t i{16}; i < block.data.size(); ++i)
    {
        EXPECT_FALSE(block.readable[i]);
        EXPECT_EQ(block.data[i], 0u);
    }
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryBulkReadRespectsPluginBatchLimit)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_read_process_bulk = stub_memory_read_bulk;
    plugin.internal_vertex_memory_get_bulk_request_limit = stub_memory_get_bulk_request_limit;
    setup_plugin(plugin);

    m_stubContext.bulkRequestLimit = 2;

    m_model->request_memory(0x500000, 5 * 4096);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x500000u);
    ASSERT_EQ(block.data.size(), static_cast<std::size_t>(5 * 4096));
    ASSERT_EQ(m_stubContext.readCalls, 0u);
    ASSERT_EQ(m_stubContext.bulkReadCalls, 3u);

    const std::vector<std::uint32_t> expectedBatchSizes{2u, 2u, 1u};
    EXPECT_EQ(m_stubContext.bulkBatchSizes, expectedBatchSizes);
}

TEST_F(DebuggerModelMemoryTest, RequestMemoryAddressOverflowSkipsPluginRead)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    setup_plugin(plugin);

    const auto address = std::numeric_limits<std::uint64_t>::max() - 8u;
    m_model->request_memory(address, 32);
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, address);
    ASSERT_EQ(block.data.size(), 32u);
    ASSERT_EQ(block.readable.size(), 32u);
    ASSERT_EQ(block.modified.size(), 32u);

    EXPECT_TRUE(m_stubContext.readAddresses.empty());
    EXPECT_TRUE(std::ranges::all_of(block.readable, [](const bool value)
    {
        return !value;
    }));
    EXPECT_TRUE(std::ranges::all_of(block.modified, [](const bool value)
    {
        return !value;
    }));
}

TEST_F(DebuggerModelMemoryTest, WriteMemoryDispatchesOnDebuggerChannelAndInvokesPluginWrite)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_write_process = stub_memory_write_capture;
    setup_plugin(plugin);

    EXPECT_CALL(*m_dispatcher, dispatch(ThreadChannel::Debugger, _))
        .Times(1)
        .WillOnce(Invoke(
            []([[maybe_unused]] ThreadChannel channel,
               std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                auto future = task.get_future();
                task();
                return future;
            }));

    const std::array<std::uint8_t, 3> payload{0xDE, 0xAD, 0xBE};
    const auto status = m_model->write_memory(0x5000, std::span<const std::uint8_t>{payload});

    EXPECT_EQ(status, StatusCode::STATUS_OK);
    EXPECT_EQ(m_stubContext.writeCalls, 1u);
    EXPECT_EQ(m_stubContext.lastWriteAddress, 0x5000u);
    EXPECT_EQ(m_stubContext.lastWritePayload, std::vector<std::uint8_t>(payload.begin(), payload.end()));
    EXPECT_EQ(memory_event_count(), 1u);
}

TEST_F(DebuggerModelMemoryTest, WriteMemoryPatchesCachedBlockAndMarksModifiedBytes)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_write_process = stub_memory_write_capture;
    setup_plugin(plugin);

    m_model->request_memory(0x7000, 16);
    flush_pending_wx_events();
    m_dirtyEvents.clear();

    const std::array<std::uint8_t, 4> patch{0xAA, 0xBB, 0xCC, 0xDD};
    const auto status = m_model->write_memory(0x7004, std::span<const std::uint8_t>{patch});

    EXPECT_EQ(status, StatusCode::STATUS_OK);

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.data.size(), 16u);
    ASSERT_EQ(block.readable.size(), 16u);
    ASSERT_EQ(block.modified.size(), 16u);

    EXPECT_EQ(block.data[4], 0xAAu);
    EXPECT_EQ(block.data[5], 0xBBu);
    EXPECT_EQ(block.data[6], 0xCCu);
    EXPECT_EQ(block.data[7], 0xDDu);

    EXPECT_FALSE(block.modified[3]);
    EXPECT_TRUE(block.modified[4]);
    EXPECT_TRUE(block.modified[5]);
    EXPECT_TRUE(block.modified[6]);
    EXPECT_TRUE(block.modified[7]);
    EXPECT_FALSE(block.modified[8]);

    EXPECT_TRUE(block.readable[4]);
    EXPECT_TRUE(block.readable[5]);
    EXPECT_TRUE(block.readable[6]);
    EXPECT_TRUE(block.readable[7]);
    EXPECT_EQ(memory_event_count(), 1u);
}

TEST_F(DebuggerModelMemoryTest, WriteMemoryWithoutPluginLoadedReturnsPluginNotActive)
{
    ON_CALL(*m_loader, has_plugin_loaded())
        .WillByDefault(Return(StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE));
    EXPECT_CALL(*m_dispatcher, dispatch(_, _)).Times(0);

    const std::array<std::uint8_t, 1> payload{0x11};
    const auto status = m_model->write_memory(0x1234, std::span<const std::uint8_t>{payload});

    EXPECT_EQ(status, StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE);
    EXPECT_EQ(m_stubContext.writeCalls, 0u);
    EXPECT_EQ(memory_event_count(), 0u);
}

TEST_F(DebuggerModelMemoryTest, StaleReadAfterWriteRedispatchesAndKeepsLatestRead)
{
    Vertex::Runtime::Plugin plugin{*m_logger};
    plugin.internal_vertex_memory_read_process = stub_memory_read_pattern;
    plugin.internal_vertex_memory_write_process = stub_memory_write_capture;
    setup_plugin(plugin);

    m_stubContext.readCallBiases = {0x00u, 0x10u};

    std::optional<std::packaged_task<StatusCode()>> delayedReadTask{};

    ON_CALL(*m_dispatcher, dispatch_with_priority(_, _, _))
        .WillByDefault(Invoke(
            [&delayedReadTask](ThreadChannel channel,
                               DispatchPriority priority,
                               std::packaged_task<StatusCode()>&& task)
                -> std::expected<std::future<StatusCode>, StatusCode>
            {
                EXPECT_EQ(channel, ThreadChannel::Debugger);
                EXPECT_EQ(priority, DispatchPriority::Low);

                auto future = task.get_future();
                if (!delayedReadTask.has_value())
                {
                    delayedReadTask.emplace(std::move(task));
                    return future;
                }

                task();
                return future;
            }));

    m_model->request_memory(0x6000, 16);
    ASSERT_TRUE(delayedReadTask.has_value());

    const std::array<std::uint8_t, 1> writePayload{0xAA};
    const auto writeStatus = m_model->write_memory(0x6000, std::span<const std::uint8_t>{writePayload});
    EXPECT_EQ(writeStatus, StatusCode::STATUS_OK);

    (*delayedReadTask)();

    flush_pending_wx_events();
    flush_pending_wx_events();

    const auto& block = m_model->get_cached_memory();
    ASSERT_EQ(block.baseAddress, 0x6000u);
    ASSERT_EQ(block.data.size(), 16u);
    ASSERT_EQ(m_stubContext.readAddresses.size(), 2u);
    EXPECT_EQ(m_stubContext.readAddresses[0], 0x6000u);
    EXPECT_EQ(m_stubContext.readAddresses[1], 0x6000u);
    EXPECT_EQ(block.data[0], 0x10u);
}
