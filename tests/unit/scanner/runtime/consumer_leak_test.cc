//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <gtest/gtest.h>

#include "fake_service.hh"

#include <vertex/runtime/subscription_guard.hh>
#include <vertex/scanner/scanner_event.hh>

#include <atomic>
#include <memory>
#include <utility>

namespace
{
    using Vertex::Scanner::IScannerRuntimeService;
    using Vertex::Scanner::ScannerEvent;
    using Vertex::Scanner::ScannerEventKind;
    using Vertex::Scanner::ScannerEventKindMask;
    using Vertex::Testing::Fakes::FakeScannerRuntimeService;

    struct Consumer final
    {
        explicit Consumer(IScannerRuntimeService& svc)
            : m_alive{std::make_shared<std::atomic<bool>>(true)}
        {
            m_scanCompleteSub = Vertex::Runtime::SubscriptionGuard<IScannerRuntimeService>{
                svc,
                svc.subscribe(
                    static_cast<ScannerEventKindMask>(ScannerEventKind::ScanComplete),
                    [weak = std::weak_ptr<std::atomic<bool>>{m_alive}](const ScannerEvent&)
                    {
                        const auto alive = weak.lock();
                        if (!alive || !alive->load(std::memory_order_acquire)) return;
                    })};

            m_valuesChangedSub = Vertex::Runtime::SubscriptionGuard<IScannerRuntimeService>{
                svc,
                svc.subscribe(
                    static_cast<ScannerEventKindMask>(ScannerEventKind::ValuesChanged),
                    [weak = std::weak_ptr<std::atomic<bool>>{m_alive}](const ScannerEvent&)
                    {
                        const auto alive = weak.lock();
                        if (!alive || !alive->load(std::memory_order_acquire)) return;
                    })};
        }

        ~Consumer()
        {
            m_scanCompleteSub.reset();
            m_valuesChangedSub.reset();
            m_alive->store(false, std::memory_order_release);
        }

        std::shared_ptr<std::atomic<bool>> m_alive;
        Vertex::Runtime::SubscriptionGuard<IScannerRuntimeService> m_scanCompleteSub{};
        Vertex::Runtime::SubscriptionGuard<IScannerRuntimeService> m_valuesChangedSub{};
    };
}

TEST(ConsumerLeakTest, HundredCyclesReturnToZeroSubscriptions)
{
    FakeScannerRuntimeService service{};
    for (int i = 0; i < 100; ++i)
    {
        Consumer consumer{service};
        (void) consumer;
    }
    EXPECT_EQ(service.active_subscription_count(), 0u);
}

TEST(ConsumerLeakTest, EventDuringDestructionDoesNotCrash)
{
    FakeScannerRuntimeService service{};
    for (int i = 0; i < 50; ++i)
    {
        auto consumer = std::make_unique<Consumer>(service);
        service.inject_event(ScannerEvent{.kind = ScannerEventKind::ScanComplete});
        consumer.reset();
        service.inject_event(ScannerEvent{.kind = ScannerEventKind::ScanComplete});
    }
    EXPECT_EQ(service.active_subscription_count(), 0u);
}

TEST(ConsumerLeakTest, SimulatePluginUnloadFiresEvents)
{
    FakeScannerRuntimeService service{};
    Vertex::Scanner::TypeSchema pluginSchema{};
    pluginSchema.id = static_cast<Vertex::Scanner::TypeId>(1000);
    pluginSchema.name = "Plug";
    pluginSchema.kind = Vertex::Scanner::TypeKind::PluginDefined;
    pluginSchema.sourcePluginIndex = 9;
    service.install_type(pluginSchema);

    int unregistered{};
    int invalidated{};
    const auto sub = service.subscribe(
        static_cast<ScannerEventKindMask>(ScannerEventKind::TypeUnregistered)
            | ScannerEventKind::RegistryInvalidated,
        [&](const ScannerEvent& evt)
        {
            if (evt.kind == ScannerEventKind::TypeUnregistered) ++unregistered;
            if (evt.kind == ScannerEventKind::RegistryInvalidated) ++invalidated;
        });

    const auto removed = service.simulate_plugin_unload(9);
    EXPECT_EQ(removed, 1u);
    EXPECT_EQ(unregistered, 1);
    EXPECT_EQ(invalidated, 1);

    service.unsubscribe(sub);
}
