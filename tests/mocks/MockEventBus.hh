//
// Mock for EventBus
// Note: EventBus is a concrete class, so we create a wrapper interface for mocking
//

#pragma once

#include <gmock/gmock.h>
#include <vertex/event/eventbus.hh>
#include <vertex/event/vertexevent.hh>

namespace Vertex::Testing::Mocks
{
    // Interface wrapper for EventBus to enable mocking
    class IEventBus
    {
    public:
        virtual ~IEventBus() = default;

        virtual void subscribe(const std::string& subscriberName, Event::EventId id,
                             const std::function<void(const Event::VertexEvent&)>& callback) = 0;
        virtual void unsubscribe(const std::string& subscriberName, Event::EventId id) = 0;
        virtual void unsubscribe_all(const std::string& subscriberName) = 0;
        virtual void broadcast(const Event::VertexEvent& event) const = 0;
        virtual void broadcast_to(const std::string& subscriber, const Event::VertexEvent& event) const = 0;
    };

    class MockEventBus : public IEventBus
    {
    public:
        ~MockEventBus() override = default;

        MOCK_METHOD(void, subscribe,
                   (const std::string& subscriberName, Event::EventId id,
                    const std::function<void(const Event::VertexEvent&)>& callback),
                   (override));

        MOCK_METHOD(void, unsubscribe,
                   (const std::string& subscriberName, Event::EventId id),
                   (override));

        MOCK_METHOD(void, unsubscribe_all,
                   (const std::string& subscriberName),
                   (override));

        MOCK_METHOD(void, broadcast,
                   (const Event::VertexEvent& event),
                   (const, override));

        MOCK_METHOD(void, broadcast_to,
                   (const std::string& subscriber, const Event::VertexEvent& event),
                   (const, override));
    };
} // namespace Vertex::Testing::Mocks
