//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/event/vertexevent.hh>
#include <vertex/event/eventbus.hh>

#include <algorithm>
#include <array>
#include <ranges>

namespace Vertex::Event
{
    SubscriptionId EventBus::subscribe(const std::string_view subscriberName, const EventId id,
                                        std::function<void(const VertexEvent&)> callback)
    {
        const auto subscriptionId = m_nextSubscriptionId.fetch_add(1, std::memory_order_relaxed);

        std::scoped_lock lock(m_mutex);
        m_subscriptionsByEvent[id].push_back({subscriptionId, std::string{subscriberName}, id, std::move(callback)});
        m_subscriptionIndex.emplace(subscriptionId, id);

        return subscriptionId;
    }

    bool EventBus::unsubscribe(const SubscriptionId subscriptionId)
    {
        std::scoped_lock lock(m_mutex);

        const auto indexIt = m_subscriptionIndex.find(subscriptionId);
        if (indexIt == m_subscriptionIndex.end())
        {
            return false;
        }

        const EventId eventId = indexIt->second;
        m_subscriptionIndex.erase(indexIt);

        auto& subscriptions = m_subscriptionsByEvent[eventId];
        const auto it = std::ranges::find(subscriptions, subscriptionId, &Subscription::id);

        if (it != subscriptions.end())
        {
            if (it != subscriptions.end() - 1)
            {
                std::iter_swap(it, subscriptions.end() - 1);
            }
            subscriptions.pop_back();
            return true;
        }

        return false;
    }

    void EventBus::unsubscribe(const std::string_view subscriberName, const EventId id)
    {
        std::scoped_lock lock(m_mutex);

        const auto it = m_subscriptionsByEvent.find(id);
        if (it == m_subscriptionsByEvent.end())
        {
            return;
        }

        auto& subscriptions = it->second;

        std::erase_if(subscriptions,
            [this, &subscriberName](const Subscription& sub)
            {
                if (sub.subscriberName == subscriberName)
                {
                    m_subscriptionIndex.erase(sub.id);
                    return true;
                }
                return false;
            });
    }

    void EventBus::unsubscribe_all(const std::string_view subscriberName)
    {
        std::scoped_lock lock(m_mutex);

        for (auto& subscriptions : m_subscriptionsByEvent | std::views::values)
        {
            std::erase_if(subscriptions,
                [this, &subscriberName](const Subscription& sub)
                {
                    if (sub.subscriberName == subscriberName)
                    {
                        m_subscriptionIndex.erase(sub.id);
                        return true;
                    }
                    return false;
                });
        }
    }

    void EventBus::broadcast(const VertexEvent& event) const
    {
        std::vector<std::function<void(const VertexEvent&)>> largeBuffer{};

        const std::function<void(const VertexEvent&)>* callbacks{};
        std::size_t callbackCount{};

        {
            std::shared_lock lock(m_mutex);

            const auto it = m_subscriptionsByEvent.find(event.get_id());
            if (it == m_subscriptionsByEvent.end() || it->second.empty())
            {
                return;
            }

            callbackCount = it->second.size();
            largeBuffer.reserve(callbackCount);
            for (const auto& sub : it->second)
            {
                largeBuffer.push_back(sub.callback);
            }
            callbacks = largeBuffer.data();
        }

        for (std::size_t i{}; i < callbackCount; ++i)
        {
            callbacks[i](event);
        }
    }

    void EventBus::broadcast_to(const std::string_view subscriber, const VertexEvent& event) const
    {
        constexpr std::size_t SMALL_BUFFER_SIZE{4};
        std::array<std::function<void(const VertexEvent&)>, SMALL_BUFFER_SIZE> smallBuffer{};
        std::vector<std::function<void(const VertexEvent&)>> largeBuffer{};

        std::function<void(const VertexEvent&)>* callbacks{};
        std::size_t callbackCount{};

        {
            std::shared_lock lock(m_mutex);

            const auto it = m_subscriptionsByEvent.find(event.get_id());
            if (it == m_subscriptionsByEvent.end())
            {
                return;
            }

            for (const auto& sub : it->second)
            {
                if (sub.subscriberName == subscriber)
                {
                    ++callbackCount;
                }
            }

            if (callbackCount == 0)
            {
                return;
            }

            if (callbackCount <= SMALL_BUFFER_SIZE)
            {
                std::size_t idx{};
                for (const auto& sub : it->second)
                {
                    if (sub.subscriberName == subscriber)
                    {
                        smallBuffer[idx++] = sub.callback;
                    }
                }
                callbacks = smallBuffer.data();
            }
            else
            {
                largeBuffer.reserve(callbackCount);
                for (const auto& sub : it->second)
                {
                    if (sub.subscriberName == subscriber)
                    {
                        largeBuffer.push_back(sub.callback);
                    }
                }
                callbacks = largeBuffer.data();
            }
        }

        for (std::size_t i{}; i < callbackCount; ++i)
        {
            callbacks[i](event);
        }
    }

    std::size_t EventBus::subscription_count() const
    {
        std::shared_lock lock(m_mutex);
        return m_subscriptionIndex.size();
    }

    std::size_t EventBus::subscription_count(const EventId id) const
    {
        std::shared_lock lock(m_mutex);

        const auto it = m_subscriptionsByEvent.find(id);
        if (it != m_subscriptionsByEvent.end())
        {
            return it->second.size();
        }
        return 0;
    }

    void EventBus::remove_from_index(const SubscriptionId id)
    {
        m_subscriptionIndex.erase(id);
    }
}
