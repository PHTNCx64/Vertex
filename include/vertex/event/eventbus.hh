//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/eventid.hh>
#include <functional>
#include <string>
#include <shared_mutex>
#include <unordered_map>
#include <vector>
#include <atomic>

namespace Vertex::Event
{
    class VertexEvent;

    using SubscriptionId = std::uint64_t;

    class EventBus final
    {
    public:
        EventBus() = default;
        ~EventBus() = default;

        EventBus(const EventBus&) = delete;
        EventBus& operator=(const EventBus&) = delete;
        EventBus(EventBus&&) = delete;
        EventBus& operator=(EventBus&&) = delete;

        SubscriptionId subscribe(std::string_view subscriberName, EventId id,
                                  std::function<void(const VertexEvent&)> callback);

        template<class TEvent>
        SubscriptionId subscribe(std::string_view subscriberName, EventId id,
                                  std::function<void(const TEvent&)> callback)
        {
            static_assert(std::is_base_of_v<VertexEvent, TEvent>, "TEvent must derive from VertexEvent!");

            return subscribe(subscriberName, id,
                [cb = std::move(callback)](const VertexEvent& event)
                {
                    cb(static_cast<const TEvent&>(event));
                });
        }

        [[nodiscard]] bool unsubscribe(SubscriptionId subscriptionId);

        void unsubscribe(std::string_view subscriberName, EventId id);
        void unsubscribe_all(std::string_view subscriberName);

        void broadcast(const VertexEvent& event) const;
        void broadcast_to(std::string_view subscriber, const VertexEvent& event) const;

        [[nodiscard]] std::size_t subscription_count() const;
        [[nodiscard]] std::size_t subscription_count(EventId id) const;

    private:
        struct Subscription final
        {
            SubscriptionId id {};
            std::string subscriberName {};
            EventId eventId {};
            std::function<void(const VertexEvent&)> callback {};
        };

        void remove_from_index(SubscriptionId id);

        mutable std::shared_mutex m_mutex;
        std::unordered_map<EventId, std::vector<Subscription>> m_subscriptionsByEvent;
        std::unordered_map<SubscriptionId, EventId> m_subscriptionIndex;
        std::atomic<SubscriptionId> m_nextSubscriptionId {1};
    };
}
