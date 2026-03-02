//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/disassemblycontrol.hh>
#include <vertex/utility.hh>
#include <wx/menu.h>
#include <wx/clipbrd.h>
#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/button.h>
#include <fmt/format.h>
#include <algorithm>
#include <array>
#include <ranges>
#include <utility>

namespace Vertex::View::Debugger
{
    DisassemblyHeader::DisassemblyHeader(
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

        m_columnWidths[static_cast<int>(DisassemblyColumn::Address)] = m_charWidth * 18;
        m_columnWidths[static_cast<int>(DisassemblyColumn::Bytes)] = m_charWidth * 24;
        m_columnWidths[static_cast<int>(DisassemblyColumn::Mnemonic)] = m_charWidth * 10;
        m_columnWidths[static_cast<int>(DisassemblyColumn::Operands)] = m_charWidth * 40;
        m_columnWidths[static_cast<int>(DisassemblyColumn::Comment)] = m_charWidth * 30;

        m_columnOrder[0] = DisassemblyColumn::Address;
        m_columnOrder[1] = DisassemblyColumn::Bytes;
        m_columnOrder[2] = DisassemblyColumn::Mnemonic;
        m_columnOrder[3] = DisassemblyColumn::Operands;
        m_columnOrder[4] = DisassemblyColumn::Comment;

        m_headerAddress = wxString::FromUTF8(languageService.fetch_translation("debugger.disassembly.columnAddress"));
        m_headerBytes = wxString::FromUTF8(languageService.fetch_translation("debugger.disassembly.columnBytes"));
        m_headerMnemonic = wxString::FromUTF8(languageService.fetch_translation("debugger.disassembly.columnMnemonic"));
        m_headerOperands = wxString::FromUTF8(languageService.fetch_translation("debugger.disassembly.columnOperands"));
        m_headerComment = wxString::FromUTF8(languageService.fetch_translation("debugger.disassembly.columnComment"));

        if (m_headerAddress.empty())
        {
            m_headerAddress = "Address";
        }
        if (m_headerBytes.empty())
        {
            m_headerBytes = "Bytes";
        }
        if (m_headerMnemonic.empty())
        {
            m_headerMnemonic = "Mnemonic";
        }
        if (m_headerOperands.empty())
        {
            m_headerOperands = "Operands";
        }
        if (m_headerComment.empty())
        {
            m_headerComment = "Comment";
        }

        wxWindowBase::SetMinSize(wxSize(-1, m_headerHeight));
        wxWindowBase::SetMaxSize(wxSize(-1, m_headerHeight));

        Bind(wxEVT_PAINT, &DisassemblyHeader::on_paint, this);
        Bind(wxEVT_ERASE_BACKGROUND, &DisassemblyHeader::on_erase_background, this);
        Bind(wxEVT_MOTION, &DisassemblyHeader::on_mouse_motion, this);
        Bind(wxEVT_LEFT_DOWN, &DisassemblyHeader::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_UP, &DisassemblyHeader::on_mouse_left_up, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &DisassemblyHeader::on_mouse_capture_lost, this);
        Bind(wxEVT_LEAVE_WINDOW, &DisassemblyHeader::on_mouse_leave, this);
    }

    void DisassemblyHeader::set_horizontal_scroll_offset(int offset)
    {
        if (m_hScrollOffset != offset)
        {
            m_hScrollOffset = offset;
            Refresh(false);
        }
    }

    void DisassemblyHeader::set_column_resize_callback(ColumnResizeCallback callback)
    {
        m_columnResizeCallback = std::move(callback);
    }

    void DisassemblyHeader::set_column_reorder_callback(ColumnReorderCallback callback)
    {
        m_columnReorderCallback = std::move(callback);
    }

    void DisassemblyHeader::set_left_offset(const int offset)
    {
        if (m_leftOffset != offset)
        {
            m_leftOffset = offset;
            Refresh(false);
        }
    }

    int DisassemblyHeader::get_column_width(DisassemblyColumn column) const
    {
        return m_columnWidths[static_cast<int>(column)];
    }

    void DisassemblyHeader::set_column_width(DisassemblyColumn column, int width)
    {
        m_columnWidths[static_cast<int>(column)] = std::max(MIN_COLUMN_WIDTH, width);
    }

    void DisassemblyHeader::set_column_order(const std::array<DisassemblyColumn, COLUMN_COUNT>& order)
    {
        m_columnOrder = order;
    }

    int DisassemblyHeader::get_total_width() const
    {
        int total = m_leftOffset + m_columnPadding;
        for (int i = 0; i < COLUMN_COUNT; ++i)
        {
            total += m_columnWidths[static_cast<int>(m_columnOrder[i])] + m_columnPadding;
        }
        return total;
    }

    int DisassemblyHeader::get_column_start_x(int visualIndex) const
    {
        int x = m_leftOffset + m_columnPadding;
        for (int i = 0; i < visualIndex && i < COLUMN_COUNT; ++i)
        {
            x += m_columnWidths[static_cast<int>(m_columnOrder[i])] + m_columnPadding;
        }
        return x;
    }

    int DisassemblyHeader::get_separator_x(const int separatorIndex) const
    {
        if (separatorIndex < 0 || separatorIndex >= COLUMN_COUNT - 1)
        {
            return -1;
        }

        int x = m_leftOffset + m_columnPadding - m_hScrollOffset;
        for (int i = 0; i <= separatorIndex; ++i)
        {
            x += m_columnWidths[static_cast<int>(m_columnOrder[i])] + m_columnPadding;
        }
        return x - m_columnPadding / 2;
    }

    int DisassemblyHeader::get_separator_at_x(int x) const
    {
        for (int i = 0; i < COLUMN_COUNT - 1; ++i)
        {
            const int sepX = get_separator_x(i);
            if (sepX >= 0 && std::abs(x - sepX) <= SEPARATOR_HIT_TOLERANCE)
            {
                return i;
            }
        }
        return -1;
    }

    int DisassemblyHeader::get_column_at_x(int x) const
    {
        int colStart = m_leftOffset + m_columnPadding - m_hScrollOffset;
        for (int i = 0; i < COLUMN_COUNT; ++i)
        {
            const int colWidth = m_columnWidths[static_cast<int>(m_columnOrder[i])];
            if (x >= colStart && x < colStart + colWidth)
            {
                return i;
            }
            colStart += colWidth + m_columnPadding;
        }
        return -1;
    }

    const wxString& DisassemblyHeader::get_column_header(DisassemblyColumn column) const
    {
        switch (column)
        {
            case DisassemblyColumn::Address: return m_headerAddress;
            case DisassemblyColumn::Bytes: return m_headerBytes;
            case DisassemblyColumn::Mnemonic: return m_headerMnemonic;
            case DisassemblyColumn::Operands: return m_headerOperands;
            case DisassemblyColumn::Comment: return m_headerComment;
            case DisassemblyColumn::COUNT: break;
        }
        std::unreachable();
    }

