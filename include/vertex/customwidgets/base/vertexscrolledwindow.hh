//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <chrono>
#include <compare>
#include <optional>
#include <vector>

#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/scrolwin.h>
#include <wx/string.h>
#include <wx/timer.h>

#include <vertex/customwidgets/base/columndefinition.hh>
#include <vertex/customwidgets/base/sortfiltermodel.hh>
#include <vertex/customwidgets/base/vertexlistheader.hh>

namespace Vertex::CustomWidgets::Base
{
    class VertexScrolledWindow : public wxScrolledWindow
    {
    public:
        VertexScrolledWindow(wxWindow* parent, const wxFont& rowFont, VertexListHeader* header);
        ~VertexScrolledWindow() override;

        VertexScrolledWindow(const VertexScrolledWindow&) = delete;
        VertexScrolledWindow& operator=(const VertexScrolledWindow&) = delete;
        VertexScrolledWindow(VertexScrolledWindow&&) = delete;
        VertexScrolledWindow& operator=(VertexScrolledWindow&&) = delete;

        void set_columns(std::vector<ColumnDefinition> columns);
        void set_column_width(int columnIndex, int width);
        [[nodiscard]] int get_column_count() const noexcept;
        [[nodiscard]] const ColumnDefinition& get_column(int columnIndex) const;

        void set_row_count(int count);
        [[nodiscard]] int get_row_count() const noexcept;
        [[nodiscard]] int get_visible_row_count() const noexcept;

        void set_selected_row(int modelRowIndex);
        [[nodiscard]] int get_selected_row() const noexcept;
        [[nodiscard]] std::optional<int> get_row_at(wxPoint position) const;
        [[nodiscard]] std::optional<int> get_column_at(wxPoint position) const;

        void ensure_row_visible(int modelRowIndex);
        void refresh_visible_rows();

        void set_sort(int columnIndex, SortDirection direction);
        void clear_sort();
        [[nodiscard]] int get_sort_column() const noexcept;
        [[nodiscard]] SortDirection get_sort_direction() const noexcept;

        void set_filter_text(const wxString& filterText);
        void clear_filter();
        [[nodiscard]] const wxString& get_filter_text() const noexcept;

        void invalidate_view();

    protected:
        struct RowContext final
        {
            int modelRowIndex{};
            int viewRowIndex{};
            wxRect rowRect{};
            bool selected{};
            bool alternate{};
        };

        struct CellContext final
        {
            int modelRowIndex{};
            int columnIndex{};
            wxRect cellRect{};
            ColumnAlignment alignment{ColumnAlignment::Left};
            bool selected{};
        };

        struct Colors final
        {
            wxColour background{};
            wxColour backgroundAlt{};
            wxColour selected{};
            wxColour selectedText{};
            wxColour separator{};
            wxColour text{};
        };

        virtual void draw_row_background(wxDC& dc, const RowContext& context);
        virtual void draw_cell(wxDC& dc, const CellContext& context) = 0;
        virtual void on_row_activated(int modelRowIndex);
        virtual void on_row_context_menu(int modelRowIndex, wxPoint screenPosition);
        virtual void on_selection_changed(int newModelRowIndex);
        virtual void on_scroll_stopped();
        virtual void on_cell_clicked(int modelRowIndex, int columnIndex, wxPoint clientPosition);
        virtual void on_cell_activated(int modelRowIndex, int columnIndex, wxPoint clientPosition);
        virtual int  measure_cell_width(int modelRowIndex, int columnIndex) const;

        virtual std::strong_ordering compare_rows(int lhsModelRowIndex,
                                                  int rhsModelRowIndex,
                                                  int columnIndex) const;
        virtual wxString get_filter_text_for_row(int modelRowIndex, int columnIndex) const;
        virtual bool passes_filter(int modelRowIndex, const wxString& filterTextLower) const;

        [[nodiscard]] int get_row_height() const noexcept;
        [[nodiscard]] int get_column_padding() const noexcept;
        [[nodiscard]] const wxFont& get_row_font() const noexcept;
        [[nodiscard]] VertexListHeader* get_header() noexcept;
        [[nodiscard]] const Colors& get_colors() const noexcept;
        void set_colors(Colors colors);

        [[nodiscard]] int model_index_for_view(int viewIndex) const noexcept;
        [[nodiscard]] std::optional<int> view_index_for_model(int modelIndex) const noexcept;
        [[nodiscard]] wxRect get_cell_rect_for_view(int viewIndex, int columnIndex) const noexcept;

    private:
        void on_paint(wxPaintEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_size(wxSizeEvent& event);
        void on_mouse_left_down(wxMouseEvent& event);
        void on_mouse_left_dclick(wxMouseEvent& event);
        void on_mouse_right_down(wxMouseEvent& event);
        void on_mouse_wheel(wxMouseEvent& event);
        void on_key_down(wxKeyEvent& event);
        void on_scrollwin(wxScrollWinEvent& event);
        void on_scroll_stop_timer(wxTimerEvent& event);

        void on_column_resize(int columnIndex, int newWidth);
        void on_column_auto_size(int columnIndex);
        void on_column_sort_click(int columnIndex);

        [[nodiscard]] int get_y_for_view_row(int viewIndex) const noexcept;
        [[nodiscard]] int get_view_row_at_y(int y) const noexcept;
        [[nodiscard]] int get_column_at_x(int x) const noexcept;
        [[nodiscard]] wxRect compute_cell_rect(int viewIndex, int columnIndex) const noexcept;

        void update_virtual_size();
        void sync_header_scroll() const;
        void apply_sort_to_model();
        void apply_filter_to_model();
        void recalculate_row_height();
        void load_system_colors();

        static constexpr int SCROLL_STOP_DELAY_MS{150};

        wxFont m_rowFont{};
        int m_rowHeight{};
        VertexListHeader* m_header{};

        std::vector<ColumnDefinition> m_columns{};
        int m_rowCount{};

        int m_sortColumn{-1};
        SortDirection m_sortDirection{SortDirection::None};
        wxString m_filterText{};
        wxString m_filterTextLower{};

        SortFilterModel m_sortFilter{};

        int m_selectedModelRow{-1};

        wxTimer* m_scrollStopTimer{};
        bool m_isScrolling{};

        std::chrono::steady_clock::time_point m_lastLeftDownTime{};
        int m_lastLeftDownRow{-1};

        Colors m_colors{};
    };
}
