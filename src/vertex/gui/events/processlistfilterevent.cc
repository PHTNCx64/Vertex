//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/gui/events/processlistfilterevent.hh>

namespace Vertex::Gui::Events
{
    wxDEFINE_EVENT(VRTX_EVT_PROCESS_LIST_FILTER, ProcessListFilterEvent);

    ProcessListFilterEvent::ProcessListFilterEvent(const ProcessListFilterEvent& event) : wxEvent(event)
    {
        m_shouldFilter = event.m_shouldFilter;
        m_filterType = event.m_filterType;
        m_filterValue = event.m_filterValue;
    }

    wxEvent* ProcessListFilterEvent::Clone() const
    {
        return new ProcessListFilterEvent(*this);
    }

    Enums::FilterType ProcessListFilterEvent::get_filter_type() const noexcept
    {
        return m_filterType;
    }

    wxString ProcessListFilterEvent::get_filter_value() const noexcept
    {
        return m_filterValue;
    }

    bool ProcessListFilterEvent::get_should_filter() const noexcept
    {
        return m_shouldFilter;
    }

    void ProcessListFilterEvent::set_filter_type(const Enums::FilterType type) noexcept
    {
        m_filterType = type;
    }

    void ProcessListFilterEvent::set_filter_value(const wxString& value) noexcept
    {
        m_filterValue = value;
    }

    void ProcessListFilterEvent::set_should_filter(const bool flag) noexcept
    {
        m_shouldFilter = flag;
    }
}