    void DisassemblyHeader::on_mouse_motion(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();

        if (m_dragging)
        {
            m_dragCurrentX = mouseX;

            m_dragTargetIndex = get_column_at_x(mouseX);
            if (m_dragTargetIndex < 0)
            {
                m_dragTargetIndex = (mouseX < m_leftOffset + m_columnPadding - m_hScrollOffset) ? 0 : COLUMN_COUNT - 1;
            }

            Refresh(false);
        }
        else if (m_resizingColumn >= 0)
        {
            const int delta = mouseX - m_resizeStartX;
            const int newWidth = std::max(MIN_COLUMN_WIDTH, m_resizeStartWidth + delta);

            m_columnWidths[static_cast<int>(m_columnOrder[m_resizingColumn])] = newWidth;

            Refresh(false);

            if (m_columnResizeCallback)
            {
                m_columnResizeCallback();
            }
        }
        else if (m_dragSourceIndex >= 0)
        {
            const int dragDistance = std::abs(mouseX - m_dragStartX);
            if (dragDistance >= DRAG_THRESHOLD)
            {
                m_dragging = true;
                m_dragCurrentX = mouseX;
                m_dragTargetIndex = get_column_at_x(mouseX);
                CaptureMouse();
                Refresh(false);
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

    void DisassemblyHeader::on_mouse_left_down(wxMouseEvent& event)
    {
        const int mouseX = event.GetX();

        const int sep = get_separator_at_x(mouseX);
        if (sep >= 0)
        {
            m_resizingColumn = sep;
            m_resizeStartX = mouseX;
            m_resizeStartWidth = m_columnWidths[static_cast<int>(m_columnOrder[sep])];
            CaptureMouse();
        }
        else
        {
            const int col = get_column_at_x(mouseX);
            if (col >= 0)
            {
                m_dragSourceIndex = col;
                m_dragStartX = mouseX;
                m_dragCurrentX = mouseX;
            }
        }

        event.Skip();
    }

    void DisassemblyHeader::on_mouse_left_up([[maybe_unused]] wxMouseEvent& event)
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

        if (m_dragging)
        {
            if (m_dragTargetIndex >= 0 && m_dragTargetIndex != m_dragSourceIndex)
            {
                const DisassemblyColumn draggedCol = m_columnOrder[m_dragSourceIndex];

                if (m_dragTargetIndex > m_dragSourceIndex)
                {
                    for (int i = m_dragSourceIndex; i < m_dragTargetIndex; ++i)
                    {
                        m_columnOrder[i] = m_columnOrder[i + 1];
                    }
                }
                else
                {
                    for (int i = m_dragSourceIndex; i > m_dragTargetIndex; --i)
                    {
                        m_columnOrder[i] = m_columnOrder[i - 1];
                    }
                }
                m_columnOrder[m_dragTargetIndex] = draggedCol;

                if (m_columnReorderCallback)
                {
                    m_columnReorderCallback();
                }
            }

            if (HasCapture())
            {
                ReleaseMouse();
            }
        }

        m_dragging = false;
        m_dragSourceIndex = -1;
        m_dragTargetIndex = -1;
        Refresh(false);

        event.Skip();
    }

    void DisassemblyHeader::on_mouse_capture_lost([[maybe_unused]] wxMouseCaptureLostEvent& event)
    {
        m_resizingColumn = -1;
        m_dragging = false;
        m_dragSourceIndex = -1;
        m_dragTargetIndex = -1;
        SetCursor(wxNullCursor);
    }

    void DisassemblyHeader::on_mouse_leave([[maybe_unused]] wxMouseEvent& event)
    {
        if (m_resizingColumn < 0 && !m_dragging)
        {
            SetCursor(wxNullCursor);
            m_dragSourceIndex = -1;
        }
        event.Skip();
    }

    void DisassemblyHeader::draw_drag_indicator(wxDC& dc, int x)
    {
        dc.SetPen(wxPen(m_colors.dragIndicator, 2));
        dc.DrawLine(x, 0, x, m_headerHeight);

        const int triSize = FromDIP(4);
        const std::array<wxPoint, 3> topTri = {{
            {x, 0},
            {x - triSize, triSize},
            {x + triSize, triSize}
        }};
        const std::array<wxPoint, 3> botTri = {{
            {x, m_headerHeight},
            {x - triSize, m_headerHeight - triSize},
            {x + triSize, m_headerHeight - triSize}
        }};

        dc.SetBrush(wxBrush(m_colors.dragIndicator));
        dc.DrawPolygon(static_cast<int>(topTri.size()), topTri.data());
        dc.DrawPolygon(static_cast<int>(botTri.size()), botTri.data());
    }

    void DisassemblyHeader::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxBufferedPaintDC dc(this);
        const wxSize size = GetClientSize();

        if (m_leftOffset > 0)
        {
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.SetBrush(wxBrush(wxColour(0x2D, 0x2D, 0x2D)));
            dc.DrawRectangle(0, 0, m_leftOffset, size.GetHeight());

            dc.SetPen(wxPen(wxColour(0x3E, 0x3E, 0x3E), 1));
            dc.DrawLine(m_leftOffset, 0, m_leftOffset, size.GetHeight());
        }

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.headerBackground));
        dc.DrawRectangle(m_leftOffset, 0, size.GetWidth() - m_leftOffset, size.GetHeight());

        dc.SetPen(wxPen(m_colors.headerBorder, 1));
        dc.DrawLine(0, size.GetHeight() - 1, size.GetWidth(), size.GetHeight() - 1);

        dc.SetFont(m_codeFontBold);
        dc.SetTextForeground(m_colors.headerText);

        int x = m_leftOffset + m_columnPadding - m_hScrollOffset;
        const int y = (m_headerHeight - dc.GetCharHeight()) / 2;

