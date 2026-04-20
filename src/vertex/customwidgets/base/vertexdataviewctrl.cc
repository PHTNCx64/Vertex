//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/base/vertexdataviewctrl.hh>

#include <vertex/customwidgets/base/ifilterabledataviewmodel.hh>
#include <vertex/customwidgets/base/isortabledataviewmodel.hh>

namespace Vertex::CustomWidgets::Base
{
    VertexDataViewCtrl::VertexDataViewCtrl(wxWindow* parent, const wxWindowID id, const long style)
        : wxDataViewCtrl(parent, id, wxDefaultPosition, wxDefaultSize, style)
    {
        Bind(wxEVT_DATAVIEW_COLUMN_HEADER_CLICK, &VertexDataViewCtrl::on_column_header_click, this);
    }

    wxDataViewColumn* VertexDataViewCtrl::append_text_column(
        const wxString& title,
        const unsigned int modelColumn,
        const int dipWidth,
        const wxAlignment alignment,
        const bool sortable,
        const bool resizable
    )
    {
        int flags{};
        if (sortable)
        {
            flags |= wxDATAVIEW_COL_SORTABLE;
        }
        if (resizable)
        {
            flags |= wxDATAVIEW_COL_RESIZABLE;
        }

        return AppendTextColumn(
            title,
            modelColumn,
            wxDATAVIEW_CELL_INERT,
            FromDIP(dipWidth),
            alignment,
            flags
        );
    }

    void VertexDataViewCtrl::set_filter_text(const wxString& filterText)
    {
        if (m_filterText == filterText)
        {
            return;
        }

        m_filterText = filterText;

        wxDataViewModel* model = GetModel();
        if (model == nullptr)
        {
            return;
        }

        auto* filterable = dynamic_cast<IFilterableDataViewModel*>(model);
        if (filterable == nullptr)
        {
            return;
        }

        filterable->set_filter_text(m_filterText);
    }

    void VertexDataViewCtrl::clear_filter()
    {
        set_filter_text(wxString{});
    }

    const wxString& VertexDataViewCtrl::get_filter_text() const noexcept
    {
        return m_filterText;
    }

    bool VertexDataViewCtrl::has_filterable_model() const noexcept
    {
        const wxDataViewModel* model = GetModel();
        if (model == nullptr)
        {
            return false;
        }
        return dynamic_cast<const IFilterableDataViewModel*>(model) != nullptr;
    }

    unsigned int VertexDataViewCtrl::get_source_row(const unsigned int viewRow) const noexcept
    {
        const wxDataViewModel* model = GetModel();
        if (model == nullptr)
        {
            return viewRow;
        }
        const auto* sortable = dynamic_cast<const ISortableDataViewModel*>(model);
        if (sortable == nullptr || !sortable->is_sort_active())
        {
            return viewRow;
        }
        return sortable->get_source_row(viewRow);
    }

    void VertexDataViewCtrl::on_column_header_click(wxDataViewEvent& event)
    {
        wxDataViewColumn* column = event.GetDataViewColumn();
        if (column == nullptr)
        {
            event.Skip();
            return;
        }
        if ((column->GetFlags() & wxDATAVIEW_COL_SORTABLE) == 0)
        {
            event.Skip();
            return;
        }
        wxDataViewModel* model = GetModel();
        auto* sortable = dynamic_cast<ISortableDataViewModel*>(model);
        if (sortable == nullptr)
        {
            event.Skip();
            return;
        }
        const auto modelCol = static_cast<int>(column->GetModelColumn());
        bool ascending{true};
        if (m_sortColumnIndex == modelCol)
        {
            ascending = !m_sortAscending;
        }
        m_sortColumnIndex = modelCol;
        m_sortAscending = ascending;
        const unsigned int colCount = GetColumnCount();
        for (unsigned int i{}; i < colCount; ++i)
        {
            wxDataViewColumn* col = GetColumn(i);
            if (col == column)
            {
                col->SetSortOrder(ascending);
            }
            else
            {
                col->UnsetAsSortKey();
            }
        }
        sortable->sort_by(static_cast<unsigned int>(modelCol), ascending);
        Refresh();
    }
}
