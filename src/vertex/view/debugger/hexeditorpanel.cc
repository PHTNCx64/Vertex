//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/hexeditorpanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

#include <wx/clipbrd.h>
#include <wx/dataobj.h>
#include <wx/dcbuffer.h>
#include <wx/log.h>
#include <wx/menu.h>
#include <wx/msgdlg.h>
#include <wx/settings.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>

#include <algorithm>
#include <cctype>
#include <limits>
#include <string>

namespace Vertex::View::Debugger
{
    namespace
    {
        constexpr int CONTEXT_MENU_COPY_HEX = wxID_HIGHEST + 201;
        constexpr int CONTEXT_MENU_COPY_ASCII = wxID_HIGHEST + 202;
        constexpr int CONTEXT_MENU_COPY_ADDRESS = wxID_HIGHEST + 203;
        constexpr int CONTEXT_MENU_GOTO_ADDRESS = wxID_HIGHEST + 204;

        constexpr int CANVAS_MARGIN_X = 8;
        constexpr int CANVAS_MARGIN_Y = 6;
        constexpr int ADDRESS_FIELD_CHARS = 18;
        constexpr std::size_t CONTINUOUS_FETCH_BLOCK_BYTES = 4096;
        constexpr std::size_t CONTINUOUS_FETCH_THRESHOLD_ROWS = 4;
        constexpr std::size_t MAX_SLIDING_WINDOW_BYTES = 64 * 1024;

        [[nodiscard]] int key_to_hex_nibble(const int keyCode)
        {
            if (keyCode >= '0' && keyCode <= '9')
            {
                return keyCode - '0';
            }
            if (keyCode >= 'a' && keyCode <= 'f')
            {
                return 10 + (keyCode - 'a');
            }
            if (keyCode >= 'A' && keyCode <= 'F')
            {
                return 10 + (keyCode - 'A');
            }
            if (keyCode >= WXK_NUMPAD0 && keyCode <= WXK_NUMPAD9)
            {
                return keyCode - WXK_NUMPAD0;
            }
            return -1;
        }

        [[nodiscard]] std::optional<std::pair<std::uint64_t, std::uint64_t>> block_range(
            const ::Vertex::Debugger::MemoryBlock& block)
        {
            const auto size = static_cast<std::uint64_t>(block.data.size());
            if (size > std::numeric_limits<std::uint64_t>::max() - block.baseAddress)
            {
                return std::nullopt;
            }
            return std::pair<std::uint64_t, std::uint64_t>{block.baseAddress, block.baseAddress + size};
        }

        [[nodiscard]] std::optional<std::uint64_t> byte_index_to_address(
            const std::optional<std::size_t> index,
            const std::uint64_t baseAddress,
            const std::size_t byteCount)
        {
            if (!index.has_value() || *index >= byteCount)
            {
                return std::nullopt;
            }
            if (*index > std::numeric_limits<std::uint64_t>::max() - baseAddress)
            {
                return std::nullopt;
            }
            return baseAddress + *index;
        }

        [[nodiscard]] std::optional<std::size_t> address_to_byte_index(
            const std::optional<std::uint64_t> address,
            const std::uint64_t baseAddress,
            const std::size_t byteCount)
        {
            if (!address.has_value() || *address < baseAddress)
            {
                return std::nullopt;
            }

            const auto offset = *address - baseAddress;
            if (offset >= byteCount)
            {
                return std::nullopt;
            }
            if (offset > std::numeric_limits<std::size_t>::max())
            {
                return std::nullopt;
            }
            return static_cast<std::size_t>(offset);
        }
    }

    HexCanvas::HexCanvas(wxWindow* parent)
        : wxScrolledWindow(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxHSCROLL | wxVSCROLL | wxWANTS_CHARS)
    {
        m_font = wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL);
        wxWindow::SetFont(m_font);
        wxWindow::SetBackgroundStyle(wxBG_STYLE_PAINT);

        wxClientDC dc(this);
        dc.SetFont(m_font);
        dc.GetTextExtent("M", &m_charWidth, &m_charHeight);
        if (m_charWidth <= 0)
        {
            m_charWidth = 8;
        }
        if (m_charHeight <= 0)
        {
            m_charHeight = 16;
        }

