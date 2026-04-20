//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <compare>

#include <wx/listctrl.h>
#include <wx/string.h>

#include <vertex/customwidgets/base/columndefinition.hh>
#include <vertex/customwidgets/base/sortfiltermodel.hh>

namespace Vertex::CustomWidgets::Base
{
    class VertexListCtrl : public wxListCtrl
    {
    public:
        VertexListCtrl(wxWindow* parent, wxWindowID id, long style);
        ~VertexListCtrl() override = default;

        VertexListCtrl(const VertexListCtrl&) = delete;
        VertexListCtrl& operator=(const VertexListCtrl&) = delete;
        VertexListCtrl(VertexListCtrl&&) = delete;
        VertexListCtrl& operator=(VertexListCtrl&&) = delete;

        void append_column(const wxString& title, int dipWidth,
                           wxListColumnFormat format = wxLIST_FORMAT_LEFT);

        void set_model_row_count(long count);
        [[nodiscard]] long get_model_row_count() const noexcept;
        [[nodiscard]] long get_visible_row_count() const noexcept;
        [[nodiscard]] long model_index_for_view(long viewIndex) const noexcept;

        void set_sort(int columnIndex, SortDirection direction);
        void clear_sort();
        [[nodiscard]] int get_sort_column() const noexcept;
        [[nodiscard]] SortDirection get_sort_direction() const noexcept;

        void set_filter_text(const wxString& filterText);
        void clear_filter();
        [[nodiscard]] const wxString& get_filter_text() const noexcept;

        void invalidate_view();

        void auto_size_column(int columnIndex);
        void auto_size_all_columns();

    protected:
        [[nodiscard]] virtual wxString on_get_item_text_for_model(long modelRowIndex,
                                                                  long columnIndex) const = 0;
        [[nodiscard]] virtual wxListItemAttr* on_get_item_attr_for_model(long modelRowIndex) const;
        [[nodiscard]] virtual int on_get_item_image_for_model(long modelRowIndex) const;

        [[nodiscard]] virtual std::strong_ordering compare_rows(long lhsModelRowIndex,
                                                                long rhsModelRowIndex,
                                                                int columnIndex) const;
        [[nodiscard]] virtual bool passes_filter(long modelRowIndex,
                                                 const wxString& filterTextLower) const;
        [[nodiscard]] virtual int compute_auto_size_width(int columnIndex) const;

    private:
        [[nodiscard]] wxString OnGetItemText(long item, long column) const override;
        [[nodiscard]] wxListItemAttr* OnGetItemAttr(long item) const override;
        [[nodiscard]] int OnGetItemImage(long item) const override;

        void on_column_click(wxListEvent& event);

        void apply_sort_to_model();
        void apply_filter_to_model();
        void refresh_item_count();

        long m_modelRowCount{};
        int m_sortColumn{-1};
        SortDirection m_sortDirection{SortDirection::None};

        wxString m_filterText{};
        wxString m_filterTextLower{};

        SortFilterModel m_sortFilter{};
    };
}
