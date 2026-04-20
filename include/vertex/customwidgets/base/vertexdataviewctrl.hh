//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dataview.h>
#include <wx/string.h>

namespace Vertex::CustomWidgets::Base
{
    class VertexDataViewCtrl : public wxDataViewCtrl
    {
    public:
        VertexDataViewCtrl(wxWindow* parent, wxWindowID id, long style);
        ~VertexDataViewCtrl() override = default;

        VertexDataViewCtrl(const VertexDataViewCtrl&) = delete;
        VertexDataViewCtrl& operator=(const VertexDataViewCtrl&) = delete;
        VertexDataViewCtrl(VertexDataViewCtrl&&) = delete;
        VertexDataViewCtrl& operator=(VertexDataViewCtrl&&) = delete;

        wxDataViewColumn* append_text_column(
            const wxString& title,
            unsigned int modelColumn,
            int dipWidth,
            wxAlignment alignment = wxALIGN_NOT,
            bool sortable = true,
            bool resizable = true
        );

        void set_filter_text(const wxString& filterText);
        void clear_filter();

        [[nodiscard]] const wxString& get_filter_text() const noexcept;
        [[nodiscard]] bool has_filterable_model() const noexcept;

        [[nodiscard]] unsigned int get_source_row(unsigned int viewRow) const noexcept;

    private:
        void on_column_header_click(wxDataViewEvent& event);

        wxString m_filterText{};
        int m_sortColumnIndex{-1};
        bool m_sortAscending{true};
    };
}
