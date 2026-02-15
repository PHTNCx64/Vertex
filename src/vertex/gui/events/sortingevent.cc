//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/gui/events/sortingevent.hh>

namespace Vertex::Gui::Events
{
    SortingEvent::SortingEvent(const SortingEvent& event) : wxEvent(event)
    {
        m_sortOrder = event.m_sortOrder;
    }

    wxEvent* SortingEvent::Clone() const
    {
        return new SortingEvent(*this);
    }

    Enums::SortOrder SortingEvent::get_sorting_order() const noexcept
    {
        return m_sortOrder;
    }

    void SortingEvent::set_sorting_order(const Enums::SortOrder order) noexcept
    {
        m_sortOrder = order;
    }

    int SortingEvent::get_column_index() const noexcept
    {
        return m_column;
    }

    void SortingEvent::set_column_index(const int columnIndex) noexcept
    {
        m_column = columnIndex;
    }

    wxDEFINE_EVENT(VRTX_EVT_SORTING, SortingEvent);
}
