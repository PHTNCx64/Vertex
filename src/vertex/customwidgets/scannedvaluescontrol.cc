//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/customwidgets/scannedvaluescontrol.hh>
#include <wx/menu.h>
#include <wx/clipbrd.h>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <string_view>

namespace Vertex::CustomWidgets
{
    ScannedValuesHeader::ScannedValuesHeader(
        wxWindow* parent,
        Language::ILanguage& languageService
    )
        : wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxFULL_REPAINT_ON_RESIZE)
    {
        wxWindowBase::SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_codeFont = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        m_codeFont.SetFaceName("Consolas");
        m_codeFontBold = m_codeFont.Bold();

        wxClientDC dc(this);
        dc.SetFont(m_codeFontBold);
        m_charWidth = dc.GetCharWidth();
        m_headerHeight = dc.GetCharHeight() + FromDIP(8);
        m_columnPadding = FromDIP(8);

        m_addressWidth = m_charWidth * 16;
        m_valueWidth = m_charWidth * 24;
        m_firstValueWidth = m_charWidth * 24;
        m_previousValueWidth = m_charWidth * 24;

        m_headerAddress = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.scannedColumnAddress"));
        m_headerValue = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.scannedColumnCurrentValue"));
        m_headerFirstValue = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.scannedColumnFirstValue"));
        m_headerPreviousValue = wxString::FromUTF8(languageService.fetch_translation("mainWindow.ui.scannedColumnPreviousValue"));

        wxWindowBase::SetMinSize(wxSize(-1, m_headerHeight));
        wxWindowBase::SetMaxSize(wxSize(-1, m_headerHeight));

        Bind(wxEVT_PAINT, &ScannedValuesHeader::on_paint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &ScannedValuesHeader::on_erase_background, this);
        Bind(wxEVT_MOTION, &ScannedValuesHeader::on_mouse_motion, this);
        Bind(wxEVT_LEFT_DOWN, &ScannedValuesHeader::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_UP, &ScannedValuesHeader::on_mouse_left_up, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &ScannedValuesHeader::on_mouse_capture_lost, this);
        Bind(wxEVT_LEAVE_WINDOW, &ScannedValuesHeader::on_mouse_leave, this);
    }

    void ScannedValuesHeader::set_horizontal_scroll_offset(const int offset)
    {
        if (m_hScrollOffset != offset)
        {
            m_hScrollOffset = offset;
            Refresh(false);
        }
    }

    void ScannedValuesHeader::set_column_resize_callback(ColumnResizeCallback callback)
    {
        m_columnResizeCallback = std::move(callback);
    }

    int ScannedValuesHeader::get_separator_x(const int separatorIndex) const
    {
        int x = m_columnPadding - m_hScrollOffset;

        switch (separatorIndex)
        {
            case 0:
                x += m_addressWidth + m_columnPadding / 2;
                break;
            case 1:
                x += m_addressWidth + m_columnPadding + m_valueWidth + m_columnPadding / 2;
                break;
            case 2:
                x += m_addressWidth + m_columnPadding + m_valueWidth + m_columnPadding +
                     m_firstValueWidth + m_columnPadding / 2;
                break;
            default:
                return -1;
        }
        return x;
    }

    int ScannedValuesHeader::get_separator_at_x(const int x) const
    {
        for (int i = 0; i < 3; ++i)
        {
            const int sepX = get_separator_x(i);
            if (std::abs(x - sepX) <= SEPARATOR_HIT_TOLERANCE)
            {
                return i;
            }
        }
        return -1;
    }

    void ScannedValuesHeader::on_mouse_motion(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();

        if (m_resizingColumn >= 0)
        {
            const int delta = mouseX - m_resizeStartX;
            const int newWidth = std::max(MIN_COLUMN_WIDTH, m_resizeStartWidth + delta);

            switch (m_resizingColumn)
            {
                case 0: m_addressWidth = newWidth; break;
                case 1: m_valueWidth = newWidth; break;
                case 2: m_firstValueWidth = newWidth; break;
                default: break;
            }

            Refresh(false);

            if (m_columnResizeCallback)
            {
                m_columnResizeCallback();
            }
        }
        else
        {
            const int sep = get_separator_at_x(mouseX);
            if (sep >= 0)
            {
                SetCursor(wxCursor(wxCURSOR_SIZEWE));
            }
            else
            {
                SetCursor(wxNullCursor);
            }
        }

        event.Skip();
    }

    void ScannedValuesHeader::on_mouse_left_down(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();
        const int sep = get_separator_at_x(mouseX);

        if (sep >= 0)
        {
            m_resizingColumn = sep;
            m_resizeStartX = mouseX;

            switch (sep)
            {
                case 0: m_resizeStartWidth = m_addressWidth; break;
                case 1: m_resizeStartWidth = m_valueWidth; break;
                case 2: m_resizeStartWidth = m_firstValueWidth; break;
                default: break;
            }

            CaptureMouse();
        }

        event.Skip();
    }

    void ScannedValuesHeader::on_mouse_left_up([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_resizingColumn >= 0)
        {
            m_resizingColumn = -1;

            if (HasCapture())
            {
                ReleaseMouse();
            }

            Refresh(false);
            if (m_columnResizeCallback)
            {
                m_columnResizeCallback();
            }
        }

        event.Skip();
    }

    void ScannedValuesHeader::on_mouse_capture_lost([[maybe_unused]] wxMouseCaptureLostEvent& event)
    {
        m_resizingColumn = -1;
        SetCursor(wxNullCursor);
    }

    void ScannedValuesHeader::on_mouse_leave([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_resizingColumn < 0)
        {
            SetCursor(wxNullCursor);
        }
        event.Skip();
    }

    void ScannedValuesHeader::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxBufferedPaintDC dc(this);
        const wxSize size = GetClientSize();

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.headerBackground));
        dc.DrawRectangle(0, 0, size.GetWidth(), size.GetHeight());

        dc.SetPen(wxPen(m_colors.headerBorder, 1));
        dc.DrawLine(0, size.GetHeight() - 1, size.GetWidth(), size.GetHeight() - 1);

        dc.SetFont(m_codeFontBold);
        dc.SetTextForeground(m_colors.headerText);

        int x = m_columnPadding - m_hScrollOffset;
        const int y = (m_headerHeight - dc.GetCharHeight()) / 2;

        dc.DrawText(m_headerAddress, x, y);
        x += m_addressWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 0 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerValue, x, y);
        x += m_valueWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 1 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerFirstValue, x, y);
        x += m_firstValueWidth + m_columnPadding;

        dc.SetPen(wxPen(m_resizingColumn == 2 ? m_colors.separatorHover : m_colors.headerBorder, 1));
        dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);

        dc.DrawText(m_headerPreviousValue, x, y);
    }

    void ScannedValuesHeader::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    ScannedValuesControl::ScannedValuesControl(
        wxWindow* parent,
        Language::ILanguage& languageService,
        const std::shared_ptr<ViewModel::MainViewModel>& viewModel,
        ScannedValuesHeader* header
    )
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxHSCROLL | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
        , m_languageService(languageService)
        , m_viewModel(viewModel)
        , m_header(header)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_codeFont = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        m_codeFont.SetFaceName("Consolas");

        wxClientDC dc(this);
        dc.SetFont(m_codeFont);
        m_lineHeight = dc.GetCharHeight() + FromDIP(4);

        m_refreshTimer = new wxTimer(this, wxID_ANY);
        m_scrollStopTimer = new wxTimer(this, wxID_ANY + 1);

        Bind(wxEVT_PAINT, &ScannedValuesControl::on_paint, this);
        Bind(wxEVT_SIZE, &ScannedValuesControl::on_size, this);
        Bind(wxEVT_LEFT_DOWN, &ScannedValuesControl::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_DCLICK, &ScannedValuesControl::on_mouse_left_dclick, this);
        Bind(wxEVT_RIGHT_DOWN, &ScannedValuesControl::on_mouse_right_down, this);
        Bind(wxEVT_MOUSEWHEEL, &ScannedValuesControl::on_mouse_wheel, this);
        Bind(wxEVT_KEY_DOWN, &ScannedValuesControl::on_key_down, this);
        Bind(wxEVT_ERASE_BACKGROUND, &ScannedValuesControl::on_erase_background, this);

        Bind(wxEVT_SCROLLWIN_TOP, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_BOTTOM, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEUP, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_LINEDOWN, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEUP, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_PAGEDOWN, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBTRACK, &ScannedValuesControl::on_scrollwin, this);
        Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &ScannedValuesControl::on_scrollwin, this);

        Bind(wxEVT_TIMER, &ScannedValuesControl::on_refresh_timer, this, m_refreshTimer->GetId());
        Bind(wxEVT_TIMER, &ScannedValuesControl::on_scroll_timer, this, m_scrollStopTimer->GetId());

        SetScrollRate(m_header->get_char_width(), m_lineHeight);
    }

    ScannedValuesControl::~ScannedValuesControl()
    {
        if (m_refreshTimer)
        {
            m_refreshTimer->Stop();
            delete m_refreshTimer;
        }
        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Stop();
            delete m_scrollStopTimer;
        }
    }

    void ScannedValuesControl::refresh_list()
    {
        m_itemCount = static_cast<int>(std::min(
            m_viewModel->get_scanned_values_count(),
            static_cast<std::int64_t>(MAX_DISPLAYED_ITEMS)));

        Scroll(0, 0);
        sync_header_scroll();
        m_selectedLine = -1;

        update_virtual_size();

        if (m_itemCount > 0)
        {
            const int clientHeight = GetClientSize().GetHeight();
            const int visibleCount = (clientHeight / m_lineHeight) + 2;
            const int endLine = std::min(visibleCount, m_itemCount);
            m_viewModel->update_cache_window(0, endLine);
        }

        Refresh(false);
    }

    void ScannedValuesControl::clear_list()
    {
        stop_auto_refresh();
        m_itemCount = 0;
        m_selectedLine = -1;
        update_virtual_size();
        Refresh(false);
    }

    void ScannedValuesControl::start_auto_refresh() const
    {
        if (m_refreshTimer && !m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Start(100);
        }
    }

    void ScannedValuesControl::stop_auto_refresh() const
    {
        if (m_refreshTimer && m_refreshTimer->IsRunning())
        {
            m_refreshTimer->Stop();
        }
    }

    void ScannedValuesControl::set_selection_change_callback(SelectionChangeCallback callback)
    {
        m_selectionChangeCallback = std::move(callback);
    }

    void ScannedValuesControl::set_add_to_table_callback(AddToTableCallback callback)
    {
        m_addToTableCallback = std::move(callback);
    }

    int ScannedValuesControl::get_selected_index() const
    {
        return m_selectedLine;
    }

    std::optional<std::uint64_t> ScannedValuesControl::get_selected_address() const
    {
        if (m_selectedLine >= 0 && m_selectedLine < m_itemCount)
        {
            const auto value = m_viewModel->get_scanned_value_at(m_selectedLine);
            if (!value.address.empty())
            {
                try
                {
                    return std::stoull(value.address, nullptr, 16);
                }
                catch (...)
                {
                    return std::nullopt;
                }
            }
        }
        return std::nullopt;
    }

    void ScannedValuesControl::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        DoPrepareDC(dc);
        render(dc);
    }

    void ScannedValuesControl::on_size([[maybe_unused]] wxSizeEvent& event)
    {
        update_virtual_size();
        Refresh(false);
        event.Skip();
    }

    void ScannedValuesControl::on_mouse_left_down(const wxMouseEvent& event)
    {
        SetFocus();

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            m_selectedLine = lineIndex;

            if (m_selectionChangeCallback)
            {
                const auto address = get_selected_address();
                m_selectionChangeCallback(m_selectedLine, address.value_or(0));
            }

            Refresh(false);
        }
    }

    void ScannedValuesControl::on_mouse_left_dclick(wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            if (m_addToTableCallback)
            {
                const auto value = m_viewModel->get_scanned_value_at(lineIndex);
                if (!value.address.empty())
                {
                    try
                    {
                        const auto address = std::stoull(value.address, nullptr, 16);
                        m_addToTableCallback(lineIndex, address);
                    }
                    catch (...)
                    {
                    }
                }
            }
        }
    }

    void ScannedValuesControl::on_mouse_right_down(wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && lineIndex < m_itemCount)
        {
            m_selectedLine = lineIndex;
            Refresh(false);

            const auto scannedValue = m_viewModel->get_scanned_value_at(m_selectedLine);

            wxMenu menu;
            menu.Append(1001, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.addToTable")));
            menu.AppendSeparator();
            menu.Append(1002, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAddress")));
            menu.Append(1003, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyValue")));
            menu.Append(1004, wxString::FromUTF8(m_languageService.fetch_translation("mainWindow.context.copyAll")));

            const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
            switch (selection)
            {
                case 1001:
                    if (m_addToTableCallback && !scannedValue.address.empty())
                    {
                        try
                        {
                            const auto address = std::stoull(scannedValue.address, nullptr, 16);
                            m_addToTableCallback(m_selectedLine, address);
                        }
                        catch (...)
                        {
                        }
                    }
                    break;

                case 1002:
                    if (wxTheClipboard->Open())
                    {
                        wxTheClipboard->SetData(new wxTextDataObject(scannedValue.address));
                        wxTheClipboard->Close();
                    }
                    break;

                case 1003:
                    if (wxTheClipboard->Open())
                    {
                        wxTheClipboard->SetData(new wxTextDataObject(scannedValue.value));
                        wxTheClipboard->Close();
                    }
                    break;

                case 1004:
                    if (wxTheClipboard->Open())
                    {
                        const auto fullLine = fmt::format("{}\t{}\t{}\t{}",
                            scannedValue.address, scannedValue.value,
                            scannedValue.firstValue, scannedValue.previousValue);
                        wxTheClipboard->SetData(new wxTextDataObject(fullLine));
                        wxTheClipboard->Close();
                    }
                    break;

                default:
                    break;
            }
        }
    }

    void ScannedValuesControl::on_mouse_wheel(wxMouseEvent& event)
    {
        const int rotation = event.GetWheelRotation();
        const int delta = event.GetWheelDelta();
        const int lines = rotation / delta * 3;

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);
        Scroll(scrollX, scrollY - lines);

        sync_header_scroll();
    }

    void ScannedValuesControl::on_key_down(wxKeyEvent& event)
    {
        const int keyCode = event.GetKeyCode();

        switch (keyCode)
        {
            case WXK_UP:
                if (m_selectedLine > 0)
                {
                    --m_selectedLine;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                else if (m_selectedLine == -1 && m_itemCount > 0)
                {
                    m_selectedLine = 0;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                break;

            case WXK_DOWN:
                if (m_selectedLine + 1 < m_itemCount)
                {
                    ++m_selectedLine;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                else if (m_selectedLine == -1 && m_itemCount > 0)
                {
                    m_selectedLine = 0;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                break;

            case WXK_PAGEUP:
            {
                const int visibleLines = get_visible_line_count();
                if (m_selectedLine >= visibleLines)
                {
                    m_selectedLine -= visibleLines;
                }
                else
                {
                    m_selectedLine = 0;
                }
                ensure_line_visible(m_selectedLine);
                if (m_selectionChangeCallback)
                {
                    const auto address = get_selected_address();
                    m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                }
                break;
            }

            case WXK_PAGEDOWN:
            {
                const int visibleLines = get_visible_line_count();
                m_selectedLine += visibleLines;
                if (m_selectedLine >= m_itemCount)
                {
                    m_selectedLine = m_itemCount - 1;
                }
                if (m_selectedLine < 0)
                {
                    m_selectedLine = 0;
                }
                ensure_line_visible(m_selectedLine);
                if (m_selectionChangeCallback)
                {
                    const auto address = get_selected_address();
                    m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                }
                break;
            }

            case WXK_HOME:
                if (event.ControlDown() && m_itemCount > 0)
                {
                    m_selectedLine = 0;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                break;

            case WXK_END:
                if (event.ControlDown() && m_itemCount > 0)
                {
                    m_selectedLine = m_itemCount - 1;
                    ensure_line_visible(m_selectedLine);
                    if (m_selectionChangeCallback)
                    {
                        const auto address = get_selected_address();
                        m_selectionChangeCallback(m_selectedLine, address.value_or(0));
                    }
                }
                break;

            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:
                if (m_selectedLine >= 0 && m_selectedLine < m_itemCount && m_addToTableCallback)
                {
                    const auto address = get_selected_address();
                    if (address.has_value())
                    {
                        m_addToTableCallback(m_selectedLine, address.value());
                    }
                }
                break;

            default:
                event.Skip();
                return;
        }

        Refresh(false);
    }

    void ScannedValuesControl::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    void ScannedValuesControl::on_scrollwin(wxScrollWinEvent& event)
    {
        m_isScrolling = true;

        sync_header_scroll();

        if (m_scrollStopTimer)
        {
            m_scrollStopTimer->Start(50, wxTIMER_ONE_SHOT);
        }

        event.Skip();
    }

    void ScannedValuesControl::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (!m_isScrolling)
        {
            refresh_visible_items();
        }
    }

    void ScannedValuesControl::on_scroll_timer([[maybe_unused]] wxTimerEvent& event)
    {
        m_isScrolling = false;
        refresh_visible_items();
    }

    void ScannedValuesControl::sync_header_scroll() const
    {
        if (m_header)
        {
            int scrollX{};
            int scrollY{};
            GetViewStart(&scrollX, &scrollY);
            m_header->set_horizontal_scroll_offset(scrollX * m_header->get_char_width());
        }
    }

    void ScannedValuesControl::refresh_visible_items()
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int clientHeight = GetClientSize().GetHeight();
        const int startLine = scrollY;
        const int visibleCount = (clientHeight / m_lineHeight) + 2;
        const int endLine = std::min(startLine + visibleCount, m_itemCount);

        if (startLine < 0 || visibleCount <= 0)
        {
            return;
        }

        m_viewModel->update_cache_window(startLine, endLine);
        m_viewModel->refresh_visible_range(startLine, endLine);

        Refresh(false);
    }

    void ScannedValuesControl::render(wxDC& dc)
    {
        render_background(dc);

        if (m_itemCount == 0)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int clientHeight = GetClientSize().GetHeight();
        const int startLine = scrollY;
        const int endLine = std::min(
            startLine + (clientHeight / m_lineHeight) + 2,
            m_itemCount
        );

        render_lines(dc, startLine, endLine);
    }

    void ScannedValuesControl::render_background(wxDC& dc) const
    {
        dc.SetBackground(wxBrush(m_colors.background));
        dc.Clear();
    }

    void ScannedValuesControl::render_lines(wxDC& dc, const int startLine, const int endLine) const
    {
        dc.SetFont(m_codeFont);

        for (int i = startLine; i < endLine; ++i)
        {
            const int y = get_y_for_line(i);
            render_line(dc, i, y);
        }
    }

    void ScannedValuesControl::render_line(wxDC& dc, const int lineIndex, const int y) const
    {
        const auto [address, value, firstValue, previousValue] = m_viewModel->get_scanned_value_at(lineIndex);

        const int addressWidth = m_header->get_address_width();
        const int valueWidth = m_header->get_value_width();
        const int firstValueWidth = m_header->get_first_value_width();
        const int previousValueWidth = m_header->get_previous_value_width();
        const int padding = m_header->get_column_padding();

        const int totalWidth = addressWidth + valueWidth + firstValueWidth + previousValueWidth + padding * 5;

        wxColour bgColor{};
        const bool isSelected = (lineIndex == m_selectedLine);

        if (isSelected)
        {
            bgColor = m_colors.selectedLine;
        }
        else if (lineIndex % 2 == 1)
        {
            bgColor = m_colors.backgroundAlt;
        }
        else
        {
            bgColor = m_colors.background;
        }

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(bgColor));
        dc.DrawRectangle(0, y, std::max(totalWidth, GetVirtualSize().GetWidth()), m_lineHeight);

        int x = padding;

        dc.SetTextForeground(m_colors.address);
        std::string_view addressView = address;

        if (addressView.size() >= 2 && addressView[0] == '0' &&
            (addressView[1] == 'x' || addressView[1] == 'X'))
        {
            addressView.remove_prefix(2);
        }

        std::array<char, 17> formattedAddress{};
        const std::size_t padCount = (addressView.size() < 16) ? (16 - addressView.size()) : 0;
        std::memset(formattedAddress.data(), '0', padCount);
        std::memcpy(formattedAddress.data() + padCount, addressView.data(),
                    std::min(addressView.size(), std::size_t{16}));
        formattedAddress[16] = '\0';

        dc.DrawText(formattedAddress.data(), x, y + (m_lineHeight - dc.GetCharHeight()) / 2);
        x += addressWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        const bool valueChanged = !previousValue.empty() &&
                                  value != previousValue;
        dc.SetTextForeground(valueChanged ? m_colors.changedValue : m_colors.value);
        dc.DrawText(value, x, y + (m_lineHeight - dc.GetCharHeight()) / 2);
        x += valueWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        dc.SetTextForeground(m_colors.firstValue);
        dc.DrawText(firstValue, x, y + (m_lineHeight - dc.GetCharHeight()) / 2);
        x += firstValueWidth + padding;

        dc.SetPen(wxPen(m_colors.separator, 1));
        dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);

        dc.SetTextForeground(m_colors.previousValue);
        dc.DrawText(previousValue, x, y + (m_lineHeight - dc.GetCharHeight()) / 2);
    }

    int ScannedValuesControl::get_line_at_y(const int y) const
    {
        return y / m_lineHeight;
    }

    int ScannedValuesControl::get_y_for_line(const int lineIndex) const
    {
        return lineIndex * m_lineHeight;
    }

    int ScannedValuesControl::get_visible_line_count() const
    {
        return GetClientSize().GetHeight() / m_lineHeight;
    }

    void ScannedValuesControl::update_virtual_size()
    {
        const int totalHeight = m_itemCount * m_lineHeight;

        const int addressWidth = m_header->get_address_width();
        const int valueWidth = m_header->get_value_width();
        const int firstValueWidth = m_header->get_first_value_width();
        const int previousValueWidth = m_header->get_previous_value_width();
        const int padding = m_header->get_column_padding();

        const int totalWidth = addressWidth + valueWidth + firstValueWidth + previousValueWidth + padding * 5;

        SetVirtualSize(totalWidth, totalHeight);
    }

    void ScannedValuesControl::ensure_line_visible(const int lineIndex)
    {
        if (lineIndex < 0 || lineIndex >= m_itemCount)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int visibleLines = get_visible_line_count();

        if (lineIndex < scrollY)
        {
            Scroll(scrollX, lineIndex);
        }
        else if (lineIndex >= scrollY + visibleLines)
        {
            Scroll(scrollX, lineIndex - visibleLines + 1);
        }

        sync_header_scroll();
    }

    void ScannedValuesControl::on_columns_resized()
    {
        update_virtual_size();
        Refresh(false);
    }
}
