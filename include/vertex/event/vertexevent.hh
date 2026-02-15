//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <any>

#include <vertex/event/eventid.hh>

namespace Vertex::Event
{
    class VertexEvent
    {
    public:
        explicit VertexEvent(const EventId eventId) : m_eventId(eventId) {}

        [[nodiscard]] EventId get_id() const noexcept
        {
            return m_eventId;
        }

        template<class T>
        void set_data(const T& data)
        {
            m_data = data;
        }

        template<class T>
        [[nodiscard]] T get_data() const
        {
            return std::any_cast<T>(m_data);
        }

    protected:
        EventId m_eventId {};
        std::any m_data {};
    };
}