        SetScrollRate(0, m_charHeight);
        Bind(wxEVT_PAINT, &HexCanvas::on_paint, this);
        update_virtual_geometry();
    }

    void HexCanvas::set_memory_block(const ::Vertex::Debugger::MemoryBlock* block)
    {
        m_memoryBlock = block;
        update_virtual_geometry();
        Refresh();
    }

    void HexCanvas::set_selected_row(const std::optional<std::size_t> rowIndex)
    {
        m_selectedRow = rowIndex;
        Refresh();
    }

    void HexCanvas::set_cursor(const std::optional<std::size_t> byteIndex, const bool asciiMode)
    {
        m_cursorByteIndex = byteIndex;
        m_cursorAsciiMode = asciiMode;
        Refresh();
    }

    void HexCanvas::set_pending_edits(const std::unordered_map<std::size_t, std::uint8_t>* pendingEdits)
    {
        m_pendingEdits = pendingEdits;
        Refresh();
    }

    void HexCanvas::set_selection_range(const std::optional<std::pair<std::size_t, std::size_t>> selectionRange)
    {
        m_selectionRange = selectionRange;
        Refresh();
    }

    std::optional<std::size_t> HexCanvas::hit_test_row(const wxPoint& clientPoint) const
    {
        if (m_memoryBlock == nullptr)
        {
            return std::nullopt;
        }

        int x{};
        int y{};
        CalcUnscrolledPosition(clientPoint.x, clientPoint.y, &x, &y);
        if (y < CANVAS_MARGIN_Y)
        {
            return std::nullopt;
        }

        const auto row = static_cast<std::size_t>((y - CANVAS_MARGIN_Y) / m_charHeight);
        const auto rowCount =
            (m_memoryBlock->data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;
        if (row >= rowCount)
        {
            return std::nullopt;
        }

        return row;
    }

    std::optional<std::pair<std::size_t, bool>> HexCanvas::hit_test_byte(const wxPoint& clientPoint) const
    {
        if (m_memoryBlock == nullptr)
        {
            return std::nullopt;
        }

        int x{};
        int y{};
        CalcUnscrolledPosition(clientPoint.x, clientPoint.y, &x, &y);

        const auto row = hit_test_row(clientPoint);
        if (!row.has_value())
        {
            return std::nullopt;
        }

        const auto rowOffset = row.value() * MemoryViewValues::BYTES_PER_ROW;
        const auto addressFieldWidth = ADDRESS_FIELD_CHARS * m_charWidth;
        const auto hexStartX = CANVAS_MARGIN_X + addressFieldWidth + m_charWidth;
        const auto asciiStartX =
            hexStartX + static_cast<int>(MemoryViewValues::BYTES_PER_ROW * 3 * m_charWidth) + (2 * m_charWidth);
        const auto hexWidth = static_cast<int>(MemoryViewValues::BYTES_PER_ROW * 3 * m_charWidth);
        const auto asciiWidth = static_cast<int>(MemoryViewValues::BYTES_PER_ROW * m_charWidth);

        if (x >= hexStartX && x < hexStartX + hexWidth)
        {
            const auto col = static_cast<std::size_t>((x - hexStartX) / (3 * m_charWidth));
            const auto byteIndex = rowOffset + col;
            if (col < MemoryViewValues::BYTES_PER_ROW && byteIndex < m_memoryBlock->data.size())
            {
                return std::pair<std::size_t, bool>{byteIndex, false};
            }
        }

        if (x >= asciiStartX && x < asciiStartX + asciiWidth)
        {
            const auto col = static_cast<std::size_t>((x - asciiStartX) / m_charWidth);
            const auto byteIndex = rowOffset + col;
            if (col < MemoryViewValues::BYTES_PER_ROW && byteIndex < m_memoryBlock->data.size())
            {
                return std::pair<std::size_t, bool>{byteIndex, true};
            }
        }

        return std::nullopt;
    }

    std::size_t HexCanvas::get_visible_rows() const
    {
        int clientWidth{};
        int clientHeight{};
        GetClientSize(&clientWidth, &clientHeight);

        if (m_charHeight <= 0)
        {
            return 1;
        }

        return std::max<std::size_t>(1, static_cast<std::size_t>(clientHeight / m_charHeight));
    }

    void HexCanvas::ensure_row_visible(const std::size_t rowIndex)
    {
        int xUnits{};
        int yUnits{};
        GetViewStart(&xUnits, &yUnits);

        const auto visibleRows = get_visible_rows();
        const auto topRow = static_cast<std::size_t>(std::max(0, yUnits));
        if (rowIndex < topRow)
        {
            Scroll(xUnits, static_cast<int>(rowIndex));
            return;
        }

        if (rowIndex >= topRow + visibleRows)
        {
            Scroll(xUnits, static_cast<int>(rowIndex - visibleRows + 1));
        }
    }

    void HexCanvas::on_paint([[maybe_unused]] wxPaintEvent& event)
    {
        wxAutoBufferedPaintDC dc(this);
        PrepareDC(dc);
        dc.SetFont(m_font);
        dc.SetBackground(wxBrush(wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW)));
        dc.Clear();

        if (m_memoryBlock == nullptr)
        {
            return;
        }

        const auto rowCount =
            (m_memoryBlock->data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;
        if (rowCount == 0)
        {
            return;
        }

        const auto defaultTextColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        const auto unreadableTextColor = wxSystemSettings::GetColour(wxSYS_COLOUR_GRAYTEXT);
        const auto baseBackground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        const wxColour modifiedTextColor{192, 32, 32};
        const wxColour pendingTextColor{36, 86, 188};
        const auto selectedBackground = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHT);
        const auto selectedTextColor = wxSystemSettings::GetColour(wxSYS_COLOUR_HIGHLIGHTTEXT);
        const auto hasSelection = m_selectionRange.has_value();
        const auto selectionStart = hasSelection ? m_selectionRange->first : 0;
        const auto selectionEnd = hasSelection ? m_selectionRange->second : 0;

        auto adjust_color = [](const wxColour& color, const int delta) -> wxColour
        {
            auto clamp_channel = [delta](const int value) -> unsigned char
            {
                return static_cast<unsigned char>(std::clamp(value + delta, 0, 255));
            };

            return wxColour{
                clamp_channel(color.Red()),
                clamp_channel(color.Green()),
                clamp_channel(color.Blue())};
        };

        const auto luminance =
            (static_cast<int>(baseBackground.Red()) +
             static_cast<int>(baseBackground.Green()) +
             static_cast<int>(baseBackground.Blue())) / 3;
        const bool darkBase = luminance < 128;
        const auto mappedRegionBackgroundA = adjust_color(baseBackground, darkBase ? 10 : -6);
        const auto mappedRegionBackgroundB = adjust_color(baseBackground, darkBase ? 16 : -12);
        const auto regionBoundaryColor = adjust_color(defaultTextColor, darkBase ? 36 : -72);

        int scrollXUnits{};
        int scrollYUnits{};
        GetViewStart(&scrollXUnits, &scrollYUnits);

        int scrollPixelsX{};
        int scrollPixelsY{};
        GetScrollPixelsPerUnit(&scrollPixelsX, &scrollPixelsY);

        int clientWidth{};
        int clientHeight{};
        GetClientSize(&clientWidth, &clientHeight);

        const auto visibleTop = scrollYUnits * scrollPixelsY;
        const auto firstRow = static_cast<std::size_t>(std::max(0, (visibleTop - CANVAS_MARGIN_Y) / m_charHeight));
        const auto visibleRows = static_cast<std::size_t>(clientHeight / m_charHeight + 3);
        const auto lastRow = std::min(rowCount, firstRow + visibleRows);

        const auto addressFieldWidth = ADDRESS_FIELD_CHARS * m_charWidth;
        const auto hexStartX = CANVAS_MARGIN_X + addressFieldWidth + m_charWidth;
        const auto asciiStartX = hexStartX + static_cast<int>(MemoryViewValues::BYTES_PER_ROW * 3 * m_charWidth) + (2 * m_charWidth);
        const auto rowDrawWidth = std::max(0, clientWidth - CANVAS_MARGIN_X * 2);

        auto find_region_index = [this](const std::uint64_t address) -> std::optional<std::size_t>
        {
            for (std::size_t i{}; i < m_memoryBlock->regions.size(); ++i)
            {
                const auto& region = m_memoryBlock->regions[i];
                if (address >= region.startAddress && address < region.endAddress)
                {
                    return i;
                }
            }
            return std::nullopt;
        };

        auto has_region_boundary_in_row = [this](const std::uint64_t rowStart, const std::uint64_t rowEnd) -> bool
        {
            for (const auto& region : m_memoryBlock->regions)
            {
                if (region.startAddress >= rowStart && region.startAddress < rowEnd)
                {
                    return true;
                }
            }
            return false;
        };

        for (std::size_t row = firstRow; row < lastRow; ++row)
        {
            const auto rowOffset = row * MemoryViewValues::BYTES_PER_ROW;
            const auto y = CANVAS_MARGIN_Y + static_cast<int>(row * m_charHeight);
            const auto rowSelected = m_selectedRow.has_value() && m_selectedRow.value() == row;

            if (rowSelected)
            {
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(selectedBackground));
                dc.DrawRectangle(CANVAS_MARGIN_X, y, std::max(0, clientWidth - CANVAS_MARGIN_X * 2), m_charHeight);
            }

            std::uint64_t rowAddress = m_memoryBlock->baseAddress;
            if (rowOffset <= std::numeric_limits<std::uint64_t>::max() - rowAddress)
            {
                rowAddress += rowOffset;
            }

            const auto rowByteCount = std::min<std::size_t>(
                MemoryViewValues::BYTES_PER_ROW,
                m_memoryBlock->data.size() - rowOffset);
            auto rowEndAddress = rowAddress;
            if (static_cast<std::uint64_t>(rowByteCount) <= std::numeric_limits<std::uint64_t>::max() - rowEndAddress)
            {
                rowEndAddress += static_cast<std::uint64_t>(rowByteCount);
            }
            else
            {
                rowEndAddress = std::numeric_limits<std::uint64_t>::max();
            }
            const auto regionIndex = find_region_index(rowAddress);
            const bool rowHasBoundary = has_region_boundary_in_row(rowAddress, rowEndAddress);

            if (regionIndex.has_value() && !rowSelected)
            {
                const auto rowBackground = (*regionIndex % 2 == 0)
                    ? mappedRegionBackgroundA
                    : mappedRegionBackgroundB;
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.SetBrush(wxBrush(rowBackground));
                dc.DrawRectangle(CANVAS_MARGIN_X, y, rowDrawWidth, m_charHeight);
            }

            dc.SetTextForeground(rowSelected ? selectedTextColor : defaultTextColor);
            dc.DrawText(wxString::FromUTF8(fmt::format("{:016X}: ", rowAddress)), CANVAS_MARGIN_X, y);

            if (rowHasBoundary && !rowSelected)
            {
                dc.SetPen(wxPen(regionBoundaryColor));
                dc.DrawLine(CANVAS_MARGIN_X, y, CANVAS_MARGIN_X + rowDrawWidth, y);
            }

            for (std::size_t col{}; col < MemoryViewValues::BYTES_PER_ROW; ++col)
            {
                const auto byteIndex = rowOffset + col;
                const auto tokenX = hexStartX + static_cast<int>(col * 3 * m_charWidth);
                const auto asciiX = asciiStartX + static_cast<int>(col * m_charWidth);

                std::string hexToken{"  "};
                char asciiToken{' '};
                auto tokenColor = defaultTextColor;
                const auto byteSelected = hasSelection && byteIndex >= selectionStart && byteIndex <= selectionEnd;

                if (byteIndex < m_memoryBlock->data.size())
                {
                    const auto isReadable = byteIndex < m_memoryBlock->readable.size()
                        ? m_memoryBlock->readable[byteIndex]
                        : true;
                    const auto isModified = byteIndex < m_memoryBlock->modified.size()
                        ? m_memoryBlock->modified[byteIndex]
                        : false;

                    std::uint8_t byte = m_memoryBlock->data[byteIndex];
                    bool isPending{};
                    if (m_pendingEdits != nullptr)
                    {
                        if (const auto it = m_pendingEdits->find(byteIndex); it != m_pendingEdits->end())
                        {
                            byte = it->second;
                            isPending = true;
                        }
                    }

                    if (!isReadable)
                    {
                        hexToken = "??";
                        asciiToken = '?';
                        tokenColor = unreadableTextColor;
                    }
                    else
                    {
                        hexToken = (isModified || isPending)
                            ? fmt::format("{:02x}", byte)
                            : fmt::format("{:02X}", byte);
                        asciiToken = (byte >= 32 && byte < 127)
                            ? static_cast<char>(byte)
                            : '.';
                        tokenColor = isPending
                            ? pendingTextColor
                            : (isModified ? modifiedTextColor : defaultTextColor);
                    }
                }

                if (byteSelected)
                {
                    dc.SetPen(*wxTRANSPARENT_PEN);
                    dc.SetBrush(wxBrush(selectedBackground));
                    dc.DrawRectangle(tokenX - 1, y, 2 * m_charWidth + 2, m_charHeight);
                    dc.DrawRectangle(asciiX - 1, y, m_charWidth + 2, m_charHeight);
                }

                dc.SetTextForeground((rowSelected || byteSelected) ? selectedTextColor : tokenColor);
                dc.DrawText(wxString::FromUTF8(hexToken), tokenX, y);
                dc.DrawText(wxString::FromUTF8(std::string{asciiToken}), asciiX, y);

                if (m_cursorByteIndex.has_value() && m_cursorByteIndex.value() == byteIndex)
                {
                    const auto cursorX = m_cursorAsciiMode ? asciiX : tokenX;
                    const auto cursorWidth = m_cursorAsciiMode ? m_charWidth : (2 * m_charWidth);
                    dc.SetPen(wxPen(rowSelected ? selectedTextColor : defaultTextColor));
                    dc.SetBrush(*wxTRANSPARENT_BRUSH);
                    dc.DrawRectangle(cursorX - 1, y, cursorWidth + 2, m_charHeight);
                }
            }
        }
    }

    void HexCanvas::update_virtual_geometry()
    {
        const auto rowCount = m_memoryBlock == nullptr
            ? std::size_t{}
            : (m_memoryBlock->data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;

        constexpr auto textWidthChars = ADDRESS_FIELD_CHARS + 2
            + static_cast<int>(MemoryViewValues::BYTES_PER_ROW * 3)
            + 2
            + static_cast<int>(MemoryViewValues::BYTES_PER_ROW);

        const auto virtualWidth = CANVAS_MARGIN_X * 2 + textWidthChars * m_charWidth;
        const auto virtualHeight = CANVAS_MARGIN_Y * 2 + static_cast<int>(std::max<std::size_t>(1, rowCount) * m_charHeight);

        SetVirtualSize(virtualWidth, virtualHeight);
        SetScrollRate(0, m_charHeight);
    }

    HexEditorPanel::HexEditorPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void HexEditorPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_addressBarSizer = new wxBoxSizer(wxHORIZONTAL);

        m_addressInput = new wxTextCtrl(
            this,
            wxID_ANY,
            "0x",
            wxDefaultPosition,
            wxSize(FromDIP(150), -1),
            wxTE_PROCESS_ENTER);
        m_goButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.go")));
        m_regionInfoText = new wxStaticText(
            this,
            wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.editMode")));

        m_canvas = new HexCanvas(this);
        m_canvas->SetToolTip(wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.editMode")));
    }

    void HexEditorPanel::layout_controls()
    {
        m_addressBarSizer->Add(
            new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.address"))),
            StandardWidgetValues::NO_PROPORTION,
            wxALIGN_CENTER_VERTICAL | wxRIGHT,
            StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_addressInput, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_goButton, StandardWidgetValues::NO_PROPORTION);

        m_mainSizer->Add(
            m_addressBarSizer,
            StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxALL,
            StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(
            m_regionInfoText,
            StandardWidgetValues::NO_PROPORTION,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(
            m_canvas,
            StandardWidgetValues::STANDARD_PROPORTION,
            wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
            StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void HexEditorPanel::bind_events()
    {
        m_goButton->Bind(wxEVT_BUTTON, &HexEditorPanel::on_goto_address, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &HexEditorPanel::on_goto_address, this);
        m_canvas->Bind(wxEVT_LEFT_DCLICK, &HexEditorPanel::on_canvas_double_click, this);
        m_canvas->Bind(wxEVT_LEFT_DOWN, &HexEditorPanel::on_canvas_left_down, this);
        m_canvas->Bind(wxEVT_LEFT_UP, &HexEditorPanel::on_canvas_left_up, this);
        m_canvas->Bind(wxEVT_MOUSE_CAPTURE_LOST, &HexEditorPanel::on_canvas_capture_lost, this);
        m_canvas->Bind(wxEVT_MOTION, &HexEditorPanel::on_canvas_mouse_move, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_TOP, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_BOTTOM, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_LINEUP, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_LINEDOWN, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_PAGEUP, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_PAGEDOWN, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_THUMBTRACK, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_SCROLLWIN_THUMBRELEASE, &HexEditorPanel::on_canvas_scroll, this);
        m_canvas->Bind(wxEVT_MOUSEWHEEL, &HexEditorPanel::on_canvas_mouse_wheel, this);
        m_canvas->Bind(wxEVT_CONTEXT_MENU, &HexEditorPanel::on_context_menu, this);
        m_canvas->Bind(wxEVT_KEY_DOWN, &HexEditorPanel::on_canvas_key_down, this);

        Bind(wxEVT_MENU, &HexEditorPanel::on_copy_hex, this, CONTEXT_MENU_COPY_HEX);
        Bind(wxEVT_MENU, &HexEditorPanel::on_copy_ascii, this, CONTEXT_MENU_COPY_ASCII);
        Bind(wxEVT_MENU, &HexEditorPanel::on_copy_address, this, CONTEXT_MENU_COPY_ADDRESS);
        Bind(wxEVT_MENU, &HexEditorPanel::on_goto_row_address, this, CONTEXT_MENU_GOTO_ADDRESS);
    }

    void HexEditorPanel::update_data(const ::Vertex::Debugger::MemoryBlock& block)
    {
        const auto previousBaseAddress = m_memoryBlock.baseAddress;
        const auto previousByteCount = m_memoryBlock.data.size();
        const auto previousCursorAddress = byte_index_to_address(
            m_cursorByteIndex,
            previousBaseAddress,
            previousByteCount);
        const auto previousSelectedRowAddress = byte_index_to_address(
            m_selectedRowIndex.has_value()
                ? std::optional<std::size_t>{m_selectedRowIndex.value() * MemoryViewValues::BYTES_PER_ROW}
                : std::nullopt,
            previousBaseAddress,
            previousByteCount);
        const auto previousContextRowAddress = byte_index_to_address(
            m_contextRowIndex.has_value()
                ? std::optional<std::size_t>{m_contextRowIndex.value() * MemoryViewValues::BYTES_PER_ROW}
                : std::nullopt,
            previousBaseAddress,
            previousByteCount);

        if (m_forceReplaceOnNextUpdate || m_memoryBlock.data.empty())
        {
            m_memoryBlock = block;
        }
        else
        {
            merge_memory_block(block);
        }
        m_forceReplaceOnNextUpdate = false;
        m_baseAddress = m_memoryBlock.baseAddress;

        m_pendingEdits.clear();
        m_pendingUndoSnapshots.clear();
        m_pendingLowNibble = false;
        clear_selection();

        if (m_memoryBlock.data.empty())
        {
            m_cursorByteIndex.reset();
            m_selectedRowIndex.reset();
            m_contextRowIndex.reset();
            refresh_display();
            return;
        }

        m_cursorByteIndex = address_to_byte_index(
            previousCursorAddress,
            m_memoryBlock.baseAddress,
            m_memoryBlock.data.size());

        auto map_row = [this](const std::optional<std::uint64_t> rowAddress) -> std::optional<std::size_t>
        {
            const auto byteIndex = address_to_byte_index(
                rowAddress,
                m_memoryBlock.baseAddress,
                m_memoryBlock.data.size());
            if (!byteIndex.has_value())
            {
                return std::nullopt;
            }
            return *byteIndex / MemoryViewValues::BYTES_PER_ROW;
        };

        if (m_cursorByteIndex.has_value())
        {
            const auto rowIndex = *m_cursorByteIndex / MemoryViewValues::BYTES_PER_ROW;
            m_selectedRowIndex = rowIndex;
            m_contextRowIndex = rowIndex;
        }
        else
        {
            m_selectedRowIndex = map_row(previousSelectedRowAddress);
            m_contextRowIndex = map_row(previousContextRowAddress);
        }

        refresh_display();
    }

    void HexEditorPanel::set_address(const std::uint64_t address) const
    {
        m_addressInput->SetValue(fmt::format("0x{:X}", address));
    }

    void HexEditorPanel::set_navigate_callback(NavigateCallback callback)
    {
        m_navigateCallback = std::move(callback);
    }

    void HexEditorPanel::set_write_callback(WriteMemoryCallback callback)
    {
        m_writeCallback = std::move(callback);
    }

    void HexEditorPanel::on_goto_address([[maybe_unused]] wxCommandEvent& event)
    {
        const auto input = m_addressInput->GetValue();
        unsigned long long address{};
        if (input.ToULongLong(&address, 16) && m_navigateCallback)
        {
            m_forceReplaceOnNextUpdate = true;
            m_lastContinuousFetchAddress.reset();
            m_navigateCallback(address);
        }
    }

    std::optional<std::vector<std::uint8_t>> HexEditorPanel::parse_hex_bytes(const std::string_view text)
    {
        std::string digits{};
        digits.reserve(text.size());

        for (const unsigned char ch : text)
        {
            if (std::isxdigit(ch))
            {
                digits.push_back(static_cast<char>(ch));
                continue;
            }

            if (std::isspace(ch) || ch == ',' || ch == ';')
            {
                continue;
            }

            return std::nullopt;
        }

        if (digits.empty() || (digits.size() % 2) != 0)
        {
            return std::nullopt;
        }

        auto parse_nibble = [](const char ch) -> std::uint8_t
        {
            if (ch >= '0' && ch <= '9')
            {
                return static_cast<std::uint8_t>(ch - '0');
            }
            if (ch >= 'a' && ch <= 'f')
            {
                return static_cast<std::uint8_t>(10 + (ch - 'a'));
            }
            if (ch >= 'A' && ch <= 'F')
            {
                return static_cast<std::uint8_t>(10 + (ch - 'A'));
            }
            return 0;
        };

        std::vector<std::uint8_t> output{};
        output.reserve(digits.size() / 2);

        for (std::size_t i{}; i < digits.size(); i += 2)
        {
            const auto high = parse_nibble(digits[i]);
            const auto low = parse_nibble(digits[i + 1]);
            output.push_back(static_cast<std::uint8_t>((high << 4) | low));
        }

        return output;
    }

    void HexEditorPanel::on_canvas_double_click(wxMouseEvent& event)
    {
        // wx asserts if capture is lost without handling while a modal dialog opens on double-click.
        if (m_canvas->HasCapture())
        {
            m_canvas->ReleaseMouse();
        }
        m_selectionDragging = false;

        const auto hit = m_canvas->hit_test_byte(event.GetPosition());
        if (hit.has_value())
        {
            set_cursor(hit->first, hit->second, std::nullopt, false);
            edit_row(hit->first / MemoryViewValues::BYTES_PER_ROW);
        }
        event.Skip();
    }

    void HexEditorPanel::on_canvas_left_down(wxMouseEvent& event)
    {
        m_canvas->SetFocus();

        const auto hit = m_canvas->hit_test_byte(event.GetPosition());
        if (hit.has_value())
        {
            set_cursor(hit->first, hit->second, std::nullopt, false);
            m_selectionAnchorByteIndex = hit->first;
            m_selectionEndByteIndex.reset();
            m_selectionDragging = true;
            if (!m_canvas->HasCapture())
            {
                m_canvas->CaptureMouse();
            }
        }
        else if (const auto row = m_canvas->hit_test_row(event.GetPosition()); row.has_value())
        {
            const auto rowOffset = row.value() * MemoryViewValues::BYTES_PER_ROW;
            if (rowOffset < m_memoryBlock.data.size())
            {
                set_cursor(rowOffset, false, std::nullopt, false);
            }

            clear_selection();
            refresh_display();
        }
        else
        {
            clear_selection();
            refresh_display();
        }

        event.Skip();
    }

    void HexEditorPanel::on_canvas_left_up(wxMouseEvent& event)
    {
        m_selectionDragging = false;
        if (m_canvas->HasCapture())
        {
            m_canvas->ReleaseMouse();
        }
        event.Skip();
    }

    void HexEditorPanel::on_canvas_capture_lost([[maybe_unused]] wxMouseCaptureLostEvent& event)
    {
        m_selectionDragging = false;
    }

    void HexEditorPanel::on_canvas_mouse_move(wxMouseEvent& event)
    {
        if (!m_selectionDragging || !event.LeftIsDown())
        {
            if (const auto hit = m_canvas->hit_test_byte(event.GetPosition()); hit.has_value())
            {
                update_region_tooltip(hit->first);
            }
            else
            {
                update_region_tooltip(m_cursorByteIndex);
            }
            event.Skip();
            return;
        }

        const auto hit = m_canvas->hit_test_byte(event.GetPosition());
        if (!hit.has_value())
        {
            event.Skip();
            return;
        }

        if (!m_selectionAnchorByteIndex.has_value())
        {
            m_selectionAnchorByteIndex = hit->first;
        }
        m_selectionEndByteIndex = hit->first;

        if (!m_cursorByteIndex.has_value() || m_cursorByteIndex.value() != hit->first || m_asciiInputMode != hit->second)
        {
            set_cursor(hit->first, hit->second, std::nullopt, false);
        }
        refresh_display();
        event.Skip();
    }

    void HexEditorPanel::on_canvas_scroll(wxScrollWinEvent& event)
    {
        event.Skip();
        CallAfter([this]()
        {
            check_continuous_fetch();
        });
    }

    void HexEditorPanel::on_canvas_mouse_wheel(wxMouseEvent& event)
    {
        event.Skip();
        CallAfter([this]()
        {
            check_continuous_fetch();
        });
    }

    void HexEditorPanel::check_continuous_fetch(const std::optional<std::ptrdiff_t> cursorDelta)
    {
        if (!m_navigateCallback || m_memoryBlock.data.empty())
        {
            return;
        }

        const auto rowCount =
            (m_memoryBlock.data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;
        if (rowCount == 0)
        {
            return;
        }

        int scrollX{};
        int scrollY{};
        m_canvas->GetViewStart(&scrollX, &scrollY);

        const auto topRow = static_cast<std::size_t>(std::max(0, scrollY));
        const auto visibleRows = m_canvas->get_visible_rows();
        const auto bottomRow = std::min<std::size_t>(
            rowCount - 1,
            topRow + visibleRows - 1);

        const auto cursorRow = m_cursorByteIndex.has_value()
            ? std::optional<std::size_t>{*m_cursorByteIndex / MemoryViewValues::BYTES_PER_ROW}
            : std::nullopt;

        const bool nearTop = topRow <= CONTINUOUS_FETCH_THRESHOLD_ROWS
            || (cursorRow.has_value() && cursorRow.value() <= CONTINUOUS_FETCH_THRESHOLD_ROWS);
        const bool nearBottom = bottomRow + CONTINUOUS_FETCH_THRESHOLD_ROWS >= rowCount - 1
            || (cursorRow.has_value() && cursorRow.value() + CONTINUOUS_FETCH_THRESHOLD_ROWS >= rowCount - 1);

        if (!nearTop && !nearBottom)
        {
            m_lastContinuousFetchAddress.reset();
            m_previousTopRow = topRow;
            return;
        }

        bool fetchTop{};
        bool fetchBottom{};

        if (nearTop && nearBottom)
        {
            if (cursorDelta.has_value() && *cursorDelta != 0)
            {
                fetchBottom = *cursorDelta > 0;
                fetchTop = *cursorDelta < 0;
            }
            else if (m_previousTopRow.has_value() && topRow != *m_previousTopRow)
            {
                fetchBottom = topRow > *m_previousTopRow;
                fetchTop = topRow < *m_previousTopRow;
            }
            else if (cursorRow.has_value())
            {
                const auto midpoint = rowCount / 2;
                fetchBottom = cursorRow.value() >= midpoint;
                fetchTop = !fetchBottom;
            }
            else
            {
                fetchBottom = true;
            }
        }
        else
        {
            fetchTop = nearTop;
            fetchBottom = nearBottom;
        }

        m_previousTopRow = topRow;

        if (fetchTop)
        {
            request_adjacent_block(true);
        }
        else if (fetchBottom)
        {
            request_adjacent_block(false);
        }
    }

    void HexEditorPanel::request_adjacent_block(const bool fetchTop)
    {
        if (!m_navigateCallback || m_memoryBlock.data.empty())
        {
            return;
        }

        std::uint64_t targetAddress{};
        if (fetchTop)
        {
            if (m_memoryBlock.baseAddress == 0)
            {
                return;
            }

            const auto fetchBytes = static_cast<std::uint64_t>(CONTINUOUS_FETCH_BLOCK_BYTES);
            targetAddress = m_memoryBlock.baseAddress > fetchBytes
                ? m_memoryBlock.baseAddress - fetchBytes
                : 0;
        }
        else
        {
            const auto currentSize = static_cast<std::uint64_t>(m_memoryBlock.data.size());
            if (currentSize > std::numeric_limits<std::uint64_t>::max() - m_memoryBlock.baseAddress)
            {
                return;
            }
            targetAddress = m_memoryBlock.baseAddress + currentSize;
        }

        if (m_lastContinuousFetchAddress.has_value()
            && m_lastContinuousFetchAddress.value() == targetAddress
            && m_lastContinuousFetchWindowBase == m_memoryBlock.baseAddress
            && m_lastContinuousFetchWindowSize == m_memoryBlock.data.size())
        {
            return;
        }

        m_lastContinuousFetchAddress = targetAddress;
        m_lastContinuousFetchWindowBase = m_memoryBlock.baseAddress;
        m_lastContinuousFetchWindowSize = m_memoryBlock.data.size();
        m_navigateCallback(targetAddress);
    }

    void HexEditorPanel::merge_memory_block(const ::Vertex::Debugger::MemoryBlock& block)
    {
        if (block.data.empty())
        {
            m_memoryBlock = block;
            return;
        }

        const auto currentRange = block_range(m_memoryBlock);
        const auto incomingRange = block_range(block);
        if (!currentRange.has_value() || !incomingRange.has_value())
        {
            m_memoryBlock = block;
            return;
        }

        // Preserve continuity only when ranges overlap or touch.
        const auto disjointWithGap = incomingRange->first > currentRange->second
            || incomingRange->second < currentRange->first;
        if (disjointWithGap)
        {
            m_memoryBlock = block;
            return;
        }

        const auto mergedStart = std::min(currentRange->first, incomingRange->first);
        const auto mergedEnd = std::max(currentRange->second, incomingRange->second);
        if (mergedEnd < mergedStart)
        {
            m_memoryBlock = block;
            return;
        }

        const auto mergedSizeU64 = mergedEnd - mergedStart;
        if (mergedSizeU64 > std::numeric_limits<std::size_t>::max())
        {
            m_memoryBlock = block;
            return;
        }
        const auto mergedSize = static_cast<std::size_t>(mergedSizeU64);

        ::Vertex::Debugger::MemoryBlock merged{};
        merged.baseAddress = mergedStart;
        merged.data.assign(mergedSize, 0);
        merged.readable.assign(mergedSize, false);
        merged.modified.assign(mergedSize, false);

        auto overlay = [&merged, mergedStart](const ::Vertex::Debugger::MemoryBlock& source)
        {
            if (source.baseAddress < mergedStart)
            {
                return;
            }

            const auto offsetU64 = source.baseAddress - mergedStart;
            if (offsetU64 > std::numeric_limits<std::size_t>::max())
            {
                return;
            }
            const auto offset = static_cast<std::size_t>(offsetU64);
            if (offset > merged.data.size() || source.data.size() > merged.data.size() - offset)
            {
                return;
            }

            std::ranges::copy(source.data, merged.data.begin() + static_cast<std::ptrdiff_t>(offset));
            for (std::size_t i{}; i < source.data.size(); ++i)
            {
                const auto srcIndex = i;
                const auto dstIndex = offset + i;
                merged.readable[dstIndex] = srcIndex < source.readable.size()
                    ? source.readable[srcIndex]
                    : true;
                merged.modified[dstIndex] = srcIndex < source.modified.size()
                    ? source.modified[srcIndex]
                    : false;
            }
        };

        overlay(m_memoryBlock);
        overlay(block);

        {
            std::vector<::Vertex::Debugger::MemoryRegionSlice> allRegions{};
            allRegions.reserve(m_memoryBlock.regions.size() + block.regions.size());
            allRegions.insert(allRegions.end(), m_memoryBlock.regions.begin(), m_memoryBlock.regions.end());
            allRegions.insert(allRegions.end(), block.regions.begin(), block.regions.end());

            std::ranges::sort(allRegions, [](const auto& lhs, const auto& rhs)
            {
                if (lhs.startAddress != rhs.startAddress)
                {
                    return lhs.startAddress < rhs.startAddress;
                }
                if (lhs.endAddress != rhs.endAddress)
                {
                    return lhs.endAddress < rhs.endAddress;
                }
                return lhs.moduleName < rhs.moduleName;
            });

            for (const auto& region : allRegions)
            {
                if (merged.regions.empty()
                    || merged.regions.back().moduleName != region.moduleName
                    || merged.regions.back().endAddress < region.startAddress)
                {
                    merged.regions.push_back(region);
                    continue;
                }
                merged.regions.back().endAddress =
                    std::max(merged.regions.back().endAddress, region.endAddress);
            }
        }

        const bool extendedTop = incomingRange->first < currentRange->first;
        const bool extendedBottom = incomingRange->second > currentRange->second;

        if (merged.data.size() > MAX_SLIDING_WINDOW_BYTES)
        {
            const auto trimCount = merged.data.size() - MAX_SLIDING_WINDOW_BYTES;
            const bool trimFront = extendedBottom && !extendedTop;

            if (trimFront)
            {
                merged.data.erase(merged.data.begin(), merged.data.begin() + static_cast<std::ptrdiff_t>(trimCount));
                merged.readable.erase(merged.readable.begin(), merged.readable.begin() + static_cast<std::ptrdiff_t>(trimCount));
                merged.modified.erase(merged.modified.begin(), merged.modified.begin() + static_cast<std::ptrdiff_t>(trimCount));

                if (trimCount > std::numeric_limits<std::uint64_t>::max() - merged.baseAddress)
                {
                    m_memoryBlock = block;
                    return;
                }
                merged.baseAddress += trimCount;
            }
            else
            {
                merged.data.resize(MAX_SLIDING_WINDOW_BYTES);
                merged.readable.resize(MAX_SLIDING_WINDOW_BYTES);
                merged.modified.resize(MAX_SLIDING_WINDOW_BYTES);
            }

            if (merged.data.size() > std::numeric_limits<std::uint64_t>::max() - merged.baseAddress)
            {
                m_memoryBlock = block;
                return;
            }
            const auto windowEnd = merged.baseAddress + static_cast<std::uint64_t>(merged.data.size());
            std::erase_if(merged.regions, [&](const auto& region)
            {
                return region.endAddress <= merged.baseAddress || region.startAddress >= windowEnd;
            });
            for (auto& region : merged.regions)
            {
                if (region.startAddress < merged.baseAddress)
                {
                    region.startAddress = merged.baseAddress;
                }
                if (region.endAddress > windowEnd)
                {
                    region.endAddress = windowEnd;
                }
            }
        }

        m_memoryBlock = std::move(merged);
    }

    void HexEditorPanel::on_context_menu(wxContextMenuEvent& event)
    {
        auto screenPoint = event.GetPosition();
        if (screenPoint == wxDefaultPosition)
        {
            screenPoint = wxGetMousePosition();
        }

        const auto clientPoint = m_canvas->ScreenToClient(screenPoint);
        const auto hit = m_canvas->hit_test_byte(clientPoint);
        if (hit.has_value())
        {
            set_cursor(hit->first, hit->second, std::nullopt, false);
        }

        wxMenu menu{};
        menu.Append(CONTEXT_MENU_COPY_HEX, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.copyHex")));
        menu.Append(CONTEXT_MENU_COPY_ASCII, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.copyAscii")));
        menu.Append(CONTEXT_MENU_COPY_ADDRESS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.copyAddress")));
        menu.AppendSeparator();
        menu.Append(CONTEXT_MENU_GOTO_ADDRESS, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.gotoAddress")));

        const auto hasRow = get_context_row_index().has_value();
        const auto hasSelection = get_selection_bounds().has_value();
        menu.Enable(CONTEXT_MENU_COPY_HEX, hasRow || hasSelection);
        menu.Enable(CONTEXT_MENU_COPY_ASCII, hasRow || hasSelection);
        menu.Enable(CONTEXT_MENU_COPY_ADDRESS, hasRow);
        menu.Enable(CONTEXT_MENU_GOTO_ADDRESS, hasRow);

        m_canvas->PopupMenu(&menu, clientPoint);
    }

    void HexEditorPanel::on_canvas_key_down(wxKeyEvent& event)
    {
        const auto keyCode = event.GetKeyCode();
        const auto ctrlDown = event.ControlDown();
        const auto shiftDown = event.ShiftDown();

        if (ctrlDown && (keyCode == 'g' || keyCode == 'G'))
        {
            m_addressInput->SetFocus();
            m_addressInput->SelectAll();
            return;
        }
        if (ctrlDown && (keyCode == 'c' || keyCode == 'C'))
        {
            wxCommandEvent commandEvent{};
            if (shiftDown)
            {
                on_copy_ascii(commandEvent);
            }
            else
            {
                on_copy_hex(commandEvent);
            }
            return;
        }
        if (ctrlDown && (keyCode == 'z' || keyCode == 'Z'))
        {
            undo_pending_edits();
            return;
        }

        switch (keyCode)
        {
            case WXK_LEFT: move_cursor(-1, shiftDown); return;
            case WXK_RIGHT: move_cursor(1, shiftDown); return;
            case WXK_UP: move_cursor(-static_cast<std::ptrdiff_t>(MemoryViewValues::BYTES_PER_ROW), shiftDown); return;
            case WXK_DOWN: move_cursor(static_cast<std::ptrdiff_t>(MemoryViewValues::BYTES_PER_ROW), shiftDown); return;
            case WXK_PAGEUP:
                move_cursor(
                    -static_cast<std::ptrdiff_t>(m_canvas->get_visible_rows() * MemoryViewValues::BYTES_PER_ROW),
                    shiftDown);
                return;
            case WXK_PAGEDOWN:
                move_cursor(
                    static_cast<std::ptrdiff_t>(m_canvas->get_visible_rows() * MemoryViewValues::BYTES_PER_ROW),
                    shiftDown);
                return;
            case WXK_HOME:
            {
                if (m_memoryBlock.data.empty())
                {
                    return;
                }

                const auto previous = m_cursorByteIndex.value_or(0);
                std::size_t target{};
                if (!m_cursorByteIndex.has_value())
                {
                    target = 0;
                }
                else if (ctrlDown)
                {
                    target = 0;
                }
                else
                {
                    target = (*m_cursorByteIndex / MemoryViewValues::BYTES_PER_ROW) * MemoryViewValues::BYTES_PER_ROW;
                }

                m_pendingLowNibble = false;
                set_cursor(target, m_asciiInputMode);
                if (shiftDown)
                {
                    if (!m_selectionAnchorByteIndex.has_value())
                    {
                        m_selectionAnchorByteIndex = previous;
                    }
                    m_selectionEndByteIndex = target;
                    refresh_display();
                }
                else
                {
                    clear_selection();
                    refresh_display();
                }
                return;
            }
            case WXK_END:
            {
                if (m_memoryBlock.data.empty())
                {
                    return;
                }

                const auto lastByte = m_memoryBlock.data.size() - 1;
                const auto previous = m_cursorByteIndex.value_or(lastByte);
                std::size_t target{lastByte};
                if (!m_cursorByteIndex.has_value())
                {
                    target = lastByte;
                }
                else if (ctrlDown)
                {
                    target = lastByte;
                }
                else
                {
                    const auto rowStart =
                        (*m_cursorByteIndex / MemoryViewValues::BYTES_PER_ROW) * MemoryViewValues::BYTES_PER_ROW;
                    target = std::min<std::size_t>(rowStart + MemoryViewValues::BYTES_PER_ROW - 1, lastByte);
                }

                m_pendingLowNibble = false;
                set_cursor(target, m_asciiInputMode);
                if (shiftDown)
                {
                    if (!m_selectionAnchorByteIndex.has_value())
                    {
                        m_selectionAnchorByteIndex = previous;
                    }
                    m_selectionEndByteIndex = target;
                    refresh_display();
                }
                else
                {
                    clear_selection();
                    refresh_display();
                }
                return;
            }
            case WXK_TAB:
                m_asciiInputMode = !m_asciiInputMode;
                m_pendingLowNibble = false;
                refresh_display();
                return;
            case WXK_RETURN:
            case WXK_NUMPAD_ENTER:
                std::ignore = commit_pending_edits();
                return;
            case WXK_ESCAPE:
                discard_pending_edits();
                return;
            default:
                break;
        }

        if (m_memoryBlock.data.empty())
        {
            event.Skip();
            return;
        }

        if (!m_cursorByteIndex.has_value())
        {
            set_cursor(0, m_asciiInputMode);
        }
        if (!m_cursorByteIndex.has_value())
        {
            event.Skip();
            return;
        }

        const auto byteIndex = m_cursorByteIndex.value();
        if (m_asciiInputMode)
        {
            if (keyCode >= 32 && keyCode < 127)
            {
                clear_selection();
                if (apply_pending_edit(byteIndex, static_cast<std::uint8_t>(keyCode)))
                {
                    move_cursor(1);
                }
                return;
            }

            event.Skip();
            return;
        }

        const auto nibble = key_to_hex_nibble(keyCode);
        if (nibble < 0)
        {
            event.Skip();
            return;
        }
        clear_selection();

        const auto pendingIt = m_pendingEdits.find(byteIndex);
        auto currentValue = pendingIt != m_pendingEdits.end()
            ? pendingIt->second
            : m_memoryBlock.data[byteIndex];

        std::uint8_t updatedValue{};
        if (m_pendingLowNibble)
        {
            updatedValue = static_cast<std::uint8_t>((currentValue & 0xF0u) | static_cast<std::uint8_t>(nibble));
        }
        else
        {
            updatedValue = static_cast<std::uint8_t>((static_cast<std::uint8_t>(nibble) << 4) | (currentValue & 0x0Fu));
        }

        if (apply_pending_edit(byteIndex, updatedValue))
        {
            if (m_pendingLowNibble)
            {
                m_pendingLowNibble = false;
                move_cursor(1);
            }
            else
            {
                m_pendingLowNibble = true;
                refresh_display();
            }
        }
    }

    void HexEditorPanel::on_copy_hex([[maybe_unused]] wxCommandEvent& event)
    {
        const auto selectionText = build_selection_hex_text();
        if (!selectionText.empty())
        {
            std::ignore = copy_text_to_clipboard(selectionText);
            return;
        }

        const auto rowIndex = get_context_row_index();
        if (!rowIndex.has_value())
        {
            return;
        }

        if (!copy_text_to_clipboard(build_row_hex_text(*rowIndex)))
        {
            return;
        }
    }

    void HexEditorPanel::on_copy_ascii([[maybe_unused]] wxCommandEvent& event)
    {
        const auto selectionText = build_selection_ascii_text();
        if (!selectionText.empty())
        {
            std::ignore = copy_text_to_clipboard(selectionText);
            return;
        }

        const auto rowIndex = get_context_row_index();
        if (!rowIndex.has_value())
        {
            return;
        }

        if (!copy_text_to_clipboard(build_row_ascii_text(*rowIndex)))
        {
            return;
        }
    }

    void HexEditorPanel::on_copy_address([[maybe_unused]] wxCommandEvent& event)
    {
        const auto address = get_context_row_address();
        if (!address.has_value())
        {
            return;
        }

        if (!copy_text_to_clipboard(wxString::FromUTF8(fmt::format("0x{:X}", *address))))
        {
            return;
        }
    }

    void HexEditorPanel::on_goto_row_address([[maybe_unused]] wxCommandEvent& event)
    {
        const auto address = get_context_row_address();
        if (!address.has_value() || !m_navigateCallback)
        {
            return;
        }

        set_address(*address);
        m_forceReplaceOnNextUpdate = true;
        m_lastContinuousFetchAddress.reset();
        m_navigateCallback(*address);
    }

    void HexEditorPanel::edit_row(const std::size_t rowIndex)
    {
        if (!m_writeCallback)
        {
            return;
        }

        const auto rows =
            (m_memoryBlock.data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;
        if (rowIndex >= rows)
        {
            return;
        }

        const auto rowOffset = rowIndex * MemoryViewValues::BYTES_PER_ROW;
        const auto rowByteCount =
            std::min<std::size_t>(MemoryViewValues::BYTES_PER_ROW, m_memoryBlock.data.size() - rowOffset);
        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            const auto byteIndex = rowOffset + i;
            if (!is_byte_editable(byteIndex))
            {
                wxMessageBox(
                    wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.unreadable")),
                    wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                    wxOK | wxICON_ERROR,
                    this);
                return;
            }
        }

        std::string currentRow{};
        currentRow.reserve(rowByteCount * 3);
        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            if (i != 0)
            {
                currentRow += ' ';
            }
            currentRow += fmt::format("{:02X}", m_memoryBlock.data[rowOffset + i]);
        }

        wxTextEntryDialog dialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.editRowPrompt")),
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.editRowTitle")),
            currentRow);

        if (dialog.ShowModal() != wxID_OK)
        {
            wxLogStatus(this, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.discardEdits")));
            return;
        }

        const auto parsed = parse_hex_bytes(dialog.GetValue().ToStdString());
        if (!parsed.has_value())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.invalidHexInput")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR,
                this);
            return;
        }

        if (parsed->size() != rowByteCount)
        {
            wxMessageBox(
                wxString::FromUTF8(fmt::format(
                    fmt::runtime(m_languageService.fetch_translation("debugger.memory.expectedRowBytes")),
                    rowByteCount,
                    parsed->size())),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR,
                this);
            return;
        }

        m_pendingUndoSnapshots.push_back(m_pendingEdits);
        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            const auto byteIndex = rowOffset + i;
            const auto value = parsed.value()[i];
            if (value == m_memoryBlock.data[byteIndex])
            {
                m_pendingEdits.erase(byteIndex);
            }
            else
            {
                m_pendingEdits[byteIndex] = value;
            }
        }

        wxLogStatus(
            this,
            wxString::FromUTF8(fmt::format(
                fmt::runtime(m_languageService.fetch_translation("debugger.hexEditor.pendingEdits")),
                parsed->size())));

        std::ignore = commit_pending_edits();
    }

    void HexEditorPanel::move_cursor(const std::ptrdiff_t delta, const bool extendSelection)
    {
        if (m_memoryBlock.data.empty())
        {
            return;
        }

        const auto currentByteIndex = m_cursorByteIndex.value_or(0);
        auto target = static_cast<std::ptrdiff_t>(currentByteIndex) + delta;
        if (target < 0)
        {
            target = 0;
        }

        const auto lastByte = static_cast<std::ptrdiff_t>(m_memoryBlock.data.size() - 1);
        if (target > lastByte)
        {
            target = lastByte;
        }

        m_pendingLowNibble = false;
        const auto targetIndex = static_cast<std::size_t>(target);
        set_cursor(targetIndex, m_asciiInputMode, delta);

        if (extendSelection)
        {
            if (!m_selectionAnchorByteIndex.has_value())
            {
                m_selectionAnchorByteIndex = currentByteIndex;
            }
            m_selectionEndByteIndex = targetIndex;
            refresh_display();
        }
        else
        {
            clear_selection();
            refresh_display();
        }
    }

    void HexEditorPanel::set_cursor(
        const std::size_t byteIndex,
        const bool asciiMode,
        const std::optional<std::ptrdiff_t> navigationDelta,
        const bool allowContinuousFetch)
    {
        if (byteIndex >= m_memoryBlock.data.size())
        {
            return;
        }

        m_cursorByteIndex = byteIndex;
        m_asciiInputMode = asciiMode;
        m_selectedRowIndex = byteIndex / MemoryViewValues::BYTES_PER_ROW;
        m_contextRowIndex = m_selectedRowIndex;
        m_canvas->ensure_row_visible(*m_selectedRowIndex);
        if (allowContinuousFetch)
        {
            check_continuous_fetch(navigationDelta);
        }
        refresh_display();
    }

    bool HexEditorPanel::apply_pending_edit(const std::size_t byteIndex, const std::uint8_t value)
    {
        if (!is_byte_editable(byteIndex))
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.unreadable")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR,
                this);
            return false;
        }

        m_pendingUndoSnapshots.push_back(m_pendingEdits);
        if (m_pendingUndoSnapshots.size() > 256)
        {
            m_pendingUndoSnapshots.erase(m_pendingUndoSnapshots.begin());
        }

        if (value == m_memoryBlock.data[byteIndex])
        {
            m_pendingEdits.erase(byteIndex);
        }
        else
        {
            m_pendingEdits[byteIndex] = value;
        }

        wxLogStatus(
            this,
            wxString::FromUTF8(fmt::format(
                fmt::runtime(m_languageService.fetch_translation("debugger.hexEditor.pendingEdits")),
                m_pendingEdits.size())));
        refresh_display();
        return true;
    }

    bool HexEditorPanel::commit_pending_edits()
    {
        if (m_pendingEdits.empty())
        {
            return true;
        }
        if (!m_writeCallback)
        {
            return false;
        }

        std::vector<std::size_t> indexes{};
        indexes.reserve(m_pendingEdits.size());
        for (const auto& [index, value] : m_pendingEdits)
        {
            std::ignore = value;
            indexes.push_back(index);
        }
        std::ranges::sort(indexes);

        std::size_t rangeStart = indexes.front();
        std::size_t previous = rangeStart;

        auto flush_range = [&](const std::size_t start, const std::size_t endInclusive) -> bool
        {
            const auto count = endInclusive - start + 1;
            if (start > std::numeric_limits<std::uint64_t>::max() - m_memoryBlock.baseAddress)
            {
                wxMessageBox(
                    wxString::FromUTF8(fmt::format(
                        fmt::runtime(m_languageService.fetch_translation("debugger.memory.writeFailed")),
                        static_cast<int>(StatusCode::STATUS_ERROR_INVALID_PARAMETER))),
                    wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                    wxOK | wxICON_ERROR,
                    this);
                return false;
            }

            const auto address = m_memoryBlock.baseAddress + start;
            std::vector<std::uint8_t> payload{};
            payload.reserve(count);
            for (std::size_t i{}; i < count; ++i)
            {
                const auto index = start + i;
                if (const auto it = m_pendingEdits.find(index); it != m_pendingEdits.end())
                {
                    payload.push_back(it->second);
                }
                else
                {
                    payload.push_back(m_memoryBlock.data[index]);
                }
            }

            const auto status = m_writeCallback(address, payload);
            if (status != StatusCode::STATUS_OK)
            {
                wxMessageBox(
                    wxString::FromUTF8(fmt::format(
                        fmt::runtime(m_languageService.fetch_translation("debugger.memory.writeFailed")),
                        static_cast<int>(status))),
                    wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                    wxOK | wxICON_ERROR,
                    this);
                return false;
            }

            return true;
        };

        for (std::size_t i{1}; i < indexes.size(); ++i)
        {
            const auto current = indexes[i];
            if (current == previous + 1)
            {
                previous = current;
                continue;
            }

            if (!flush_range(rangeStart, previous))
            {
                return false;
            }

            rangeStart = current;
            previous = current;
        }
        if (!flush_range(rangeStart, previous))
        {
            return false;
        }

        if (m_memoryBlock.modified.size() != m_memoryBlock.data.size())
        {
            m_memoryBlock.modified.resize(m_memoryBlock.data.size(), false);
        }
        if (m_memoryBlock.readable.size() != m_memoryBlock.data.size())
        {
            m_memoryBlock.readable.resize(m_memoryBlock.data.size(), false);
        }

        for (const auto& [index, value] : m_pendingEdits)
        {
            m_memoryBlock.data[index] = value;
            m_memoryBlock.modified[index] = true;
            m_memoryBlock.readable[index] = true;
        }

        m_pendingEdits.clear();
        m_pendingUndoSnapshots.clear();
        m_pendingLowNibble = false;
        refresh_display();
        wxLogStatus(this, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.commitEdits")));
        return true;
    }

    void HexEditorPanel::discard_pending_edits()
    {
        m_pendingEdits.clear();
        m_pendingUndoSnapshots.clear();
        m_pendingLowNibble = false;
        refresh_display();
        wxLogStatus(this, wxString::FromUTF8(m_languageService.fetch_translation("debugger.hexEditor.discardEdits")));
    }

    void HexEditorPanel::undo_pending_edits()
    {
        if (m_pendingUndoSnapshots.empty())
        {
            return;
        }

        m_pendingEdits = std::move(m_pendingUndoSnapshots.back());
        m_pendingUndoSnapshots.pop_back();
        m_pendingLowNibble = false;
        refresh_display();
        wxLogStatus(
            this,
            wxString::FromUTF8(fmt::format(
                fmt::runtime(m_languageService.fetch_translation("debugger.hexEditor.pendingEdits")),
                m_pendingEdits.size())));
    }

    bool HexEditorPanel::is_byte_editable(const std::size_t byteIndex) const
    {
        if (byteIndex >= m_memoryBlock.data.size())
        {
            return false;
        }
        if (byteIndex < m_memoryBlock.readable.size() && !m_memoryBlock.readable[byteIndex])
        {
            return false;
        }
        return true;
    }

    void HexEditorPanel::clear_selection()
    {
        m_selectionAnchorByteIndex.reset();
        m_selectionEndByteIndex.reset();
        m_selectionDragging = false;
    }

    std::optional<std::pair<std::size_t, std::size_t>> HexEditorPanel::get_selection_bounds() const
    {
        if (!m_selectionAnchorByteIndex.has_value() || !m_selectionEndByteIndex.has_value())
        {
            return std::nullopt;
        }

        auto start = m_selectionAnchorByteIndex.value();
        auto end = m_selectionEndByteIndex.value();
        if (start > end)
        {
            std::swap(start, end);
        }

        if (start >= m_memoryBlock.data.size())
        {
            return std::nullopt;
        }
        end = std::min(end, m_memoryBlock.data.size() - 1);
        return std::pair<std::size_t, std::size_t>{start, end};
    }

    wxString HexEditorPanel::build_selection_hex_text() const
    {
        const auto bounds = get_selection_bounds();
        if (!bounds.has_value())
        {
            return {};
        }

        std::string text{};
        text.reserve((bounds->second - bounds->first + 1) * 3);

        for (std::size_t byteIndex = bounds->first; byteIndex <= bounds->second; ++byteIndex)
        {
            if (byteIndex != bounds->first)
            {
                text += ' ';
            }

            const auto isReadable = byteIndex < m_memoryBlock.readable.size()
                ? m_memoryBlock.readable[byteIndex]
                : true;
            if (!isReadable)
            {
                text += "??";
                continue;
            }

            std::uint8_t value = m_memoryBlock.data[byteIndex];
            if (const auto it = m_pendingEdits.find(byteIndex); it != m_pendingEdits.end())
            {
                value = it->second;
            }
            text += fmt::format("{:02X}", value);
        }

        return wxString::FromUTF8(text);
    }

    wxString HexEditorPanel::build_selection_ascii_text() const
    {
        const auto bounds = get_selection_bounds();
        if (!bounds.has_value())
        {
            return {};
        }

        std::string text{};
        text.reserve(bounds->second - bounds->first + 1);

        for (std::size_t byteIndex = bounds->first; byteIndex <= bounds->second; ++byteIndex)
        {
            const auto isReadable = byteIndex < m_memoryBlock.readable.size()
                ? m_memoryBlock.readable[byteIndex]
                : true;
            if (!isReadable)
            {
                text += '?';
                continue;
            }

            std::uint8_t value = m_memoryBlock.data[byteIndex];
            if (const auto it = m_pendingEdits.find(byteIndex); it != m_pendingEdits.end())
            {
                value = it->second;
            }

            text += (value >= 32 && value < 127)
                ? static_cast<char>(value)
                : '.';
        }

        return wxString::FromUTF8(text);
    }

    std::optional<std::size_t> HexEditorPanel::get_context_row_index() const
    {
        if (m_contextRowIndex.has_value())
        {
            return m_contextRowIndex;
        }
        if (m_selectedRowIndex.has_value())
        {
            return m_selectedRowIndex;
        }
        if (m_cursorByteIndex.has_value())
        {
            return m_cursorByteIndex.value() / MemoryViewValues::BYTES_PER_ROW;
        }
        return std::nullopt;
    }

    std::optional<std::uint64_t> HexEditorPanel::get_context_row_address() const
    {
        const auto rowIndex = get_context_row_index();
        if (!rowIndex.has_value())
        {
            return std::nullopt;
        }

        const auto rowOffset = *rowIndex * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock.data.size())
        {
            return std::nullopt;
        }
        if (m_memoryBlock.baseAddress > std::numeric_limits<std::uint64_t>::max() - rowOffset)
        {
            return std::nullopt;
        }

        return m_memoryBlock.baseAddress + rowOffset;
    }

    wxString HexEditorPanel::build_row_hex_text(const std::size_t rowIndex) const
    {
        const auto rowOffset = rowIndex * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock.data.size())
        {
            return {};
        }

        const auto rowByteCount =
            std::min<std::size_t>(MemoryViewValues::BYTES_PER_ROW, m_memoryBlock.data.size() - rowOffset);
        std::string rowText{};
        rowText.reserve(rowByteCount * 3);

        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            if (i != 0)
            {
                rowText += ' ';
            }
            const auto byteIndex = rowOffset + i;
            const auto isReadable = byteIndex < m_memoryBlock.readable.size()
                ? m_memoryBlock.readable[byteIndex]
                : true;
            if (!isReadable)
            {
                rowText += "??";
                continue;
            }

            std::uint8_t value = m_memoryBlock.data[byteIndex];
            if (const auto it = m_pendingEdits.find(byteIndex); it != m_pendingEdits.end())
            {
                value = it->second;
            }
            rowText += fmt::format("{:02X}", value);
        }

        return wxString::FromUTF8(rowText);
    }

    wxString HexEditorPanel::build_row_ascii_text(const std::size_t rowIndex) const
    {
        const auto rowOffset = rowIndex * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock.data.size())
        {
            return {};
        }

        const auto rowByteCount =
            std::min<std::size_t>(MemoryViewValues::BYTES_PER_ROW, m_memoryBlock.data.size() - rowOffset);
        std::string rowText{};
        rowText.reserve(rowByteCount);

        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            const auto byteIndex = rowOffset + i;
            const auto isReadable = byteIndex < m_memoryBlock.readable.size()
                ? m_memoryBlock.readable[byteIndex]
                : true;
            if (!isReadable)
            {
                rowText += '?';
                continue;
            }

            std::uint8_t value = m_memoryBlock.data[byteIndex];
            if (const auto it = m_pendingEdits.find(byteIndex); it != m_pendingEdits.end())
            {
                value = it->second;
            }

            rowText += (value >= 32 && value < 127)
                ? static_cast<char>(value)
                : '.';
        }

        return wxString::FromUTF8(rowText);
    }

    bool HexEditorPanel::copy_text_to_clipboard(const wxString& text) const
    {
        if (text.empty())
        {
            return false;
        }
        if (!wxTheClipboard->Open())
        {
            return false;
        }

        wxTheClipboard->SetData(new wxTextDataObject(text));
        wxTheClipboard->Close();
        return true;
    }

    void HexEditorPanel::refresh_display()
    {
        m_canvas->set_memory_block(&m_memoryBlock);
        m_canvas->set_pending_edits(&m_pendingEdits);
        m_canvas->set_selection_range(get_selection_bounds());
        m_canvas->set_selected_row(m_selectedRowIndex);
        m_canvas->set_cursor(m_cursorByteIndex, m_asciiInputMode);
        update_region_tooltip(m_cursorByteIndex);
    }

    void HexEditorPanel::update_region_tooltip(const std::optional<std::size_t> byteIndex)
    {
        auto baseInfo = m_languageService.fetch_translation("debugger.hexEditor.editMode");
        if (baseInfo.empty())
        {
            baseInfo = "Hex editor";
        }
        if (!byteIndex.has_value()
            || *byteIndex >= m_memoryBlock.data.size()
            || *byteIndex > std::numeric_limits<std::uint64_t>::max() - m_memoryBlock.baseAddress)
        {
            m_canvas->SetToolTip(wxString::FromUTF8(baseInfo));
            if (m_regionInfoText != nullptr)
            {
                m_regionInfoText->SetLabel(wxString::FromUTF8(baseInfo));
            }
            return;
        }

        const auto address = m_memoryBlock.baseAddress + *byteIndex;
        for (const auto& region : m_memoryBlock.regions)
        {
            if (address < region.startAddress || address >= region.endAddress)
            {
                continue;
            }

            const auto module = region.moduleName.empty()
                ? m_languageService.fetch_translation("debugger.ui.unknown")
                : region.moduleName;
            const auto regionInfoText = fmt::format(
                "Region: 0x{:X}-0x{:X}  Module: {}",
                region.startAddress,
                region.endAddress,
                module);
            m_canvas->SetToolTip(wxString::FromUTF8(fmt::format(
                "{}\n{}",
                baseInfo,
                regionInfoText)));
            if (m_regionInfoText != nullptr)
            {
                m_regionInfoText->SetLabel(wxString::FromUTF8(regionInfoText));
            }
            return;
        }

        m_canvas->SetToolTip(wxString::FromUTF8(baseInfo));
        if (m_regionInfoText != nullptr)
        {
            m_regionInfoText->SetLabel(wxString::FromUTF8(baseInfo));
        }
    }
}
