//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/event.h>
#include <wx/string.h>

#include <vertex/gui/enums/filtertype.hh>

namespace Vertex::Gui::Events
{
    class ProcessListFilterEvent;

    wxDECLARE_EVENT(VRTX_EVT_PROCESS_LIST_FILTER, ProcessListFilterEvent);

    class ProcessListFilterEvent final : public wxEvent
    {
    public:
        explicit ProcessListFilterEvent() = default;

        explicit ProcessListFilterEvent(const wxEventType type, const int id = 0)
            : wxEvent(id, type)
        {
        }

        ProcessListFilterEvent(const ProcessListFilterEvent& event);

        [[nodiscard]] wxEvent* Clone() const override;
        void set_should_filter(bool flag) noexcept;
        void set_filter_type(Enums::FilterType type) noexcept;
        void set_filter_value(const wxString& value) noexcept;

        [[nodiscard]] bool get_should_filter() const noexcept;
        [[nodiscard]] Enums::FilterType get_filter_type() const noexcept;
        [[nodiscard]] wxString get_filter_value() const noexcept;

    private:
        bool m_shouldFilter {};
        Enums::FilterType m_filterType {};
        wxString m_filterValue {};
    };
}
