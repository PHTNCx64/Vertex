//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/vertexevent.hh>
#include <vertex/event/eventid.hh>
#include <vertex/utility.hh>

namespace Vertex::Event
{
    class ViewUpdateEvent final : public VertexEvent
    {
    public:
        explicit ViewUpdateEvent(const ViewUpdateFlags flags)
            : VertexEvent(VIEW_UPDATE_EVENT), m_updateFlags(flags)
        {}

        [[nodiscard]] ViewUpdateFlags get_update_flags() const noexcept
        {
            return m_updateFlags;
        }

    private:
        ViewUpdateFlags m_updateFlags {};
    };
}
