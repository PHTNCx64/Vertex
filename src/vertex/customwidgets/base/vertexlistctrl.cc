//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/base/vertexlistctrl.hh>

#include <utility>

namespace Vertex::CustomWidgets::Base
{
    VertexListCtrl::VertexListCtrl(wxWindow* parent, const wxWindowID id, const long style)
        : wxListCtrl(parent, id, wxDefaultPosition, wxDefaultSize,
                     style | wxLC_VIRTUAL | wxLC_REPORT)
    {
        Bind(wxEVT_LIST_COL_CLICK, &VertexListCtrl::on_column_click, this);
    }

    void VertexListCtrl::append_column(const wxString& title, const int dipWidth,
                                       const wxListColumnFormat format)
    {
        const long columnIndex = GetColumnCount();
        InsertColumn(columnIndex, title, format, FromDIP(dipWidth));
    }

    void VertexListCtrl::set_model_row_count(const long count)
    {
        m_modelRowCount = count < 0 ? 0 : count;
        m_sortFilter.set_model_row_count(static_cast<int>(m_modelRowCount));
        invalidate_view();
    }

    long VertexListCtrl::get_model_row_count() const noexcept
    {
        return m_modelRowCount;
    }

    long VertexListCtrl::get_visible_row_count() const noexcept
    {
        return static_cast<long>(m_sortFilter.view_row_count());
    }

    long VertexListCtrl::model_index_for_view(const long viewIndex) const noexcept
    {
        return static_cast<long>(m_sortFilter.model_for_view(static_cast<int>(viewIndex)));
    }

    void VertexListCtrl::set_sort(const int columnIndex, const SortDirection direction)
    {
        m_sortColumn = direction == SortDirection::None ? -1 : columnIndex;
        m_sortDirection = direction;
        apply_sort_to_model();
        invalidate_view();
    }

    void VertexListCtrl::clear_sort()
    {
        m_sortColumn = -1;
        m_sortDirection = SortDirection::None;
        m_sortFilter.clear_sort();
        invalidate_view();
    }

    int VertexListCtrl::get_sort_column() const noexcept
    {
        return m_sortColumn;
    }

    SortDirection VertexListCtrl::get_sort_direction() const noexcept
    {
        return m_sortDirection;
    }

    void VertexListCtrl::set_filter_text(const wxString& filterText)
    {
        if (m_filterText == filterText)
        {
            return;
        }

        m_filterText = filterText;
        m_filterTextLower = filterText.Lower();

        apply_filter_to_model();
        invalidate_view();
    }

    void VertexListCtrl::clear_filter()
    {
        set_filter_text(wxString{});
    }

    const wxString& VertexListCtrl::get_filter_text() const noexcept
    {
        return m_filterText;
    }

    void VertexListCtrl::invalidate_view()
    {
        m_sortFilter.rebuild();
        refresh_item_count();
        Refresh();
    }

    void VertexListCtrl::auto_size_column(const int columnIndex)
    {
        const int width = compute_auto_size_width(columnIndex);
        if (width > 0)
        {
            SetColumnWidth(columnIndex, width);
            return;
        }
        SetColumnWidth(columnIndex, wxLIST_AUTOSIZE);
    }

    void VertexListCtrl::auto_size_all_columns()
    {
        const int columnCount = GetColumnCount();
        for (int i{}; i < columnCount; ++i)
        {
            auto_size_column(i);
        }
    }

    wxListItemAttr* VertexListCtrl::on_get_item_attr_for_model([[maybe_unused]] const long modelRowIndex) const
    {
        return nullptr;
    }

    int VertexListCtrl::on_get_item_image_for_model([[maybe_unused]] const long modelRowIndex) const
    {
        return -1;
    }

    std::strong_ordering VertexListCtrl::compare_rows(const long lhsModelRowIndex,
                                                      const long rhsModelRowIndex,
                                                      const int columnIndex) const
    {
        const wxString lhs = on_get_item_text_for_model(lhsModelRowIndex, columnIndex);
        const wxString rhs = on_get_item_text_for_model(rhsModelRowIndex, columnIndex);
        const int cmp = lhs.Cmp(rhs);
        if (cmp < 0)
        {
            return std::strong_ordering::less;
        }
        if (cmp > 0)
        {
            return std::strong_ordering::greater;
        }
        return std::strong_ordering::equal;
    }

    bool VertexListCtrl::passes_filter(const long modelRowIndex, const wxString& filterTextLower) const
    {
        if (filterTextLower.IsEmpty())
        {
            return true;
        }

        const int columnCount = GetColumnCount();
        for (int c{}; c < columnCount; ++c)
        {
            const wxString cellText = on_get_item_text_for_model(modelRowIndex, c).Lower();
            if (cellText.Contains(filterTextLower))
            {
                return true;
            }
        }
        return false;
    }

    int VertexListCtrl::compute_auto_size_width([[maybe_unused]] const int columnIndex) const
    {
        return 0;
    }

    wxString VertexListCtrl::OnGetItemText(const long item, const long column) const
    {
        const long modelIndex = model_index_for_view(item);
        if (modelIndex < 0)
        {
            return wxString{};
        }
        return on_get_item_text_for_model(modelIndex, column);
    }

    wxListItemAttr* VertexListCtrl::OnGetItemAttr(const long item) const
    {
        const long modelIndex = model_index_for_view(item);
        if (modelIndex < 0)
        {
            return nullptr;
        }
        return on_get_item_attr_for_model(modelIndex);
    }

    int VertexListCtrl::OnGetItemImage(const long item) const
    {
        const long modelIndex = model_index_for_view(item);
        if (modelIndex < 0)
        {
            return -1;
        }
        return on_get_item_image_for_model(modelIndex);
    }

    void VertexListCtrl::on_column_click(wxListEvent& event)
    {
        const int clickedColumn = event.GetColumn();
        if (clickedColumn < 0)
        {
            event.Skip();
            return;
        }

        SortDirection nextDirection{SortDirection::Ascending};
        if (m_sortColumn == clickedColumn)
        {
            switch (m_sortDirection)
            {
            case SortDirection::None:
                nextDirection = SortDirection::Ascending;
                break;
            case SortDirection::Ascending:
                nextDirection = SortDirection::Descending;
                break;
            case SortDirection::Descending:
                nextDirection = SortDirection::None;
                break;
            }
        }

        set_sort(clickedColumn, nextDirection);
        event.Skip();
    }

    void VertexListCtrl::apply_sort_to_model()
    {
        if (m_sortColumn < 0 || m_sortDirection == SortDirection::None)
        {
            m_sortFilter.clear_sort();
            return;
        }

        const int sortColumn = m_sortColumn;
        m_sortFilter.set_sort(
            [this, sortColumn](const int lhs, const int rhs) noexcept
            {
                return compare_rows(static_cast<long>(lhs),
                                    static_cast<long>(rhs),
                                    sortColumn);
            },
            m_sortDirection
        );
    }

    void VertexListCtrl::apply_filter_to_model()
    {
        if (m_filterTextLower.IsEmpty())
        {
            m_sortFilter.clear_filter();
            return;
        }

        m_sortFilter.set_filter(
            [this](const int modelRow) noexcept
            {
                return passes_filter(static_cast<long>(modelRow), m_filterTextLower);
            }
        );
    }

    void VertexListCtrl::refresh_item_count()
    {
        SetItemCount(get_visible_row_count());
    }
}
