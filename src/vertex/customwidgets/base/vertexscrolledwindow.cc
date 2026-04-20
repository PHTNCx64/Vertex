//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/base/vertexscrolledwindow.hh>

#include <algorithm>
#include <utility>

#include <wx/dcbuffer.h>
#include <wx/settings.h>

namespace Vertex::CustomWidgets::Base
{
    VertexScrolledWindow::VertexScrolledWindow(wxWindow* parent, const wxFont& rowFont, VertexListHeader* header)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxHSCROLL | wxVSCROLL | wxBORDER_NONE | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
        , m_rowFont(rowFont)
        , m_header(header)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetDoubleBuffered(true);
        SetScrollRate(1, 1);

        load_system_colors();
        recalculate_row_height();

        m_scrollStopTimer = new wxTimer(this, wxID_ANY);

        if (m_header != nullptr)
        {
            m_header->set_column_resize_callback([this](const int col, const int width)
            {
                on_column_resize(col, width);
            });
            m_header->set_column_auto_size_callback([this](const int col)
            {
                on_column_auto_size(col);
            });
            m_header->set_column_sort_click_callback([this](const int col)
            {
                on_column_sort_click(col);
            });
        }

        Bind(wxEVT_PAINT, &VertexScrolledWindow::on_paint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &VertexScrolledWindow::on_erase_background, this);
        Bind(wxEVT_SIZE, &VertexScrolledWindow::on_size, this);
        Bind(wxEVT_LEFT_DOWN, &VertexScrolledWindow::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_DCLICK, &VertexScrolledWindow::on_mouse_left_dclick, this);
        Bind(wxEVT_RIGHT_DOWN, &VertexScrolledWindow::on_mouse_right_down, this);
        Bind(wxEVT_MOUSEWHEEL, &VertexScrolledWindow::on_mouse_wheel, this);
        Bind(wxEVT_KEY_DOWN, &VertexScrolledWindow::on_key_down, this);
        Bind(wxEVT_SCROLLWIN_TOP, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_BOTTOM, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEUP, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEDOWN, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEUP, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEDOWN, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBTRACK, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &VertexScrolledWindow::on_scrollwin, this);
        Bind(wxEVT_TIMER, &VertexScrolledWindow::on_scroll_stop_timer, this, m_scrollStopTimer->GetId());
    }

    VertexScrolledWindow::~VertexScrolledWindow()
    {
        if (m_scrollStopTimer != nullptr)
        {
            m_scrollStopTimer->Stop();
            delete m_scrollStopTimer;
        }
    }

    void VertexScrolledWindow::set_columns(std::vector<ColumnDefinition> columns)
    {
        m_columns = std::move(columns);
        if (m_header != nullptr)
        {
            m_header->set_columns(m_columns);
        }
        update_virtual_size();
        Refresh();
    }

    void VertexScrolledWindow::set_column_width(const int columnIndex, const int width)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
        {
            return;
        }
        m_columns[static_cast<std::size_t>(columnIndex)].width = std::max(width,
            m_columns[static_cast<std::size_t>(columnIndex)].minWidth);
        if (m_header != nullptr)
        {
            m_header->set_column_width(columnIndex,
                m_columns[static_cast<std::size_t>(columnIndex)].width);
        }
        update_virtual_size();
        Refresh();
    }

    int VertexScrolledWindow::get_column_count() const noexcept
    {
        return static_cast<int>(m_columns.size());
    }

    const ColumnDefinition& VertexScrolledWindow::get_column(const int columnIndex) const
    {
        return m_columns.at(static_cast<std::size_t>(columnIndex));
    }

    void VertexScrolledWindow::set_row_count(const int count)
    {
        m_rowCount = count < 0 ? 0 : count;
        m_sortFilter.set_model_row_count(m_rowCount);
        invalidate_view();
    }

    int VertexScrolledWindow::get_row_count() const noexcept
    {
        return m_rowCount;
    }

    int VertexScrolledWindow::get_visible_row_count() const noexcept
    {
        return m_sortFilter.view_row_count();
    }

    void VertexScrolledWindow::set_selected_row(const int modelRowIndex)
    {
        if (m_selectedModelRow == modelRowIndex)
        {
            return;
        }
        m_selectedModelRow = modelRowIndex;
        on_selection_changed(modelRowIndex);
        Refresh();
    }

    int VertexScrolledWindow::get_selected_row() const noexcept
    {
        return m_selectedModelRow;
    }

    std::optional<int> VertexScrolledWindow::get_row_at(const wxPoint position) const
    {
        const int unscrolledY = CalcUnscrolledPosition(position).y;
        const int viewIndex = get_view_row_at_y(unscrolledY);
        if (viewIndex < 0)
        {
            return std::nullopt;
        }
        return model_index_for_view(viewIndex);
    }

    std::optional<int> VertexScrolledWindow::get_column_at(const wxPoint position) const
    {
        const int unscrolledX = CalcUnscrolledPosition(position).x;
        const int columnIndex = get_column_at_x(unscrolledX);
        if (columnIndex < 0)
        {
            return std::nullopt;
        }
        return columnIndex;
    }

    void VertexScrolledWindow::ensure_row_visible(const int modelRowIndex)
    {
        const auto viewIndexOpt = view_index_for_model(modelRowIndex);
        if (!viewIndexOpt.has_value())
        {
            return;
        }

        const int y = get_y_for_view_row(viewIndexOpt.value());
        const wxSize clientSize = GetClientSize();
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        if (y < scrollY)
        {
            Scroll(-1, y);
        }
        else if (y + m_rowHeight > scrollY + clientSize.GetHeight())
        {
            Scroll(-1, y + m_rowHeight - clientSize.GetHeight());
        }
    }

    void VertexScrolledWindow::refresh_visible_rows()
    {
        Refresh();
    }

    void VertexScrolledWindow::set_sort(const int columnIndex, const SortDirection direction)
    {
        m_sortColumn = direction == SortDirection::None ? -1 : columnIndex;
        m_sortDirection = direction;
        if (m_header != nullptr)
        {
            m_header->set_sort_indicator(m_sortColumn, m_sortDirection);
        }
        apply_sort_to_model();
        invalidate_view();
    }

    void VertexScrolledWindow::clear_sort()
    {
        set_sort(-1, SortDirection::None);
    }

    int VertexScrolledWindow::get_sort_column() const noexcept
    {
        return m_sortColumn;
    }

    SortDirection VertexScrolledWindow::get_sort_direction() const noexcept
    {
        return m_sortDirection;
    }

    void VertexScrolledWindow::set_filter_text(const wxString& filterText)
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

    void VertexScrolledWindow::clear_filter()
    {
        set_filter_text(wxString{});
    }

    const wxString& VertexScrolledWindow::get_filter_text() const noexcept
    {
        return m_filterText;
    }

    void VertexScrolledWindow::invalidate_view()
    {
        m_sortFilter.rebuild();
        update_virtual_size();
        Refresh();
    }

    void VertexScrolledWindow::draw_row_background(wxDC& dc, const RowContext& context)
    {
        const wxColour background = context.selected
            ? m_colors.selected
            : (context.alternate ? m_colors.backgroundAlt : m_colors.background);
        dc.SetBrush(wxBrush(background));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(context.rowRect);
    }

    void VertexScrolledWindow::on_row_activated([[maybe_unused]] const int modelRowIndex)
    {
    }

    void VertexScrolledWindow::on_row_context_menu([[maybe_unused]] const int modelRowIndex,
                                                   [[maybe_unused]] const wxPoint screenPosition)
    {
    }

    void VertexScrolledWindow::on_selection_changed([[maybe_unused]] const int newModelRowIndex)
    {
    }

    void VertexScrolledWindow::on_scroll_stopped()
    {
    }

    void VertexScrolledWindow::on_cell_clicked([[maybe_unused]] const int modelRowIndex,
                                               [[maybe_unused]] const int columnIndex,
                                               [[maybe_unused]] const wxPoint clientPosition)
    {
    }

    void VertexScrolledWindow::on_cell_activated(const int modelRowIndex,
                                                 [[maybe_unused]] const int columnIndex,
                                                 [[maybe_unused]] const wxPoint clientPosition)
    {
        on_row_activated(modelRowIndex);
    }

    wxRect VertexScrolledWindow::get_cell_rect_for_view(const int viewIndex, const int columnIndex) const noexcept
    {
        return compute_cell_rect(viewIndex, columnIndex);
    }

    int VertexScrolledWindow::measure_cell_width([[maybe_unused]] const int modelRowIndex,
                                                 [[maybe_unused]] const int columnIndex) const
    {
        return 0;
    }

    std::strong_ordering VertexScrolledWindow::compare_rows(const int lhsModelRowIndex,
                                                            const int rhsModelRowIndex,
                                                            const int columnIndex) const
    {
        const wxString lhs = get_filter_text_for_row(lhsModelRowIndex, columnIndex);
        const wxString rhs = get_filter_text_for_row(rhsModelRowIndex, columnIndex);
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

    wxString VertexScrolledWindow::get_filter_text_for_row([[maybe_unused]] const int modelRowIndex,
                                                           [[maybe_unused]] const int columnIndex) const
    {
        return wxString{};
    }

    bool VertexScrolledWindow::passes_filter(const int modelRowIndex,
                                             const wxString& filterTextLower) const
    {
        if (filterTextLower.IsEmpty())
        {
            return true;
        }

        const int columnCount = get_column_count();
        for (int c{}; c < columnCount; ++c)
        {
            if (!m_columns[static_cast<std::size_t>(c)].filterable)
            {
                continue;
            }
            const wxString cellText = get_filter_text_for_row(modelRowIndex, c).Lower();
            if (cellText.Contains(filterTextLower))
            {
                return true;
            }
        }
        return false;
    }

    int VertexScrolledWindow::get_row_height() const noexcept
    {
        return m_rowHeight;
    }

    int VertexScrolledWindow::get_column_padding() const noexcept
    {
        return m_header != nullptr ? m_header->get_column_padding() : 0;
    }

    const wxFont& VertexScrolledWindow::get_row_font() const noexcept
    {
        return m_rowFont;
    }

    VertexListHeader* VertexScrolledWindow::get_header() noexcept
    {
        return m_header;
    }

    const VertexScrolledWindow::Colors& VertexScrolledWindow::get_colors() const noexcept
    {
        return m_colors;
    }

    void VertexScrolledWindow::set_colors(Colors colors)
    {
        m_colors = std::move(colors);
        Refresh();
    }

    int VertexScrolledWindow::model_index_for_view(const int viewIndex) const noexcept
    {
        return m_sortFilter.model_for_view(viewIndex);
    }

    std::optional<int> VertexScrolledWindow::view_index_for_model(const int modelIndex) const noexcept
    {
        return m_sortFilter.view_for_model(modelIndex);
    }

    void VertexScrolledWindow::load_system_colors()
    {
        m_colors.background = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOX);
        m_colors.text = wxSystemSettings::GetColour(wxSYS_COLOUR_LISTBOXTEXT);
        m_colors.selected = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
        m_colors.selectedText = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
        m_colors.separator = wxSystemSettings::GetColour(wxSYS_COLOUR_BTNSHADOW);

        const bool isDark = m_colors.background.GetLuminance() < 0.5;
        m_colors.backgroundAlt = isDark
            ? m_colors.background.ChangeLightness(115)
            : m_colors.background.ChangeLightness(95);
    }

    void VertexScrolledWindow::recalculate_row_height()
    {
        wxClientDC dc(this);
        dc.SetFont(m_rowFont);
        m_rowHeight = dc.GetCharHeight() + FromDIP(4);
    }

    void VertexScrolledWindow::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    void VertexScrolledWindow::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        DoPrepareDC(dc);

        dc.SetFont(m_rowFont);

        const wxSize clientSize = GetClientSize();
        const int clientWidth = clientSize.GetWidth();
        const int clientHeight = clientSize.GetHeight();
        if (clientWidth <= 0 || clientHeight <= 0)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        dc.SetBrush(wxBrush(m_colors.background));
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(scrollX, scrollY, clientWidth, clientHeight);

        const int visibleCount = get_visible_row_count();
        if (visibleCount <= 0 || m_rowHeight <= 0)
        {
            return;
        }

        const int startView = std::max(0, scrollY / m_rowHeight);
        const int endView = std::min(visibleCount,
            ((scrollY + clientHeight) / m_rowHeight) + 1);

        const int padding = get_column_padding();
        const int columnCount = static_cast<int>(m_columns.size());
        const int totalWidth = [&]()
        {
            int total{padding};
            for (const auto& column : m_columns)
            {
                total += column.width + padding;
            }
            return total;
        }();

        for (int viewIndex{startView}; viewIndex < endView; ++viewIndex)
        {
            const int modelIndex = model_index_for_view(viewIndex);
            if (modelIndex < 0)
            {
                continue;
            }

            const int rowY = get_y_for_view_row(viewIndex);
            const wxRect rowRect(0, rowY, totalWidth, m_rowHeight);

            RowContext rowContext{};
            rowContext.modelRowIndex = modelIndex;
            rowContext.viewRowIndex = viewIndex;
            rowContext.rowRect = rowRect;
            rowContext.selected = modelIndex == m_selectedModelRow;
            rowContext.alternate = (viewIndex % 2) == 1;

            draw_row_background(dc, rowContext);

            int cursor{padding};
            for (int c{}; c < columnCount; ++c)
            {
                const auto& column = m_columns[static_cast<std::size_t>(c)];

                CellContext cellContext{};
                cellContext.modelRowIndex = modelIndex;
                cellContext.columnIndex = c;
                cellContext.cellRect = wxRect(cursor, rowY, column.width, m_rowHeight);
                cellContext.alignment = column.alignment;
                cellContext.selected = rowContext.selected;

                draw_cell(dc, cellContext);

                cursor += column.width + padding;
            }
        }

        dc.SetPen(wxPen(m_colors.separator));
        int cursor{padding};
        for (int c{}; c < columnCount - 1; ++c)
        {
            cursor += m_columns[static_cast<std::size_t>(c)].width + padding;
            const int separatorX = cursor - (padding / 2);
            dc.DrawLine(separatorX, scrollY, separatorX, scrollY + clientHeight);
        }
    }

    void VertexScrolledWindow::on_size([[maybe_unused]] wxSizeEvent& event)
    {
        update_virtual_size();
        event.Skip();
    }

    void VertexScrolledWindow::on_mouse_left_down(wxMouseEvent& event)
    {
        SetFocus();
        const auto rowOpt = get_row_at(event.GetPosition());
        if (!rowOpt.has_value())
        {
            m_lastLeftDownRow = -1;
            event.Skip();
            return;
        }

        const int row = rowOpt.value();
        set_selected_row(row);

        const int columnIndex = get_column_at(event.GetPosition()).value_or(-1);

        constexpr auto DCLICK_WINDOW = std::chrono::milliseconds(400);
        const auto now = std::chrono::steady_clock::now();
        const bool isDoubleClick = event.LeftDClick()
            || (row == m_lastLeftDownRow && (now - m_lastLeftDownTime) < DCLICK_WINDOW);

        if (isDoubleClick)
        {
            m_lastLeftDownRow = -1;
            on_cell_activated(row, columnIndex, event.GetPosition());
            return;
        }

        m_lastLeftDownTime = now;
        m_lastLeftDownRow = row;
        on_cell_clicked(row, columnIndex, event.GetPosition());
    }

    void VertexScrolledWindow::on_mouse_left_dclick(wxMouseEvent& event)
    {
        SetFocus();
        const auto rowOpt = get_row_at(event.GetPosition());
        if (rowOpt.has_value())
        {
            const int row = rowOpt.value();
            set_selected_row(row);
            const int columnIndex = get_column_at(event.GetPosition()).value_or(-1);
            on_cell_activated(row, columnIndex, event.GetPosition());
            return;
        }
        event.Skip();
    }

    void VertexScrolledWindow::on_mouse_right_down(wxMouseEvent& event)
    {
        SetFocus();
        const auto rowOpt = get_row_at(event.GetPosition());
        if (rowOpt.has_value())
        {
            set_selected_row(rowOpt.value());
            on_row_context_menu(rowOpt.value(), ClientToScreen(event.GetPosition()));
            return;
        }
        event.Skip();
    }

    void VertexScrolledWindow::on_mouse_wheel(wxMouseEvent& event)
    {
        event.Skip();
    }

    void VertexScrolledWindow::on_key_down(wxKeyEvent& event)
    {
        const int visibleCount = get_visible_row_count();
        if (visibleCount <= 0)
        {
            event.Skip();
            return;
        }

        auto selectedViewOpt = m_selectedModelRow >= 0
            ? view_index_for_model(m_selectedModelRow)
            : std::optional<int>{};
        int viewIndex = selectedViewOpt.value_or(-1);

        const wxSize clientSize = GetClientSize();
        const int pageSize = std::max(1, clientSize.GetHeight() / std::max(1, m_rowHeight));

        bool handled{true};
        switch (event.GetKeyCode())
        {
        case WXK_UP:
            viewIndex = std::max(0, viewIndex - 1);
            break;
        case WXK_DOWN:
            viewIndex = viewIndex < 0 ? 0 : std::min(visibleCount - 1, viewIndex + 1);
            break;
        case WXK_PAGEUP:
            viewIndex = std::max(0, viewIndex - pageSize);
            break;
        case WXK_PAGEDOWN:
            viewIndex = viewIndex < 0 ? 0 : std::min(visibleCount - 1, viewIndex + pageSize);
            break;
        case WXK_HOME:
            viewIndex = 0;
            break;
        case WXK_END:
            viewIndex = visibleCount - 1;
            break;
        case WXK_RETURN:
            if (m_selectedModelRow >= 0)
            {
                on_row_activated(m_selectedModelRow);
            }
            return;
        default:
            handled = false;
            break;
        }

        if (!handled)
        {
            event.Skip();
            return;
        }

        const int modelIndex = model_index_for_view(viewIndex);
        if (modelIndex >= 0)
        {
            set_selected_row(modelIndex);
            ensure_row_visible(modelIndex);
        }
    }

    void VertexScrolledWindow::on_scrollwin(wxScrollWinEvent& event)
    {
        m_isScrolling = true;
        if (m_scrollStopTimer != nullptr)
        {
            m_scrollStopTimer->Start(SCROLL_STOP_DELAY_MS, wxTIMER_ONE_SHOT);
        }
        sync_header_scroll();
        event.Skip();
    }

    void VertexScrolledWindow::on_scroll_stop_timer([[maybe_unused]] wxTimerEvent& event)
    {
        m_isScrolling = false;
        on_scroll_stopped();
    }

    void VertexScrolledWindow::on_column_resize(const int columnIndex, const int newWidth)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
        {
            return;
        }
        m_columns[static_cast<std::size_t>(columnIndex)].width = newWidth;
        update_virtual_size();
        Refresh();
    }

    void VertexScrolledWindow::on_column_auto_size(const int columnIndex)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()))
        {
            return;
        }

        int maxWidth = m_columns[static_cast<std::size_t>(columnIndex)].minWidth;
        {
            wxClientDC dc(this);
            dc.SetFont(m_rowFont);
            const int headerWidth = dc.GetTextExtent(
                m_columns[static_cast<std::size_t>(columnIndex)].title).GetWidth() +
                (get_column_padding() * 2);
            maxWidth = std::max(maxWidth, headerWidth);
        }

        const int visibleCount = get_visible_row_count();
        for (int v{}; v < visibleCount; ++v)
        {
            const int modelIndex = model_index_for_view(v);
            if (modelIndex < 0)
            {
                continue;
            }
            const int width = measure_cell_width(modelIndex, columnIndex);
            if (width > maxWidth)
            {
                maxWidth = width;
            }
        }

        if (maxWidth > 0)
        {
            set_column_width(columnIndex, maxWidth);
        }
    }

    void VertexScrolledWindow::on_column_sort_click(const int columnIndex)
    {
        if (columnIndex < 0 || columnIndex >= static_cast<int>(m_columns.size()) ||
            !m_columns[static_cast<std::size_t>(columnIndex)].sortable)
        {
            return;
        }

        SortDirection nextDirection{SortDirection::Ascending};
        if (m_sortColumn == columnIndex)
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

        set_sort(columnIndex, nextDirection);
    }

    int VertexScrolledWindow::get_y_for_view_row(const int viewIndex) const noexcept
    {
        return viewIndex * m_rowHeight;
    }

    int VertexScrolledWindow::get_view_row_at_y(const int y) const noexcept
    {
        if (m_rowHeight <= 0)
        {
            return -1;
        }
        const int index = y / m_rowHeight;
        if (index < 0 || index >= get_visible_row_count())
        {
            return -1;
        }
        return index;
    }

    int VertexScrolledWindow::get_column_at_x(const int x) const noexcept
    {
        const int padding = get_column_padding();
        int cursor{padding};
        for (int c{}; c < static_cast<int>(m_columns.size()); ++c)
        {
            const int contentStart = cursor;
            const int contentEnd = contentStart + m_columns[static_cast<std::size_t>(c)].width;
            if (x >= contentStart && x < contentEnd)
            {
                return c;
            }
            cursor = contentEnd + padding;
        }
        return -1;
    }

    wxRect VertexScrolledWindow::compute_cell_rect(const int viewIndex, const int columnIndex) const noexcept
    {
        const int padding = get_column_padding();
        int cursor{padding};
        for (int c{}; c < columnIndex; ++c)
        {
            cursor += m_columns[static_cast<std::size_t>(c)].width + padding;
        }
        return wxRect(cursor, get_y_for_view_row(viewIndex),
                      m_columns[static_cast<std::size_t>(columnIndex)].width, m_rowHeight);
    }

    void VertexScrolledWindow::update_virtual_size()
    {
        const int padding = get_column_padding();
        int totalWidth{padding};
        for (const auto& column : m_columns)
        {
            totalWidth += column.width + padding;
        }
        const int totalHeight = get_visible_row_count() * m_rowHeight;
        SetVirtualSize(totalWidth, totalHeight);
    }

    void VertexScrolledWindow::sync_header_scroll() const
    {
        if (m_header == nullptr)
        {
            return;
        }
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);
        m_header->set_horizontal_scroll_offset(scrollX);
    }

    void VertexScrolledWindow::apply_sort_to_model()
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
                return compare_rows(lhs, rhs, sortColumn);
            },
            m_sortDirection
        );
    }

    void VertexScrolledWindow::apply_filter_to_model()
    {
        if (m_filterTextLower.IsEmpty())
        {
            m_sortFilter.clear_filter();
            return;
        }

        m_sortFilter.set_filter(
            [this](const int modelRow) noexcept
            {
                return passes_filter(modelRow, m_filterTextLower);
            }
        );
    }
}
