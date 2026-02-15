//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/event.h>

#include <vertex/gui/enums/sortorder.hh>

namespace Vertex::Gui::Events
{
    class SortingEvent;

    wxDECLARE_EVENT(VRTX_EVT_SORTING, SortingEvent);

    class SortingEvent final : public wxEvent
    {
    public:
        explicit SortingEvent() = default;

        explicit SortingEvent(const wxEventType type, const int id = 0)
            : wxEvent(id, type)
        {
        }

        SortingEvent(const SortingEvent& event);

        [[nodiscard]] wxEvent* Clone() const override;
        [[nodiscard]] Enums::SortOrder get_sorting_order() const noexcept;
        void set_sorting_order(Enums::SortOrder order) noexcept;
        [[nodiscard]] int get_column_index() const noexcept;
        void set_column_index(int columnIndex) noexcept;

    private:
        Enums::SortOrder m_sortOrder {};
        int m_column {};
    };
}