        for (int i = 0; i < COLUMN_COUNT; ++i)
        {
            const DisassemblyColumn col = m_columnOrder[i];
            const int colWidth = m_columnWidths[static_cast<int>(col)];

            if (m_dragging && i == m_dragSourceIndex)
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(m_colors.draggedColumn));
                dc.DrawRectangle(x - m_columnPadding / 2, 0, colWidth + m_columnPadding, m_headerHeight);
            }

            dc.SetTextForeground(m_colors.headerText);
            dc.DrawText(get_column_header(col), x, y);

            x += colWidth + m_columnPadding;

            if (i < COLUMN_COUNT - 1)
            {
                const bool isResizing = (m_resizingColumn == i);
                dc.SetPen(wxPen(isResizing ? m_colors.separatorHover : m_colors.headerBorder, 1));
                dc.DrawLine(x - m_columnPadding / 2, 2, x - m_columnPadding / 2, m_headerHeight - 2);
            }
        }

        if (m_dragging && m_dragTargetIndex >= 0)
        {
            int indicatorX{};
            if (m_dragTargetIndex <= m_dragSourceIndex)
            {
                indicatorX = get_column_start_x(m_dragTargetIndex) - m_hScrollOffset;
            }
            else
            {
                indicatorX = get_column_start_x(m_dragTargetIndex + 1) - m_hScrollOffset - m_columnPadding / 2;
            }
            draw_drag_indicator(dc, indicatorX);
        }
    }

    void DisassemblyHeader::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    DisassemblyControl::DisassemblyControl(wxWindow* parent, Language::ILanguage& languageService, DisassemblyHeader* header)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                           wxVSCROLL | wxHSCROLL | wxFULL_REPAINT_ON_RESIZE | wxWANTS_CHARS)
        , m_loadingAnimTimer(this)
        , m_header(header)
        , m_languageService(languageService)
    {
        wxWindowBase::SetBackgroundStyle(wxBG_STYLE_PAINT);

        m_codeFont = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        m_codeFont.SetFaceName("Consolas");
        m_codeFontBold = m_codeFont.Bold();

        wxClientDC dc(this);
        dc.SetFont(m_codeFont);
        m_lineHeight = dc.GetCharHeight() + FromDIP(2);
        m_charWidth = dc.GetCharWidth();

        m_gutterWidth = FromDIP(24);
        m_arrowGutterWidth = FromDIP(ARROW_GUTTER_BASE_WIDTH);
        m_addressWidth = m_charWidth * 18;
        m_bytesWidth = m_charWidth * 24;
        m_mnemonicWidth = m_charWidth * 10;
        m_operandsWidth = m_charWidth * 40;

        Bind(wxEVT_PAINT, &DisassemblyControl::on_paint, this);
        Bind(wxEVT_SIZE, &DisassemblyControl::on_size, this);
        Bind(wxEVT_LEFT_DOWN, &DisassemblyControl::on_mouse_left_down, this);
        Bind(wxEVT_LEFT_DCLICK, &DisassemblyControl::on_mouse_left_dclick, this);
        Bind(wxEVT_RIGHT_DOWN, &DisassemblyControl::on_mouse_right_down, this);
        Bind(wxEVT_MOUSEWHEEL, &DisassemblyControl::on_mouse_wheel, this);
        Bind(wxEVT_KEY_DOWN, &DisassemblyControl::on_key_down, this);
        Bind(wxEVT_ERASE_BACKGROUND, &DisassemblyControl::on_erase_background, this);
        Bind(wxEVT_SCROLLWIN_TOP, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_BOTTOM, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_LINEUP, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_LINEDOWN, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_PAGEUP, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_PAGEDOWN, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_THUMBTRACK, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &DisassemblyControl::on_scroll, this);
        Bind(wxEVT_TIMER, &DisassemblyControl::on_loading_anim_timer, this, m_loadingAnimTimer.GetId());

        SetScrollRate(m_charWidth, m_lineHeight);

        if (m_header)
        {
            m_header->set_left_offset(m_gutterWidth + m_arrowGutterWidth);
        }
    }

    void DisassemblyControl::set_header(DisassemblyHeader* header)
    {
        m_header = header;
        if (m_header)
        {
            m_header->set_left_offset(m_gutterWidth + m_arrowGutterWidth);
        }
        update_virtual_size();
        Refresh();
    }

    void DisassemblyControl::on_columns_changed()
    {
        update_virtual_size();
        Refresh();
    }

    void DisassemblyControl::set_disassembly(const ::Vertex::Debugger::DisassemblyRange& range)
    {
        std::uint64_t preservedAddress{};
        int preservedOffset{};
        bool hasPreservedPosition{};

        const auto selectedAddress = get_selected_address();

        if (!m_range.lines.empty())
        {
            int scrollX{};
            int scrollY{};
            GetViewStart(&scrollX, &scrollY);

            const auto firstVisibleLine = static_cast<std::size_t>(scrollY);
            if (firstVisibleLine < m_range.lines.size())
            {
                preservedAddress = m_range.lines[firstVisibleLine].address;
                preservedOffset = scrollY - static_cast<int>(firstVisibleLine);
                hasPreservedPosition = true;
            }
        }

        const bool isFullReload = m_range.lines.empty()
            || range.lines.empty()
            || (range.startAddress != m_range.startAddress && range.endAddress != m_range.endAddress);

        m_range = range;
        m_addressToLine.clear();
        m_fetchingMore = false;

        if (isFullReload)
        {
            m_topEdgeState = EdgeState::Idle;
            m_bottomEdgeState = EdgeState::Idle;
            m_loadingAnimTimer.Stop();
        }

        for (std::size_t i = 0; i < range.lines.size(); ++i)
        {
            m_addressToLine[range.lines[i].address] = i;
        }

        if (selectedAddress != 0)
        {
            if (const auto it = m_addressToLine.find(selectedAddress); it != m_addressToLine.end())
            {
                m_selectedLine = it->second;
            }
            else if (!range.lines.empty())
            {
                m_selectedLine = std::min(m_selectedLine, range.lines.size() - 1);
            }
            else
            {
                m_selectedLine = 0;
            }
        }
        else
        {
            m_selectedLine = 0;
        }

        calculate_arrows();
        update_virtual_size();

        if (hasPreservedPosition)
        {
            if (const auto it = m_addressToLine.find(preservedAddress); it != m_addressToLine.end())
            {
                int scrollX{};
                int scrollY{};
                GetViewStart(&scrollX, &scrollY);
                const int newScrollY = static_cast<int>(it->second) + preservedOffset;
                Scroll(scrollX, std::max(0, newScrollY));
            }
        }

        Refresh();
    }

    void DisassemblyControl::set_current_instruction(std::uint64_t address)
    {
        m_currentInstructionAddress = address;
        Refresh();
    }

    void DisassemblyControl::set_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints)
    {
        m_breakpointMap.clear();
        for (const auto& bp : breakpoints)
        {
            m_breakpointMap[bp.address] = BreakpointVisualInfo{
                .id = bp.id,
                .state = bp.state,
                .hasCondition = bp.conditionType != VERTEX_BP_COND_NONE,
                .conditionType = bp.conditionType,
                .condition = bp.condition,
                .hitCount = bp.hitCount,
                .hitCountTarget = bp.hitCountTarget
            };
        }
        Refresh();
    }

    void DisassemblyControl::scroll_to_address(std::uint64_t address)
    {
        if (const auto it = m_addressToLine.find(address); it != m_addressToLine.end())
        {
            const auto lineIndex = it->second;
            const int visibleLines = get_visible_line_count();

            int scrollX{};
            int scrollY{};
            GetViewStart(&scrollX, &scrollY);

            int targetY = static_cast<int>(lineIndex) - visibleLines / 2;
            if (targetY < 0)
            {
                targetY = 0;
            }

            Scroll(scrollX, targetY);
            Refresh();
        }
    }

    void DisassemblyControl::select_address(std::uint64_t address)
    {
        if (const auto it = m_addressToLine.find(address); it != m_addressToLine.end())
        {
            m_selectedLine = it->second;
            scroll_to_address(address);

            if (m_selectionChangeCallback)
            {
                m_selectionChangeCallback(address);
            }
        }
    }

    std::uint64_t DisassemblyControl::get_selected_address() const
    {
        if (m_selectedLine < m_range.lines.size())
        {
            return m_range.lines[m_selectedLine].address;
        }
        return 0;
    }

    std::optional<std::size_t> DisassemblyControl::get_line_at_address(std::uint64_t address) const
    {
        if (const auto it = m_addressToLine.find(address); it != m_addressToLine.end())
        {
            return it->second;
        }
        return std::nullopt;
    }

    void DisassemblyControl::set_navigate_callback(NavigateCallback callback)
    {
        m_navigateCallback = std::move(callback);
    }

    void DisassemblyControl::set_breakpoint_toggle_callback(BreakpointToggleCallback callback)
    {
        m_breakpointToggleCallback = std::move(callback);
    }

    void DisassemblyControl::set_breakpoint_enable_callback(BreakpointEnableCallback callback)
    {
        m_breakpointEnableCallback = std::move(callback);
    }

    void DisassemblyControl::set_breakpoint_remove_callback(BreakpointRemoveCallback callback)
    {
        m_breakpointRemoveCallback = std::move(callback);
    }

    void DisassemblyControl::set_breakpoint_edit_condition_callback(BreakpointEditConditionCallback callback)
    {
        m_breakpointEditConditionCallback = std::move(callback);
    }

    void DisassemblyControl::set_run_to_cursor_callback(RunToCursorCallback callback)
    {
        m_runToCursorCallback = std::move(callback);
    }

    void DisassemblyControl::set_selection_change_callback(SelectionChangeCallback callback)
    {
        m_selectionChangeCallback = std::move(callback);
    }

    void DisassemblyControl::set_scroll_boundary_callback(ScrollBoundaryCallback callback)
    {
        m_scrollBoundaryCallback = std::move(callback);
    }

    void DisassemblyControl::set_xref_query_callback(XrefQueryCallback callback)
    {
        m_xrefQueryCallback = std::move(callback);
    }

    void DisassemblyControl::set_extension_result(const bool isTop, const ::Vertex::Debugger::ExtensionResult result)
    {
        auto& edgeState = isTop ? m_topEdgeState : m_bottomEdgeState;

        switch (result)
        {
            case ::Vertex::Debugger::ExtensionResult::Success:
                edgeState = EdgeState::Idle;
                break;
            case ::Vertex::Debugger::ExtensionResult::EndOfRange:
                edgeState = EdgeState::EndOfRange;
                break;
            case ::Vertex::Debugger::ExtensionResult::Error:
                edgeState = EdgeState::Error;
                break;
        }

        m_fetchingMore = false;

        if (m_topEdgeState != EdgeState::Loading && m_bottomEdgeState != EdgeState::Loading)
        {
            m_loadingAnimTimer.Stop();
        }

        Refresh();
    }

    void DisassemblyControl::render_edge_indicator(wxDC& dc, const bool isTop) const
    {
        const auto& edgeState = isTop ? m_topEdgeState : m_bottomEdgeState;
        if (edgeState == EdgeState::Idle)
        {
            return;
        }

        const int clientWidth = GetClientSize().GetWidth();
        const int indicatorHeight = FromDIP(DisassemblyIndicatorValues::INDICATOR_HEIGHT);

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int viewportTop = scrollY * m_lineHeight;
        const int clientHeight = GetClientSize().GetHeight();
        const int indicatorY = isTop ? viewportTop : (viewportTop + clientHeight - indicatorHeight);

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.edgeIndicatorBg));
        dc.DrawRectangle(0, indicatorY, clientWidth + scrollX * m_charWidth, indicatorHeight);

        dc.SetFont(m_codeFont);
        const int textY = indicatorY + (indicatorHeight - dc.GetCharHeight()) / 2;
        const int textX = m_gutterWidth + m_arrowGutterWidth + FromDIP(8);

        switch (edgeState)
        {
            case EdgeState::Loading:
            {
                wxString loadingStr = wxString::FromUTF8(
                    m_languageService.fetch_translation("debugger.disassembly.loading"));
                if (loadingStr.empty())
                {
                    loadingStr = "Loading";
                }
                const int dotCount = (m_loadingAnimFrame % (DisassemblyIndicatorValues::LOADING_DOT_COUNT + 1));
                for (int i = 0; i < dotCount; ++i)
                {
                    loadingStr += ".";
                }
                dc.SetTextForeground(m_colors.loadingText);
                dc.DrawText(loadingStr, textX, textY);
                break;
            }
            case EdgeState::EndOfRange:
            {
                wxString endStr = wxString::FromUTF8(
                    m_languageService.fetch_translation("debugger.disassembly.endOfRange"));
                if (endStr.empty())
                {
                    endStr = "--- End of disassemblable range ---";
                }
                dc.SetTextForeground(m_colors.endOfRangeText);
                dc.DrawText(endStr, textX, textY);
                break;
            }
            case EdgeState::Error:
            {
                wxString errorStr = wxString::FromUTF8(
                    m_languageService.fetch_translation("debugger.disassembly.extensionError"));
                if (errorStr.empty())
                {
                    errorStr = "Failed to load more instructions.";
                }
                wxString retryStr = wxString::FromUTF8(
                    m_languageService.fetch_translation("debugger.disassembly.retryHint"));
                if (retryStr.empty())
                {
                    retryStr = "[Click to retry]";
                }
                dc.SetTextForeground(m_colors.errorText);
                dc.DrawText(errorStr, textX, textY);
                const int retryX = textX + dc.GetTextExtent(errorStr).GetWidth() + FromDIP(8);
                dc.SetTextForeground(m_colors.errorRetryText);
                dc.DrawText(retryStr, retryX, textY);
                break;
            }
            case EdgeState::Idle:
                break;
        }
    }

    void DisassemblyControl::on_loading_anim_timer([[maybe_unused]] wxTimerEvent& event)
    {
        ++m_loadingAnimFrame;
        Refresh();
    }

    void DisassemblyControl::retry_extension(const bool isTop)
    {
        if (!m_scrollBoundaryCallback || m_range.lines.empty())
        {
            return;
        }

        auto& edgeState = isTop ? m_topEdgeState : m_bottomEdgeState;
        edgeState = EdgeState::Loading;
        m_fetchingMore = true;

        if (!m_loadingAnimTimer.IsRunning())
        {
            m_loadingAnimTimer.Start(DisassemblyIndicatorValues::LOADING_ANIM_INTERVAL_MS);
        }

        if (isTop)
        {
            m_scrollBoundaryCallback(m_range.startAddress, true);
        }
        else
        {
            m_scrollBoundaryCallback(m_range.endAddress, false);
        }

        Refresh();
    }

    void DisassemblyControl::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        DoPrepareDC(dc);
        render(dc);
    }

    void DisassemblyControl::on_size([[maybe_unused]] wxSizeEvent& event)
    {
        update_virtual_size();
        Refresh();
        event.Skip();
    }

    void DisassemblyControl::on_mouse_left_down(const wxMouseEvent& event)
    {
        SetFocus();

        const int indicatorHeight = FromDIP(DisassemblyIndicatorValues::INDICATOR_HEIGHT);
        const int clientHeight = GetClientSize().GetHeight();

        if (m_topEdgeState == EdgeState::Error && event.GetY() < indicatorHeight)
        {
            retry_extension(true);
            return;
        }

        if (m_bottomEdgeState == EdgeState::Error && event.GetY() > clientHeight - indicatorHeight)
        {
            retry_extension(false);
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && static_cast<std::size_t>(lineIndex) < m_range.lines.size())
        {
            if (event.GetX() < m_gutterWidth && m_breakpointToggleCallback)
            {
                m_breakpointToggleCallback(m_range.lines[lineIndex].address);
            }
            else
            {
                m_selectedLine = static_cast<std::size_t>(lineIndex);
                if (m_selectionChangeCallback)
                {
                    m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                }
            }
            Refresh();
        }
    }

    void DisassemblyControl::on_mouse_left_dclick(const wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && static_cast<std::size_t>(lineIndex) < m_range.lines.size())
        {
            const auto& line = m_range.lines[lineIndex];
            if (line.branchTarget.has_value() && m_navigateCallback)
            {
                m_navigateCallback(line.branchTarget.value());
            }
        }
    }

    void DisassemblyControl::on_mouse_right_down(const wxMouseEvent& event)
    {
        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int y = event.GetY() + scrollY * m_lineHeight;
        const int lineIndex = get_line_at_y(y);

        if (lineIndex >= 0 && static_cast<std::size_t>(lineIndex) < m_range.lines.size())
        {
            m_selectedLine = static_cast<std::size_t>(lineIndex);
            Refresh();

            const auto& line = m_range.lines[m_selectedLine];
            const auto bpIt = m_breakpointMap.find(line.address);
            const bool hasBp = (bpIt != m_breakpointMap.end());

            wxMenu menu;
            menu.Append(MENU_ID_TOGGLE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.toggleBreakpoint")));
            menu.Append(MENU_ID_RUN_TO_CURSOR, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.runToCursor")));

            if (hasBp)
            {
                menu.AppendSeparator();
                const bool isEnabled = (bpIt->second.state == ::Vertex::Debugger::BreakpointState::Enabled);
                if (isEnabled)
                {
                    menu.Append(MENU_ID_ENABLE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.disableBreakpoint")));
                }
                else
                {
                    menu.Append(MENU_ID_ENABLE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.enableBreakpoint")));
                }
                menu.Append(MENU_ID_EDIT_CONDITION, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.editCondition")));
                menu.Append(MENU_ID_REMOVE_BREAKPOINT, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.removeBreakpoint")));
            }

            menu.AppendSeparator();

            if (line.branchTarget.has_value())
            {
                menu.Append(MENU_ID_FOLLOW_JUMP, wxString::FromUTF8(wxString::Format(m_languageService.fetch_translation("debugger.contextMenu.followJump").c_str(), line.branchTarget.value())));
            }

            menu.Append(MENU_ID_XREFS_TO, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.xrefsTo")));
            menu.Append(MENU_ID_XREFS_FROM, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.xrefsFrom")));
            menu.AppendSeparator();

            menu.Append(MENU_ID_COPY_ADDRESS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.copyAddress")));
            menu.Append(MENU_ID_COPY_LINE, wxString::FromUTF8(m_languageService.fetch_translation("debugger.contextMenu.copyLine")));

            const int selection = GetPopupMenuSelectionFromUser(menu, event.GetPosition());
            switch (selection)
            {
                case MENU_ID_TOGGLE_BREAKPOINT:
                    if (m_breakpointToggleCallback)
                    {
                        m_breakpointToggleCallback(line.address);
                    }
                    break;
                case MENU_ID_RUN_TO_CURSOR:
                    if (m_runToCursorCallback)
                    {
                        m_runToCursorCallback(line.address);
                    }
                    break;
                case MENU_ID_ENABLE_BREAKPOINT:
                    if (hasBp && m_breakpointEnableCallback)
                    {
                        const bool isEnabled = (bpIt->second.state == ::Vertex::Debugger::BreakpointState::Enabled);
                        m_breakpointEnableCallback(bpIt->second.id, !isEnabled);
                    }
                    break;
                case MENU_ID_EDIT_CONDITION:
                    if (hasBp && m_breakpointEditConditionCallback)
                    {
                        ::Vertex::Debugger::Breakpoint bp{};
                        bp.id = bpIt->second.id;
                        bp.address = line.address;
                        bp.state = bpIt->second.state;
                        bp.conditionType = bpIt->second.conditionType;
                        bp.condition = bpIt->second.condition;
                        bp.hitCount = bpIt->second.hitCount;
                        bp.hitCountTarget = bpIt->second.hitCountTarget;
                        m_breakpointEditConditionCallback(bpIt->second.id, bp);
                    }
                    break;
                case MENU_ID_REMOVE_BREAKPOINT:
                    if (hasBp && m_breakpointRemoveCallback)
                    {
                        m_breakpointRemoveCallback(bpIt->second.id);
                    }
                    break;
                case MENU_ID_FOLLOW_JUMP:
                    if (line.branchTarget.has_value() && m_navigateCallback)
                    {
                        m_navigateCallback(line.branchTarget.value());
                    }
                    break;
                case MENU_ID_XREFS_TO:
                {
                    if (m_xrefQueryCallback)
                    {
                        m_xrefQueryCallback(line.address, ::Vertex::Debugger::XrefDirection::To,
                            [this, weak = std::weak_ptr<std::monostate>{m_lifetimeSentinel}]
                            (std::vector<::Vertex::Debugger::XrefEntry> xrefs)
                            {
                                if (weak.expired()) { return; }
                                show_xrefs_dialog(xrefs, ::Vertex::Debugger::XrefDirection::To);
                            });
                    }
                    break;
                }
                case MENU_ID_XREFS_FROM:
                {
                    if (m_xrefQueryCallback)
                    {
                        m_xrefQueryCallback(line.address, ::Vertex::Debugger::XrefDirection::From,
                            [this, weak = std::weak_ptr<std::monostate>{m_lifetimeSentinel}]
                            (std::vector<::Vertex::Debugger::XrefEntry> xrefs)
                            {
                                if (weak.expired()) { return; }
                                show_xrefs_dialog(xrefs, ::Vertex::Debugger::XrefDirection::From);
                            });
                    }
                    break;
                }
                case MENU_ID_COPY_ADDRESS:
                    if (wxTheClipboard->Open())
                    {
                        auto addrText = fmt::format("{:X}", line.address);
                        if (!line.symbolName.empty())
                        {
                            if (!line.moduleName.empty())
                            {
                                addrText += fmt::format(" {}!{}", line.moduleName, line.symbolName);
                            }
                            else
                            {
                                addrText += fmt::format(" {}", line.symbolName);
                            }
                        }
                        wxTheClipboard->SetData(new wxTextDataObject(addrText));
                        wxTheClipboard->Close();
                    }
                    break;
                case MENU_ID_COPY_LINE:
                    if (wxTheClipboard->Open())
                    {
                        std::string bytesStr;
                        for (const auto byte : line.bytes)
                        {
                            bytesStr += fmt::format("{:02X} ", byte);
                        }
                        auto fullLine = fmt::format("{:X}", line.address);
                        if (!line.moduleName.empty())
                        {
                            if (!line.sectionName.empty())
                            {
                                fullLine += fmt::format(" [{}:{}]", line.moduleName, line.sectionName);
                            }
                            else
                            {
                                fullLine += fmt::format(" [{}]", line.moduleName);
                            }
                        }
                        fullLine += fmt::format("  {}  {} {}", bytesStr, line.mnemonic, line.operands);
                        if (!line.comment.empty())
                        {
                            if (line.isFunctionEntry)
                            {
                                fullLine += fmt::format("  <{}>", line.comment);
                            }
                            else
                            {
                                fullLine += fmt::format("  ; {}", line.comment);
                            }
                        }
                        wxTheClipboard->SetData(new wxTextDataObject(fullLine));
                        wxTheClipboard->Close();
                    }
                    break;
                default:
                    break;
            }
        }
    }

    void DisassemblyControl::on_mouse_wheel(const wxMouseEvent& event)
    {
        const int rotation = event.GetWheelRotation();
        const int delta = event.GetWheelDelta();
        if (delta == 0) [[unlikely]]
        {
            return;
        }

        m_wheelAccumulator += rotation;
        const int steps = m_wheelAccumulator / delta;
        if (steps == 0)
        {
            return;
        }
        m_wheelAccumulator -= steps * delta;

        const int lines = steps * 3;

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);
        Scroll(scrollX, scrollY - lines);

        sync_header_scroll();
        check_scroll_boundaries();
    }

    void DisassemblyControl::on_key_down(wxKeyEvent& event)
    {
        if (m_range.lines.empty())
        {
            event.Skip();
            return;
        }

        const int keyCode = event.GetKeyCode();
        bool navigated{};

        switch (keyCode)
        {
            case WXK_UP:
                if (m_selectedLine > 0)
                {
                    --m_selectedLine;
                    scroll_to_address(m_range.lines[m_selectedLine].address);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                    }
                }
                navigated = true;
                break;

            case WXK_DOWN:
                if (m_selectedLine + 1 < m_range.lines.size())
                {
                    ++m_selectedLine;
                    scroll_to_address(m_range.lines[m_selectedLine].address);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                    }
                }
                navigated = true;
                break;

            case WXK_PAGEUP:
            {
                const int visibleLines = get_visible_line_count();
                if (m_selectedLine >= static_cast<std::size_t>(visibleLines))
                {
                    m_selectedLine -= visibleLines;
                }
                else
                {
                    m_selectedLine = 0;
                }
                scroll_to_address(m_range.lines[m_selectedLine].address);
                if (m_selectionChangeCallback)
                {
                    m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                }
                navigated = true;
                break;
            }

            case WXK_PAGEDOWN:
            {
                const int visibleLines = get_visible_line_count();
                m_selectedLine += visibleLines;
                if (m_selectedLine >= m_range.lines.size())
                {
                    m_selectedLine = m_range.lines.size() - 1;
                }
                scroll_to_address(m_range.lines[m_selectedLine].address);
                if (m_selectionChangeCallback)
                {
                    m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                }
                navigated = true;
                break;
            }

            case WXK_HOME:
                if (event.ControlDown() && !m_range.lines.empty())
                {
                    m_selectedLine = 0;
                    scroll_to_address(m_range.lines[m_selectedLine].address);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                    }
                    navigated = true;
                }
                break;

            case WXK_END:
                if (event.ControlDown() && !m_range.lines.empty())
                {
                    m_selectedLine = m_range.lines.size() - 1;
                    scroll_to_address(m_range.lines[m_selectedLine].address);
                    if (m_selectionChangeCallback)
                    {
                        m_selectionChangeCallback(m_range.lines[m_selectedLine].address);
                    }
                    navigated = true;
                }
                break;

            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:
                if (m_selectedLine < m_range.lines.size())
                {
                    const auto& line = m_range.lines[m_selectedLine];
                    if (line.branchTarget.has_value() && m_navigateCallback)
                    {
                        m_navigateCallback(line.branchTarget.value());
                    }
                }
                break;

            case WXK_F9:
                if (m_selectedLine < m_range.lines.size() && m_breakpointToggleCallback)
                {
                    m_breakpointToggleCallback(m_range.lines[m_selectedLine].address);
                }
                break;

            default:
                event.Skip();
                break;
        }

        if (navigated)
        {
            check_scroll_boundaries();
        }
        Refresh();
    }

    void DisassemblyControl::on_erase_background([[maybe_unused]] wxEraseEvent& event)
    {
    }

    void DisassemblyControl::on_scroll(wxScrollWinEvent& event)
    {
        event.Skip();
        sync_header_scroll();
        check_scroll_boundaries();
    }

    void DisassemblyControl::check_scroll_boundaries()
    {
        if (!m_scrollBoundaryCallback || m_range.lines.empty() || m_fetchingMore)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        GetViewStart(&scrollX, &scrollY);

        const int visibleLines = get_visible_line_count();
        const auto totalLines = static_cast<int>(m_range.lines.size());

        const bool nearTop = scrollY <= SCROLL_BOUNDARY_THRESHOLD;
        const bool nearBottom = scrollY + visibleLines >= totalLines - SCROLL_BOUNDARY_THRESHOLD;
        const bool scrollingDown = scrollY > m_previousScrollY;
        m_previousScrollY = scrollY;

        if (nearTop && m_range.startAddress == 0 && m_topEdgeState != EdgeState::EndOfRange)
        {
            m_topEdgeState = EdgeState::EndOfRange;
            Refresh();
        }

        const bool canExtendTop = nearTop && m_range.startAddress > 0
            && m_topEdgeState != EdgeState::EndOfRange && m_topEdgeState != EdgeState::Loading;
        const bool canExtendBottom = nearBottom
            && m_bottomEdgeState != EdgeState::EndOfRange && m_bottomEdgeState != EdgeState::Loading;

        if (canExtendTop && canExtendBottom)
        {
            m_fetchingMore = true;
            if (scrollingDown)
            {
                m_bottomEdgeState = EdgeState::Loading;
                if (!m_loadingAnimTimer.IsRunning())
                {
                    m_loadingAnimTimer.Start(DisassemblyIndicatorValues::LOADING_ANIM_INTERVAL_MS);
                }
                m_scrollBoundaryCallback(m_range.endAddress, false);
            }
            else
            {
                m_topEdgeState = EdgeState::Loading;
                if (!m_loadingAnimTimer.IsRunning())
                {
                    m_loadingAnimTimer.Start(DisassemblyIndicatorValues::LOADING_ANIM_INTERVAL_MS);
                }
                m_scrollBoundaryCallback(m_range.startAddress, true);
            }
            Refresh();
        }
        else if (canExtendTop)
        {
            m_fetchingMore = true;
            m_topEdgeState = EdgeState::Loading;
            if (!m_loadingAnimTimer.IsRunning())
            {
                m_loadingAnimTimer.Start(DisassemblyIndicatorValues::LOADING_ANIM_INTERVAL_MS);
            }
            m_scrollBoundaryCallback(m_range.startAddress, true);
            Refresh();
        }
        else if (canExtendBottom)
        {
            m_fetchingMore = true;
            m_bottomEdgeState = EdgeState::Loading;
            if (!m_loadingAnimTimer.IsRunning())
            {
                m_loadingAnimTimer.Start(DisassemblyIndicatorValues::LOADING_ANIM_INTERVAL_MS);
            }
            m_scrollBoundaryCallback(m_range.endAddress, false);
            Refresh();
        }
    }

    void DisassemblyControl::render(wxDC& dc) const
    {
        render_background(dc);

        if (m_range.lines.empty())
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
            static_cast<int>(m_range.lines.size())
        );

        render_arrow_gutter(dc, startLine, endLine);
        render_lines(dc, startLine, endLine);

        render_edge_indicator(dc, true);
        render_edge_indicator(dc, false);
    }

    void DisassemblyControl::render_background(wxDC& dc) const
    {
        const wxSize clientSize = GetVirtualSize();
        dc.SetBackground(wxBrush(m_colors.background));
        dc.Clear();

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.gutter));
        dc.DrawRectangle(0, 0, m_gutterWidth + m_arrowGutterWidth, clientSize.GetHeight());

        dc.SetPen(wxPen(m_colors.gutterBorder, 1));
        dc.DrawLine(m_gutterWidth + m_arrowGutterWidth, 0,
                    m_gutterWidth + m_arrowGutterWidth, clientSize.GetHeight());
    }

    void DisassemblyControl::render_arrow_gutter(wxDC& dc, int startLine, int endLine) const
    {
        for (const auto& arrow : m_arrows)
        {
            const auto sourceLineInt = static_cast<int>(arrow.sourceLineIndex);

            int effectiveTargetLine{};
            if (arrow.targetOutOfBounds)
            {
                effectiveTargetLine = arrow.targetIsAbove ? -1 : static_cast<int>(m_range.lines.size());
            }
            else
            {
                effectiveTargetLine = static_cast<int>(arrow.targetLineIndex);
            }

            const int arrowMinLine = std::min(sourceLineInt, effectiveTargetLine);
            const int arrowMaxLine = std::max(sourceLineInt, effectiveTargetLine);

            if (arrowMaxLine >= startLine && arrowMinLine <= endLine)
            {
                render_arrow(dc, arrow, startLine, endLine);
            }
        }
    }

    void DisassemblyControl::render_lines(wxDC& dc, int startLine, int endLine) const
    {
        dc.SetFont(m_codeFont);

        for (int i = startLine; i < endLine; ++i)
        {
            const int y = get_y_for_line(static_cast<std::size_t>(i));
            render_line(dc, static_cast<std::size_t>(i), y);
        }
    }

    void DisassemblyControl::render_column_content(wxDC& dc, const ::Vertex::Debugger::DisassemblyLine& line, DisassemblyColumn column, int x, int y) const
    {
        switch (column)
        {
            case DisassemblyColumn::Address:
            {
                dc.SetFont(m_codeFont);
                const auto addrText = fmt::format("{:016X}", line.address);
                if (line.isFunctionEntry && !line.symbolName.empty())
                {
                    dc.SetTextForeground(m_colors.symbolLabel);
                }
                else
                {
                    dc.SetTextForeground(m_colors.address);
                }
                dc.DrawText(addrText, x, y);

                if (!line.moduleName.empty())
                {
                    const auto addrExtent = dc.GetTextExtent(addrText);
                    dc.SetTextForeground(m_colors.moduleContext);
                    if (!line.sectionName.empty())
                    {
                        dc.DrawText(fmt::format("{}:{}", line.moduleName, line.sectionName),
                            x + addrExtent.GetWidth() + m_charWidth, y);
                    }
                    else
                    {
                        dc.DrawText(line.moduleName, x + addrExtent.GetWidth() + m_charWidth, y);
                    }
                }
                break;
            }

            case DisassemblyColumn::Bytes:
            {
                dc.SetTextForeground(m_colors.bytes);
                dc.SetFont(m_codeFont);
                std::string bytesStr;
                for (const auto byte : line.bytes)
                {
                    bytesStr += fmt::format("{:02X} ", byte);
                }
                dc.DrawText(bytesStr, x, y);
                break;
            }

            case DisassemblyColumn::Mnemonic:
            {
                wxColour mnemonicColor = m_colors.mnemonicNormal;
                switch (line.branchType)
                {
                    case ::Vertex::Debugger::BranchType::UnconditionalJump:
                    case ::Vertex::Debugger::BranchType::ConditionalJump:
                        mnemonicColor = m_colors.mnemonicJump;
                        break;
                    case ::Vertex::Debugger::BranchType::Call:
                        mnemonicColor = m_colors.mnemonicCall;
                        break;
                    case ::Vertex::Debugger::BranchType::Return:
                        mnemonicColor = m_colors.mnemonicRet;
                        break;
                    case ::Vertex::Debugger::BranchType::Loop:
                        mnemonicColor = m_colors.mnemonicJump;
                        break;
                    case ::Vertex::Debugger::BranchType::None:
                    case ::Vertex::Debugger::BranchType::Interrupt:
                    {
                        const auto& mnem = line.mnemonic;
                        if (mnem.starts_with("mov") || mnem.starts_with("lea") ||
                            mnem.starts_with("push") || mnem.starts_with("pop"))
                        {
                            mnemonicColor = m_colors.mnemonicMov;
                        }
                        else if (mnem.starts_with("add") || mnem.starts_with("sub") ||
                                 mnem.starts_with("mul") || mnem.starts_with("div") ||
                                 mnem.starts_with("inc") || mnem.starts_with("dec") ||
                                 mnem.starts_with("and") || mnem.starts_with("or") ||
                                 mnem.starts_with("xor") || mnem.starts_with("shl") ||
                                 mnem.starts_with("shr") || mnem.starts_with("cmp") ||
                                 mnem.starts_with("test"))
                        {
                            mnemonicColor = m_colors.mnemonicArith;
                        }
                        break;
                    }
                }
                dc.SetTextForeground(mnemonicColor);
                dc.SetFont(m_codeFontBold);
                dc.DrawText(line.mnemonic, x, y);
                break;
            }

            case DisassemblyColumn::Operands:
                dc.SetFont(m_codeFont);
                dc.SetTextForeground(m_colors.operands);
                dc.DrawText(line.operands, x, y);
                break;

            case DisassemblyColumn::Comment:
            {
                if (!line.comment.empty())
                {
                    dc.SetFont(m_codeFont);
                    if (line.isFunctionEntry)
                    {
                        dc.SetTextForeground(m_colors.symbolLabel);
                        dc.SetFont(m_codeFontBold);
                        dc.DrawText(fmt::format("<{}>", line.comment), x, y);
                    }
                    else if (!line.targetSymbolName.empty())
                    {
                        dc.SetTextForeground(m_colors.symbolLabel);
                        dc.DrawText("; " + line.comment, x, y);
                    }
                    else if (line.comment.starts_with("["))
                    {
                        dc.SetTextForeground(m_colors.moduleContext);
                        dc.DrawText("; " + line.comment, x, y);
                    }
                    else
                    {
                        dc.SetTextForeground(m_colors.comment);
                        dc.DrawText("; " + line.comment, x, y);
                    }
                }
                break;
            }

            case DisassemblyColumn::COUNT:
                std::unreachable();
        }
    }

    void DisassemblyControl::render_line(wxDC& dc, std::size_t lineIndex, int y) const
    {
        const auto& line = m_range.lines[lineIndex];

        wxColour bgColor = m_colors.background;

        const bool isSelected = (lineIndex == m_selectedLine);
        const bool isCurrent = (line.address == m_currentInstructionAddress);
        const auto bpIt = m_breakpointMap.find(line.address);
        const bool hasBreakpoint = (bpIt != m_breakpointMap.end());

        if (hasBreakpoint)
        {
            switch (bpIt->second.state)
            {
                case ::Vertex::Debugger::BreakpointState::Disabled:
                    bgColor = m_colors.breakpointLineDisabled;
                    break;
                case ::Vertex::Debugger::BreakpointState::Pending:
                    bgColor = m_colors.breakpointLinePending;
                    break;
                case ::Vertex::Debugger::BreakpointState::Enabled:
                case ::Vertex::Debugger::BreakpointState::Error:
                default:
                    bgColor = m_colors.breakpointLine;
                    break;
            }
        }
        else if (isCurrent)
        {
            bgColor = m_colors.currentLine;
        }
        else if (isSelected)
        {
            bgColor = m_colors.selectedLine;
        }
        else if (lineIndex % 2 == 1)
        {
            bgColor = m_colors.backgroundAlt;
        }

        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(bgColor));
        dc.DrawRectangle(m_gutterWidth + m_arrowGutterWidth, y, GetVirtualSize().GetWidth(), m_lineHeight);

        if (line.isFunctionEntry && lineIndex > 0)
        {
            dc.SetPen(wxPen(m_colors.functionEntryLine, 1, wxPENSTYLE_DOT));
            dc.DrawLine(m_gutterWidth + m_arrowGutterWidth, y, GetVirtualSize().GetWidth(), y);
        }

        constexpr int gutterX = 0;
        if (hasBreakpoint)
        {
            render_breakpoint_marker(dc, gutterX + m_gutterWidth / 2, y + m_lineHeight / 2, bpIt->second);
        }
        if (isCurrent)
        {
            render_current_instruction_marker(dc, gutterX + m_gutterWidth / 2, y + m_lineHeight / 2);
        }

        if (m_header)
        {
            const auto& columnOrder = m_header->get_column_order();
            const int padding = m_header->get_column_padding();
            int x = m_gutterWidth + m_arrowGutterWidth + padding;

            for (int i = 0; i < DisassemblyHeader::COLUMN_COUNT; ++i)
            {
                const DisassemblyColumn col = columnOrder[i];
                const int colWidth = m_header->get_column_width(col);

                render_column_content(dc, line, col, x, y);

                x += colWidth + padding;

                if (i < DisassemblyHeader::COLUMN_COUNT - 1)
                {
                    dc.SetPen(wxPen(m_separatorColor, 1));
                    dc.DrawLine(x - padding / 2, y, x - padding / 2, y + m_lineHeight);
                }
            }
        }
        else
        {
            const int contentX = m_gutterWidth + m_arrowGutterWidth + FromDIP(4);
            const int addressX = contentX;
            const int bytesX = addressX + m_addressWidth + FromDIP(8);
            const int mnemonicX = bytesX + m_bytesWidth + FromDIP(8);
            const int operandsX = mnemonicX + m_mnemonicWidth + FromDIP(4);
            const int commentX = operandsX + m_operandsWidth + FromDIP(8);

            render_column_content(dc, line, DisassemblyColumn::Address, addressX, y);
            render_column_content(dc, line, DisassemblyColumn::Bytes, bytesX, y);
            render_column_content(dc, line, DisassemblyColumn::Mnemonic, mnemonicX, y);
            render_column_content(dc, line, DisassemblyColumn::Operands, operandsX, y);
            render_column_content(dc, line, DisassemblyColumn::Comment, commentX, y);
        }

        if (line.isJumpTarget || line.isCallTarget)
        {
            dc.SetPen(wxPen(line.isCallTarget ? m_colors.arrowCall : m_colors.arrowConditional, 1));
            const int markerX = m_gutterWidth + m_arrowGutterWidth - FromDIP(4);
            dc.DrawLine(markerX, y + 2, markerX, y + m_lineHeight - 2);
        }
    }

    void DisassemblyControl::render_breakpoint_marker(wxDC& dc, int x, int y, const BreakpointVisualInfo& info) const
    {
        const int radius = FromDIP(6);
        const bool isConditional = info.hasCondition;

        auto draw_diamond = [&](int cx, int cy, int r)
        {
            std::array<wxPoint, 4> points = {{
                {cx, cy - r},
                {cx + r, cy},
                {cx, cy + r},
                {cx - r, cy}
            }};
            dc.DrawPolygon(static_cast<int>(points.size()), points.data());
        };

        switch (info.state)
        {
            case ::Vertex::Debugger::BreakpointState::Enabled:
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(m_colors.breakpointMarker));
                if (isConditional)
                {
                    draw_diamond(x, y, radius);
                }
                else
                {
                    dc.DrawCircle(x, y, radius);
                }
                break;
            }
            case ::Vertex::Debugger::BreakpointState::Disabled:
            {
                dc.SetPen(wxPen(m_colors.breakpointMarkerDisabled, FromDIP(2)));
                dc.SetBrush(*wxTRANSPARENT_BRUSH);
                if (isConditional)
                {
                    draw_diamond(x, y, radius);
                }
                else
                {
                    dc.DrawCircle(x, y, radius);
                }
                break;
            }
            case ::Vertex::Debugger::BreakpointState::Pending:
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(m_colors.breakpointMarkerPending));
                if (isConditional)
                {
                    draw_diamond(x, y, radius);
                }
                else
                {
                    dc.DrawCircle(x, y, radius);
                }
                break;
            }
            case ::Vertex::Debugger::BreakpointState::Error:
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(m_colors.breakpointMarker));
                dc.DrawCircle(x, y, radius);

                dc.SetPen(wxPen(*wxWHITE, FromDIP(2)));
                const int xSize = radius / 2;
                dc.DrawLine(x - xSize, y - xSize, x + xSize + 1, y + xSize + 1);
                dc.DrawLine(x - xSize, y + xSize, x + xSize + 1, y - xSize - 1);
                break;
            }
        }
    }

    void DisassemblyControl::render_current_instruction_marker(wxDC& dc, int x, int y) const
    {
        const int size = FromDIP(5);
        std::array<wxPoint, 3> points = {{
            {x - size, y - size},
            {x + size, y},
            {x - size, y + size}
        }};
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.SetBrush(wxBrush(m_colors.currentMarker));
        dc.DrawPolygon(static_cast<int>(points.size()), points.data());
    }

    void DisassemblyControl::calculate_arrows()
    {
        m_arrows.clear();

        for (std::size_t i = 0; i < m_range.lines.size(); ++i)
        {
            const auto& line = m_range.lines[i];
            if (line.branchTarget.has_value() && line.branchType != ::Vertex::Debugger::BranchType::None)
            {
                const auto targetAddress = line.branchTarget.value();
                ArrowInfo arrow{};
                arrow.sourceLineIndex = i;
                arrow.targetAddress = targetAddress;
                arrow.branchType = line.branchType;
                arrow.nestingLevel = 0;

                if (const auto targetIt = m_addressToLine.find(targetAddress); targetIt != m_addressToLine.end())
                {
                    arrow.targetLineIndex = targetIt->second;
                    arrow.targetOutOfBounds = false;
                }
                else
                {
                    arrow.targetOutOfBounds = true;
                    arrow.targetIsAbove = (targetAddress < m_range.startAddress);
                    arrow.targetLineIndex = arrow.targetIsAbove ? 0 : m_range.lines.size() - 1;
                }

                m_arrows.push_back(arrow);
            }
        }

        std::ranges::sort(m_arrows, [](const ArrowInfo& a, const ArrowInfo& b)
        {
            if (a.targetOutOfBounds != b.targetOutOfBounds)
            {
                return !a.targetOutOfBounds;
            }
            const auto spanA = std::abs(static_cast<long long>(a.targetLineIndex) -
                                        static_cast<long long>(a.sourceLineIndex));
            const auto spanB = std::abs(static_cast<long long>(b.targetLineIndex) -
                                        static_cast<long long>(b.sourceLineIndex));
            return spanA < spanB;
        });

        std::vector<std::pair<std::size_t, std::size_t>> usedRanges[MAX_ARROW_NESTING];

        for (auto& arrow : m_arrows)
        {
            const auto minLine = std::min(arrow.sourceLineIndex, arrow.targetLineIndex);
            const auto maxLine = std::max(arrow.sourceLineIndex, arrow.targetLineIndex);

            for (int level = 0; level < MAX_ARROW_NESTING; ++level)
            {
                bool overlaps{};
                for (const auto& [rangeMin, rangeMax] : usedRanges[level])
                {
                    if (!(maxLine < rangeMin || minLine > rangeMax))
                    {
                        overlaps = true;
                        break;
                    }
                }

                if (!overlaps)
                {
                    arrow.nestingLevel = level;
                    usedRanges[level].emplace_back(minLine, maxLine);
                    break;
                }
            }
        }

        for (auto& line : m_range.lines)
        {
            line.isJumpTarget = false;
            line.isCallTarget = false;
        }

        for (const auto& arrow : m_arrows)
        {
            if (arrow.targetOutOfBounds)
            {
                continue;
            }

            auto& targetLine = m_range.lines[arrow.targetLineIndex];

            if (arrow.branchType == ::Vertex::Debugger::BranchType::Call)
            {
                targetLine.isCallTarget = true;
            }
            else
            {
                targetLine.isJumpTarget = true;
            }
        }

        int maxNesting{};
        for (const auto& arrow : m_arrows)
        {
            maxNesting = std::max(maxNesting, arrow.nestingLevel);
        }
        m_arrowGutterWidth = FromDIP(ARROW_GUTTER_BASE_WIDTH + maxNesting * ARROW_SPACING);

        if (m_header)
        {
            m_header->set_left_offset(m_gutterWidth + m_arrowGutterWidth);
        }
    }

    void DisassemblyControl::render_arrow(wxDC& dc, const ArrowInfo& arrow, int startLine, int endLine) const
    {
        const wxColour color = get_arrow_color(arrow.branchType);
        const int penWidth = (arrow.branchType == ::Vertex::Debugger::BranchType::Call) ? 3 : 2;

        const int baseX = m_gutterWidth + m_arrowGutterWidth - FromDIP(6);
        const int arrowX = baseX - FromDIP(12) - (arrow.nestingLevel * FromDIP(ARROW_SPACING));

        const auto sourceLineInt = static_cast<int>(arrow.sourceLineIndex);
        const bool sourceAboveView = sourceLineInt < startLine;
        const bool sourceBelowView = sourceLineInt > endLine;
        const bool sourceOutOfView = sourceAboveView || sourceBelowView;

        int sourceY{};
        if (sourceAboveView)
        {
            sourceY = get_y_for_line(static_cast<std::size_t>(startLine)) - m_lineHeight;
        }
        else if (sourceBelowView)
        {
            sourceY = get_y_for_line(static_cast<std::size_t>(endLine)) + m_lineHeight;
        }
        else
        {
            sourceY = get_y_for_line(arrow.sourceLineIndex) + m_lineHeight / 2;
        }

        int targetY{};
        bool targetOutOfView{};
        bool targetAboveView{};

        if (arrow.targetOutOfBounds)
        {
            targetOutOfView = true;
            targetAboveView = arrow.targetIsAbove;
            if (arrow.targetIsAbove)
            {
                targetY = get_y_for_line(static_cast<std::size_t>(startLine)) - m_lineHeight;
            }
            else
            {
                targetY = get_y_for_line(static_cast<std::size_t>(endLine)) + m_lineHeight;
            }
        }
        else
        {
            const auto targetLineInt = static_cast<int>(arrow.targetLineIndex);
            targetAboveView = targetLineInt < startLine;
            const bool targetBelowView = targetLineInt > endLine;
            targetOutOfView = targetAboveView || targetBelowView;

            if (targetAboveView)
            {
                targetY = get_y_for_line(static_cast<std::size_t>(startLine)) - m_lineHeight;
            }
            else if (targetBelowView)
            {
                targetY = get_y_for_line(static_cast<std::size_t>(endLine)) + m_lineHeight;
            }
            else
            {
                targetY = get_y_for_line(arrow.targetLineIndex) + m_lineHeight / 2;
            }
        }

        wxPen pen(color, penWidth);
        pen.SetCap(wxCAP_BUTT);
        pen.SetJoin(wxJOIN_MITER);
        dc.SetPen(pen);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);

        if (!sourceOutOfView)
        {
            dc.DrawLine(baseX, sourceY, arrowX, sourceY);
        }

        dc.DrawLine(arrowX, sourceY, arrowX, targetY);

        const int triangleSize = FromDIP(5);

        auto draw_triangle = [&](int tipX, int tipY, int baseOffsetY)
        {
            const std::array<wxPoint, 3> tri = {{
                {tipX, tipY},
                {tipX - triangleSize, tipY + baseOffsetY},
                {tipX + triangleSize, tipY + baseOffsetY}
            }};
            dc.SetBrush(wxBrush(color));
            dc.SetPen(wxPen(color, 1));
            dc.DrawPolygon(static_cast<int>(tri.size()), tri.data());
        };

        if (targetOutOfView)
        {
            const int baseOffset = targetAboveView ? triangleSize * 2 : -triangleSize * 2;
            draw_triangle(arrowX, targetY, baseOffset);
        }
        else
        {
            dc.DrawLine(arrowX, targetY, baseX - FromDIP(6), targetY);

            const int arrowSize = FromDIP(4);
            const std::array<wxPoint, 3> arrowHead = {{
                {baseX, targetY},
                {baseX - arrowSize * 2, targetY - arrowSize},
                {baseX - arrowSize * 2, targetY + arrowSize}
            }};

            dc.SetBrush(wxBrush(color));
            dc.SetPen(wxPen(color, 1));
            dc.DrawPolygon(static_cast<int>(arrowHead.size()), arrowHead.data());
        }

        if (sourceOutOfView)
        {
            const int baseOffset = sourceAboveView ? triangleSize * 2 : -triangleSize * 2;
            draw_triangle(arrowX, sourceY, baseOffset);
        }
    }

    wxColour DisassemblyControl::get_arrow_color(::Vertex::Debugger::BranchType type) const
    {
        switch (type)
        {
            case Vertex::Debugger::BranchType::UnconditionalJump:
                return m_colors.arrowUnconditional;
            case Vertex::Debugger::BranchType::ConditionalJump:
                return m_colors.arrowConditional;
            case Vertex::Debugger::BranchType::Call:
                return m_colors.arrowCall;
            case Vertex::Debugger::BranchType::Loop:
                return m_colors.arrowLoop;
            case Vertex::Debugger::BranchType::None:
            case Vertex::Debugger::BranchType::Return:
            case Vertex::Debugger::BranchType::Interrupt:
                return m_colors.arrowConditional;
        }
        std::unreachable();
    }

    int DisassemblyControl::get_line_at_y(int y) const
    {
        return y / m_lineHeight;
    }

    int DisassemblyControl::get_y_for_line(std::size_t lineIndex) const
    {
        return static_cast<int>(lineIndex) * m_lineHeight;
    }

    int DisassemblyControl::get_visible_line_count() const
    {
        return GetClientSize().GetHeight() / m_lineHeight;
    }

    void DisassemblyControl::update_virtual_size()
    {
        const int totalHeight = static_cast<int>(m_range.lines.size()) * m_lineHeight;

        int totalWidth{};
        if (m_header)
        {
            totalWidth = m_header->get_total_width();
        }
        else
        {
            totalWidth = m_gutterWidth + m_arrowGutterWidth + m_addressWidth +
                         m_bytesWidth + m_mnemonicWidth + m_operandsWidth + FromDIP(300);
        }
        SetVirtualSize(totalWidth, totalHeight);
    }

    void DisassemblyControl::sync_header_scroll() const
    {
        if (m_header)
        {
            int scrollX{};
            int scrollY{};
            GetViewStart(&scrollX, &scrollY);
            m_header->set_horizontal_scroll_offset(scrollX * m_charWidth);
        }
    }

    void DisassemblyControl::show_xrefs_dialog(
        const std::vector<::Vertex::Debugger::XrefEntry>& xrefs,
        const ::Vertex::Debugger::XrefDirection direction)
    {
        const auto titleKey = direction == ::Vertex::Debugger::XrefDirection::To
            ? "debugger.xrefs.dialogTitleTo"
            : "debugger.xrefs.dialogTitleFrom";

        wxDialog dialog(
            this,
            wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation(titleKey)),
            wxDefaultPosition,
            FromDIP(wxSize(XrefDialogValues::DIALOG_WIDTH, XrefDialogValues::DIALOG_HEIGHT)),
            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER
        );

        auto* sizer = new wxBoxSizer(wxVERTICAL);

        if (xrefs.empty())
        {
            auto* noResultsLabel = new wxStaticText(
                &dialog, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.noResults")));
            sizer->Add(noResultsLabel, 0, wxALL | wxALIGN_CENTER, StandardWidgetValues::BORDER_TWICE);
            sizer->Add(dialog.CreateStdDialogButtonSizer(wxOK), 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            dialog.SetSizer(sizer);
            dialog.ShowModal();
            return;
        }

        auto* listCtrl = new wxListView(
            &dialog, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxLC_REPORT | wxLC_SINGLE_SEL);

        listCtrl->AppendColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.columnType")),
            wxLIST_FORMAT_LEFT, FromDIP(XrefDialogValues::COLUMN_WIDTH_TYPE));
        listCtrl->AppendColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.columnAddress")),
            wxLIST_FORMAT_LEFT, FromDIP(XrefDialogValues::COLUMN_WIDTH_ADDRESS));
        listCtrl->AppendColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.columnSymbol")),
            wxLIST_FORMAT_LEFT, FromDIP(XrefDialogValues::COLUMN_WIDTH_SYMBOL));

        const auto typeCallStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.typeCall"));
        const auto typeJumpStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.typeJump"));
        const auto typeCondJumpStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.typeCondJump"));
        const auto typeLoopStr = wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.typeLoop"));

        for (std::size_t i{}; i < xrefs.size(); ++i)
        {
            const auto& xref = xrefs[i];

            wxString typeStr;
            switch (xref.type)
            {
                case ::Vertex::Debugger::XrefType::Call:
                    typeStr = typeCallStr;
                    break;
                case ::Vertex::Debugger::XrefType::UnconditionalJump:
                    typeStr = typeJumpStr;
                    break;
                case ::Vertex::Debugger::XrefType::ConditionalJump:
                    typeStr = typeCondJumpStr;
                    break;
                case ::Vertex::Debugger::XrefType::Loop:
                    typeStr = typeLoopStr;
                    break;
            }

            const auto idx = listCtrl->InsertItem(static_cast<long>(i), typeStr);

            const auto displayAddress = direction == ::Vertex::Debugger::XrefDirection::To
                ? xref.address
                : xref.targetAddress;

            listCtrl->SetItem(idx, 1, wxString::Format("0x%llX", displayAddress));
            listCtrl->SetItem(idx, 2, wxString::FromUTF8(xref.symbolName));
            listCtrl->SetItemData(idx, static_cast<long>(i));
        }

        std::uint64_t navigateTarget{};

        const auto resolve_navigate_target = [listCtrl, &xrefs, direction](const long itemIndex) -> std::optional<std::uint64_t>
        {
            if (itemIndex < 0)
            {
                return std::nullopt;
            }

            const auto dataIdx = static_cast<std::size_t>(listCtrl->GetItemData(itemIndex));
            if (dataIdx >= xrefs.size())
            {
                return std::nullopt;
            }

            return direction == ::Vertex::Debugger::XrefDirection::To
                ? xrefs[dataIdx].address
                : xrefs[dataIdx].targetAddress;
        };

        listCtrl->Bind(wxEVT_LIST_ITEM_ACTIVATED,
            [&navigateTarget, &dialog, resolve_navigate_target](wxListEvent& evt)
            {
                if (const auto target = resolve_navigate_target(evt.GetIndex()); target.has_value())
                {
                    navigateTarget = target.value();
                    dialog.EndModal(wxID_OK);
                }
            });

        auto* goButton = new wxButton(&dialog, wxID_OK,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.xrefs.navigate")));
        auto* cancelButton = new wxButton(&dialog, wxID_CANCEL);

        auto* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        buttonSizer->AddStretchSpacer();
        buttonSizer->Add(goButton, 0, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        buttonSizer->Add(cancelButton, 0);

        sizer->Add(listCtrl, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        sizer->Add(buttonSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        dialog.SetSizer(sizer);

        goButton->Bind(wxEVT_BUTTON,
            [listCtrl, &navigateTarget, &dialog, resolve_navigate_target](wxCommandEvent&)
            {
                if (const auto target = resolve_navigate_target(listCtrl->GetFirstSelected()); target.has_value())
                {
                    navigateTarget = target.value();
                    dialog.EndModal(wxID_OK);
                }
            });

        if (dialog.ShowModal() == wxID_OK && navigateTarget != 0 && m_navigateCallback)
        {
            m_navigateCallback(navigateTarget);
        }
    }

} // namespace Vertex::View::Debugger
