//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <functional>
#include <span>
#include <vector>

#include <wx/font.h>
#include <wx/panel.h>
#include <wx/string.h>

#include <vertex/customwidgets/base/columndefinition.hh>

namespace Vertex::CustomWidgets::Base
{
    class VertexListHeader final : public wxPanel
    {
    public:
        using ColumnResizeCallback = std::function<void(int columnIndex, int newWidth)>;
        using ColumnAutoSizeCallback = std::function<void(int columnIndex)>;
        using ColumnSortClickCallback = std::function<void(int columnIndex)>;

        VertexListHeader(wxWindow* parent, const wxFont& font);
        ~VertexListHeader() override = default;

        VertexListHeader(const VertexListHeader&) = delete;
        VertexListHeader& operator=(const VertexListHeader&) = delete;
        VertexListHeader(VertexListHeader&&) = delete;
        VertexListHeader& operator=(VertexListHeader&&) = delete;

        void set_columns(std::vector<ColumnDefinition> columns);
        void set_column_width(int columnIndex, int width);
        void set_horizontal_scroll_offset(int offset) noexcept;
        void set_sort_indicator(int columnIndex, SortDirection direction);

        void set_column_resize_callback(ColumnResizeCallback callback);
        void set_column_auto_size_callback(ColumnAutoSizeCallback callback);
        void set_column_sort_click_callback(ColumnSortClickCallback callback);

        [[nodiscard]] int get_header_height() const noexcept;
        [[nodiscard]] int get_column_x(int columnIndex) const noexcept;
        [[nodiscard]] int get_total_width() const noexcept;
        [[nodiscard]] int get_column_padding() const noexcept;
        [[nodiscard]] std::span<const ColumnDefinition> get_columns() const noexcept;

    private:
        void on_paint(wxPaintEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_mouse_motion(wxMouseEvent& event);
        void on_mouse_left_down(wxMouseEvent& event);
        void on_mouse_left_up(wxMouseEvent& event);
        void on_mouse_left_dclick(wxMouseEvent& event);
        void on_mouse_capture_lost(wxMouseCaptureLostEvent& event);
        void on_mouse_leave(wxMouseEvent& event);

        [[nodiscard]] int get_separator_at_x(int x) const noexcept;
        [[nodiscard]] int get_column_at_x(int x) const noexcept;
        void recalculate_metrics();

        static constexpr int SEPARATOR_HIT_TOLERANCE{4};
        static constexpr int DEFAULT_COLUMN_PADDING{8};

        std::vector<ColumnDefinition> m_columns{};
        int m_headerHeight{};
        int m_charWidth{};
        int m_columnPadding{DEFAULT_COLUMN_PADDING};
        int m_hScrollOffset{};

        int m_sortColumnIndex{-1};
        SortDirection m_sortDirection{SortDirection::None};

        int m_resizingColumn{-1};
        int m_resizeStartX{};
        int m_resizeStartWidth{};

        int m_hoveredSeparator{-1};

        wxFont m_font{};
        wxFont m_fontBold{};

        ColumnResizeCallback m_resizeCallback{};
        ColumnAutoSizeCallback m_autoSizeCallback{};
        ColumnSortClickCallback m_sortClickCallback{};

        struct Colors final
        {
            wxColour background{};
            wxColour border{};
            wxColour text{};
            wxColour separatorHover{};
        } m_colors{};

        void load_system_colors();
    };
}
