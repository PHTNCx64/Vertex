//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/command.hh>

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Vertex::Runtime
{
    template<class TKind, class TEvent>
    class Fanout final
    {
    public:
        using Mask = std::underlying_type_t<TKind>;
        using Callback = std::move_only_function<void(const TEvent&) const>;

        Fanout() = default;

        ~Fanout()
        {
            shutdown();
        }

        Fanout(const Fanout&) = delete;
        Fanout& operator=(const Fanout&) = delete;
        Fanout(Fanout&&) = delete;
        Fanout& operator=(Fanout&&) = delete;

        [[nodiscard]] SubscriptionId subscribe(Mask mask, Callback callback)
        {
            if (mask == 0 || !callback || m_shuttingDown.load(std::memory_order_acquire))
            {
                return INVALID_SUBSCRIPTION_ID;
            }

            auto subscriber = std::make_shared<Subscriber>();
            subscriber->mask = mask;
            subscriber->callback = std::move(callback);

            const auto id = m_nextId.fetch_add(1, std::memory_order_relaxed);

            std::unique_lock lock {m_registryMutex};
            if (m_shuttingDown.load(std::memory_order_acquire))
            {
                return INVALID_SUBSCRIPTION_ID;
            }
            m_subscribers.emplace(id, std::move(subscriber));
            return id;
        }

        void unsubscribe(SubscriptionId id) noexcept
        {
            if (id == INVALID_SUBSCRIPTION_ID)
            {
                return;
            }

            std::shared_ptr<Subscriber> subscriber {};
            {
                std::unique_lock lock {m_registryMutex};
                const auto it = m_subscribers.find(id);
                if (it == m_subscribers.end())
                {
                    return;
                }
                subscriber = std::move(it->second);
                m_subscribers.erase(it);
            }

            std::unique_lock subLock {subscriber->dispatchMutex};
            subscriber->active = false;
            subscriber->drainCv.wait(subLock, [&]
            {
                const auto self = std::this_thread::get_id();
                const auto selfCount = static_cast<std::uint32_t>(std::count(
                    subscriber->dispatchers.begin(),
                    subscriber->dispatchers.end(),
                    self));
                return subscriber->inFlight == selfCount;
            });
        }

        void fire(const TEvent& event)
        {
            if (m_shuttingDown.load(std::memory_order_acquire))
            {
                return;
            }

            const auto kindBit = static_cast<Mask>(event.kind);
            if (kindBit == 0)
            {
                return;
            }

            std::vector<std::shared_ptr<Subscriber>> snapshot {};
            {
                std::shared_lock lock {m_registryMutex};
                snapshot.reserve(m_subscribers.size());
                for (const auto& [id, sub] : m_subscribers)
                {
                    if ((sub->mask & kindBit) != 0)
                    {
                        snapshot.emplace_back(sub);
                    }
                }
            }

            const auto selfId = std::this_thread::get_id();
            for (const auto& subscriber : snapshot)
            {
                {
                    std::scoped_lock subLock {subscriber->dispatchMutex};
                    if (!subscriber->active || !subscriber->callback)
                    {
                        continue;
                    }
                    ++subscriber->inFlight;
                    subscriber->dispatchers.emplace_back(selfId);
                }

                struct DispatchRelease final
                {
                    Subscriber& sub;
                    std::thread::id id;
                    ~DispatchRelease()
                    {
                        {
                            std::scoped_lock l {sub.dispatchMutex};
                            if (sub.inFlight > 0)
                            {
                                --sub.inFlight;
                            }
                            const auto it = std::find(sub.dispatchers.begin(),
                                                      sub.dispatchers.end(),
                                                      id);
                            if (it != sub.dispatchers.end())
                            {
                                sub.dispatchers.erase(it);
                            }
                        }
                        sub.drainCv.notify_all();
                    }
                } release {*subscriber, selfId};

                subscriber->callback(event);
            }
        }

        void shutdown() noexcept
        {
            bool expected = false;
            if (!m_shuttingDown.compare_exchange_strong(expected, true,
                                                       std::memory_order_acq_rel))
            {
                return;
            }

            std::unordered_map<SubscriptionId, std::shared_ptr<Subscriber>> drained {};
            {
                std::unique_lock lock {m_registryMutex};
                drained.swap(m_subscribers);
            }

            for (auto& [id, subscriber] : drained)
            {
                std::unique_lock subLock {subscriber->dispatchMutex};
                subscriber->active = false;
                subscriber->drainCv.wait(subLock, [&]
                {
                    const auto self = std::this_thread::get_id();
                    const auto selfCount = static_cast<std::uint32_t>(std::count(
                        subscriber->dispatchers.begin(),
                        subscriber->dispatchers.end(),
                        self));
                    return subscriber->inFlight == selfCount;
                });
            }
        }

        [[nodiscard]] std::size_t subscriber_count() const noexcept
        {
            std::shared_lock lock {m_registryMutex};
            return m_subscribers.size();
        }

    private:
        struct Subscriber final
        {
            Mask mask {};
            Callback callback {};
            std::mutex dispatchMutex {};
            std::uint32_t inFlight {0};
            std::condition_variable drainCv {};
            bool active {true};
            std::vector<std::thread::id> dispatchers {};
        };

        mutable std::shared_mutex m_registryMutex {};
        std::unordered_map<SubscriptionId, std::shared_ptr<Subscriber>> m_subscribers {};
        std::atomic<SubscriptionId> m_nextId {1};
        std::atomic<bool> m_shuttingDown {false};
    };
}
