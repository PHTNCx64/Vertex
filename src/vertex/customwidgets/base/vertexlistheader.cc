//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/base/vertexlistheader.hh>

#include <algorithm>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/dcclient.h>
#include <wx/settings.h>

namespace Vertex::CustomWidgets::Base
{
    VertexListHeader::VertexListHeader(wxWindow* parent, const wxFont& font)
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE)
        , m_font(font)
        , m_fontBold(font)
    {
        m_fontBold.SetWeight(wxFONTWEIGHT_BOLD);
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetDoubleBuffered(true);

        m_columnPadding = FromDIP(DEFAULT_COLUMN_PADDING);

        load_system_colors();
        recalculate_metrics();

        Bind(wxEVT_PAINT, &VertexListHeader::on_paint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &VertexListHeader::on_erase_background, this);
        Bind(wxEVT_MOTION, &VertexListHeader::on_mouse_motion, this);
        Bind(wxEVT_LEFT_DOWN, &VertexListHeader::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_UP, &VertexListHeader::on_mouse_left_up, this);
        Bind(wxEVT_LEFT_DCLICK, &VertexListHeader::on_mouse_left_dclick, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &VertexListHeader::on_mouse_capture_lost, this);
        Bind(wxEVT_LEAVE_WINDOW, &VertexListHeader::on_mouse_leave, this);
    }

    void VertexListHeader::set_columns(std::vector<ColumnDefinition> columns)
    {
        m_columns = std::move(columns);
        recalculate_metrics();
        SetMinSize(wxSize(-1, m_headerHeight));
        Refresh();
    }

    void VertexListHeader::set_column_width(const int columnIndex, const int width)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
        {
            return;
        }
        m_columns[static_cast<std::size_t>(columnIndex)].width = std::max(width,
            m_columns[static_cast<std::size_t>(columnIndex)].minWidth);
        Refresh();
    }

    void VertexListHeader::set_horizontal_scroll_offset(const int offset) noexcept
    {
        if (m_hScrollOffset == offset)
        {
            return;
        }
        m_hScrollOffset = offset;
        Refresh();
    }

    void VertexListHeader::set_sort_indicator(const int columnIndex, const SortDirection direction)
    {
        m_sortColumnIndex = columnIndex;
        m_sortDirection = direction;
        Refresh();
    }

    void VertexListHeader::set_column_resize_callback(ColumnResizeCallback callback)
    {
        m_resizeCallback = std::move(callback);
    }

    void VertexListHeader::set_column_auto_size_callback(ColumnAutoSizeCallback callback)
    {
        m_autoSizeCallback = std::move(callback);
    }

    void VertexListHeader::set_column_sort_click_callback(ColumnSortClickCallback callback)
    {
        m_sortClickCallback = std::move(callback);
    }

    int VertexListHeader::get_header_height() const noexcept
    {
        return m_headerHeight;
    }

    int VertexListHeader::get_column_x(const int columnIndex) const noexcept
    {
        if (columnIndex < 0 || columnIndex > static_cast<int>(m_columns.size()))
        {
            return 0;
        }
        int x{m_columnPadding};
        for (int i{}; i < columnIndex; ++i)
        {
            x += m_columns[static_cast<std::size_t>(i)].width + m_columnPadding;
        }
        return x;
    }

    int VertexListHeader::get_total_width() const noexcept
    {
        int total{m_columnPadding};
        for (const auto& column : m_columns)
        {
            total += column.width + m_columnPadding;
        }
        return total;
    }

    std::span<const ColumnDefinition> VertexListHeader::get_columns() const noexcept
    {
        return m_columns;
    }

    int VertexListHeader::get_column_padding() const noexcept
    {
        return m_columnPadding;
    }

    void VertexListHeader::load_system_colors()
    {
        m_colors.background = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNFACE);
        m_colors.border = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);
        m_colors.text = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNTEXT);
        m_colors.separatorHover = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
    }

    void VertexListHeader::recalculate_metrics()
    {
        wxClientDC dc(this);
        dc.SetFont(m_fontBold);
        m_charWidth = dc.GetCharWidth();
        m_headerHeight = dc.GetCharHeight() + m_columnPadding;
    }

    int VertexListHeader::get_separator_at_x(const int x) const noexcept
    {
        const int columnCount = static_cast<int>(m_columns.size());
        int cursor{m_columnPadding - m_hScrollOffset};
        for (int i{}; i < columnCount - 1; ++i)
        {
            cursor += m_columns[static_cast<std::size_t>(i)].width + m_columnPadding;
            const int separatorX = cursor - (m_columnPadding / 2);
            if (std::abs(x - separatorX) <= SEPARATOR_HIT_TOLERANCE &&
                m_columns[static_cast<std::size_t>(i)].resizable)
            {
                return i;
            }
        }
        return -1;
    }

    int VertexListHeader::get_column_at_x(const int x) const noexcept
    {
        int cursor{m_columnPadding - m_hScrollOffset};
        for (int i{}; i < static_cast<int>(m_columns.size()); ++i)
        {
            const int contentStart = cursor;
            const int contentEnd = contentStart + m_columns[static_cast<std::size_t>(i)].width;
            if (x >= contentStart && x < contentEnd)
            {
                return i;
            }
            cursor = contentEnd + m_columnPadding;
        }
        return -1;
    }

    void VertexListHeader::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    void VertexListHeader::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.SetFont(m_fontBold);

        const wxSize clientSize = GetClientSize();
        const int clientWidth = clientSize.GetWidth();
        const int clientHeight = clientSize.GetHeight();
        if (clientWidth <= 0 || clientHeight <= 0)
        {
            return;
        }

        dc.SetBrush(wxBrush(m_colors.background));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(0, 0, clientWidth, clientHeight);

        int cursor{m_columnPadding - m_hScrollOffset};
        dc.SetTextForeground(m_colors.text);
        const int textY = (clientHeight - dc.GetCharHeight()) / 2;
        const int columnCount = static_cast<int>(m_columns.size());

        for (int i{}; i < columnCount; ++i)
        {
            const auto& column = m_columns[static_cast<std::size_t>(i)];
            const int contentX = cursor;
            const int contentWidth = column.width;
            const wxSize titleExtent = dc.GetTextExtent(column.title);

            int textX{};
            switch (column.alignment)
            {
            case ColumnAlignment::Right:
                textX = contentX + contentWidth - titleExtent.GetWidth();
                break;
            case ColumnAlignment::Center:
                textX = contentX + (contentWidth - titleExtent.GetWidth()) / 2;
                break;
            case ColumnAlignment::Left:
            default:
                textX = contentX;
                break;
            }

            if (contentWidth > 0)
            {
                dc.DestroyClippingRegion();
                dc.SetClippingRegion(contentX, 0, contentWidth, clientHeight);
                dc.DrawText(column.title, textX, textY);
                dc.DestroyClippingRegion();
            }

            if (m_sortColumnIndex == i && m_sortDirection != SortDirection::None)
            {
                const wxString indicator = m_sortDirection == SortDirection::Ascending
                    ? wxString::FromUTF8("\xE2\x96\xB2")
                    : wxString::FromUTF8("\xE2\x96\xBC");
                const wxSize indicatorSize = dc.GetTextExtent(indicator);
                const int indicatorX = contentX + contentWidth - indicatorSize.GetWidth();
                const int indicatorY = (clientHeight - indicatorSize.GetHeight()) / 2;
                dc.DrawText(indicator, indicatorX, indicatorY);
            }

            cursor = contentX + contentWidth + m_columnPadding;

            if (i < columnCount - 1)
            {
                const int separatorX = cursor - (m_columnPadding / 2);
                const bool isHoveredSeparator = m_hoveredSeparator == i;
                dc.SetPen(wxPen(isHoveredSeparator ? m_colors.separatorHover : m_colors.border));
                const int separatorTop = std::min(2, clientHeight - 1);
                const int separatorBottom = std::max(separatorTop, clientHeight - 2);
                dc.DrawLine(separatorX, separatorTop, separatorX, separatorBottom);
            }
        }

        dc.SetPen(wxPen(m_colors.border));
        dc.DrawLine(0, clientHeight - 1, clientWidth, clientHeight - 1);
    }

    void VertexListHeader::on_mouse_motion(wxMouseEvent& event)
    {
        const int x = event.GetX();

        if (m_resizingColumn >= 0)
        {
            const int delta = x - m_resizeStartX;
            const int newWidth = std::max(
                m_resizeStartWidth + delta,
                m_columns[static_cast<std::size_t>(m_resizingColumn)].minWidth);
            m_columns[static_cast<std::size_t>(m_resizingColumn)].width = newWidth;
            if (m_resizeCallback)
            {
                m_resizeCallback(m_resizingColumn, newWidth);
            }
            Refresh();
            return;
        }

        const int separatorIndex = get_separator_at_x(x);
        if (separatorIndex != m_hoveredSeparator)
        {
            m_hoveredSeparator = separatorIndex;
            SetCursor(separatorIndex >= 0 ? wxCursor(wxCURSOR_SIZEWE) : wxNullCursor);
            Refresh();
        }

        event.Skip();
    }

    void VertexListHeader::on_mouse_left_down(wxMouseEvent& event)
    {
        const int x = event.GetX();
        const int separatorIndex = get_separator_at_x(x);

        if (separatorIndex >= 0)
        {
            m_resizingColumn = separatorIndex;
            m_resizeStartX = x;
            m_resizeStartWidth = m_columns[static_cast<std::size_t>(separatorIndex)].width;
            CaptureMouse();
            return;
        }

        event.Skip();
    }

    void VertexListHeader::on_mouse_left_up(wxMouseEvent& event)
    {
        if (m_resizingColumn >= 0)
        {
            m_resizingColumn = -1;
            if (HasCapture())
            {
                ReleaseMouse();
            }
            return;
        }

        const int x = event.GetX();
        const int columnIndex = get_column_at_x(x);
        if (columnIndex >= 0 &&
            m_columns[static_cast<std::size_t>(columnIndex)].sortable &&
            m_sortClickCallback)
        {
            m_sortClickCallback(columnIndex);
        }

        event.Skip();
    }

    void VertexListHeader::on_mouse_left_dclick(wxMouseEvent& event)
    {
        const int x = event.GetX();
        const int separatorIndex = get_separator_at_x(x);
        if (separatorIndex >= 0 &&
            m_columns[static_cast<std::size_t>(separatorIndex)].autoSizable &&
            m_autoSizeCallback)
        {
            m_autoSizeCallback(separatorIndex);
            return;
        }
        event.Skip();
    }

    void VertexListHeader::on_mouse_capture_lost([[maybe_unused]] wxMouseCaptureLostEvent& event)
    {
        m_resizingColumn = -1;
    }

    void VertexListHeader::on_mouse_leave([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_hoveredSeparator >= 0 && m_resizingColumn < 0)
        {
            m_hoveredSeparator = -1;
            SetCursor(wxNullCursor);
            Refresh();
        }
    }
}
