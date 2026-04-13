//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/memorypanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

#include <wx/msgdlg.h>
#include <wx/log.h>
#include <wx/settings.h>
#include <wx/textdlg.h>

#include <bit>
#include <algorithm>
#include <cctype>
#include <cmath>
#include <limits>
#include <string>

namespace Vertex::View::Debugger
{
    namespace
    {
        [[nodiscard]] wxColour adjust_color(const wxColour& color, const int delta)
        {
            auto clamp_channel = [delta](const int value) -> unsigned char
            {
                return static_cast<unsigned char>(std::clamp(value + delta, 0, 255));
            };

            return wxColour{
                clamp_channel(color.Red()),
                clamp_channel(color.Green()),
                clamp_channel(color.Blue())};
        }
    }

    MemoryVirtualListCtrl::MemoryVirtualListCtrl(wxWindow* parent)
        : wxListCtrl(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_VIRTUAL)
    {
    }

    void MemoryVirtualListCtrl::set_memory_block(const ::Vertex::Debugger::MemoryBlock* const memoryBlock)
    {
        m_memoryBlock = memoryBlock;

        const auto defaultTextColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        const auto baseBackground = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        const auto luminance =
            (static_cast<int>(baseBackground.Red()) +
             static_cast<int>(baseBackground.Green()) +
             static_cast<int>(baseBackground.Blue())) / 3;
        const bool darkBase = luminance < 128;
        const auto mappedRegionBackgroundA = adjust_color(baseBackground, darkBase ? 10 : -6);
        const auto mappedRegionBackgroundB = adjust_color(baseBackground, darkBase ? 16 : -12);
        const auto mappedBoundaryBackgroundA = adjust_color(mappedRegionBackgroundA, darkBase ? 8 : -10);
        const auto mappedBoundaryBackgroundB = adjust_color(mappedRegionBackgroundB, darkBase ? 8 : -10);

        m_mappedAttrA.SetTextColour(defaultTextColor);
        m_mappedAttrA.SetBackgroundColour(mappedRegionBackgroundA);
        m_mappedAttrB.SetTextColour(defaultTextColor);
        m_mappedAttrB.SetBackgroundColour(mappedRegionBackgroundB);
        m_boundaryAttrA.SetTextColour(defaultTextColor);
        m_boundaryAttrA.SetBackgroundColour(mappedBoundaryBackgroundA);
        m_boundaryAttrB.SetTextColour(defaultTextColor);
        m_boundaryAttrB.SetBackgroundColour(mappedBoundaryBackgroundB);

        rebuild_row_region_cache();
    }

    wxListItemAttr* MemoryVirtualListCtrl::OnGetItemAttr(const long item) const
    {
        if (item < 0 || m_memoryBlock == nullptr)
        {
            return nullptr;
        }

        const auto row = static_cast<std::size_t>(item);
        if (row >= m_rowRegionIndices.size())
        {
            return nullptr;
        }

        const auto regionIndex = m_rowRegionIndices[row];
        if (regionIndex < 0)
        {
            return nullptr;
        }

        const bool isBoundaryRow = row < m_rowStartsAtBoundary.size() && m_rowStartsAtBoundary[row];
        const bool useAltPalette = (regionIndex % 2) != 0;

        if (isBoundaryRow)
        {
            return useAltPalette ? &m_boundaryAttrB : &m_boundaryAttrA;
        }
        return useAltPalette ? &m_mappedAttrB : &m_mappedAttrA;
    }

    void MemoryVirtualListCtrl::rebuild_row_region_cache()
    {
        m_rowRegionIndices.clear();
        m_rowStartsAtBoundary.clear();

        if (m_memoryBlock == nullptr || m_memoryBlock->data.empty())
        {
            return;
        }

        const auto rowCount =
            (m_memoryBlock->data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW;
        m_rowRegionIndices.assign(rowCount, -1);
        m_rowStartsAtBoundary.assign(rowCount, false);

        if (m_memoryBlock->regions.empty())
        {
            return;
        }

        const auto blockStart = m_memoryBlock->baseAddress;
        const auto blockSize = static_cast<std::uint64_t>(m_memoryBlock->data.size());
        const auto blockEnd = blockSize <= std::numeric_limits<std::uint64_t>::max() - blockStart
            ? blockStart + blockSize
            : std::numeric_limits<std::uint64_t>::max();

        std::size_t regionCursor{};
        for (std::size_t row{}; row < rowCount; ++row)
        {
            const auto rowOffset = row * MemoryViewValues::BYTES_PER_ROW;
            if (rowOffset > std::numeric_limits<std::uint64_t>::max() - blockStart)
            {
                continue;
            }

            const auto rowAddress = blockStart + static_cast<std::uint64_t>(rowOffset);
            while (regionCursor < m_memoryBlock->regions.size()
                && m_memoryBlock->regions[regionCursor].endAddress <= rowAddress)
            {
                ++regionCursor;
            }

            if (regionCursor >= m_memoryBlock->regions.size())
            {
                continue;
            }

            const auto& region = m_memoryBlock->regions[regionCursor];
            if (rowAddress >= region.startAddress
                && rowAddress < region.endAddress
                && regionCursor <= static_cast<std::size_t>(std::numeric_limits<int>::max()))
            {
                m_rowRegionIndices[row] = static_cast<int>(regionCursor);
            }
        }

        for (const auto& region : m_memoryBlock->regions)
        {
            if (region.startAddress < blockStart || region.startAddress >= blockEnd)
            {
                continue;
            }

            const auto offset = static_cast<std::size_t>(region.startAddress - blockStart);
            const auto row = offset / MemoryViewValues::BYTES_PER_ROW;
            if (row < m_rowStartsAtBoundary.size())
            {
                m_rowStartsAtBoundary[row] = true;
            }
        }
    }

    wxString MemoryVirtualListCtrl::OnGetItemText(const long item, const long column) const
    {
        if (m_memoryBlock == nullptr)
        {
            return EMPTY_STRING;
        }

        const auto row = static_cast<std::size_t>(item);
        const auto rowOffset = row * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock->data.size())
        {
            return EMPTY_STRING;
        }

        if (column == 0)
        {
            const auto rowAddress = m_memoryBlock->baseAddress + rowOffset;
            return wxString::FromUTF8(fmt::format("{:X}", rowAddress));
        }

        if (column == 1)
        {
            std::string hexText{};
            hexText.reserve(MemoryViewValues::BYTES_PER_ROW * 3);
            for (std::size_t col{}; col < MemoryViewValues::BYTES_PER_ROW; ++col)
            {
                const auto byteIndex = rowOffset + col;
                if (byteIndex < m_memoryBlock->data.size())
                {
                    const bool isReadable = byteIndex < m_memoryBlock->readable.size()
                        ? m_memoryBlock->readable[byteIndex]
                        : true;
                    const bool isModified = byteIndex < m_memoryBlock->modified.size()
                        ? m_memoryBlock->modified[byteIndex]
                        : false;
                    const auto byte = m_memoryBlock->data[byteIndex];

                    if (!isReadable)
                    {
                        hexText += "?? ";
                    }
                    else if (isModified)
                    {
                        hexText += fmt::format("{:02x} ", byte);
                    }
                    else
                    {
                        hexText += fmt::format("{:02X} ", byte);
                    }
                }
                else
                {
                    hexText += "   ";
                }
            }

            return wxString::FromUTF8(hexText);
        }

        if (column == 2)
        {
            std::string asciiText{};
            asciiText.reserve(MemoryViewValues::BYTES_PER_ROW);
            for (std::size_t col{}; col < MemoryViewValues::BYTES_PER_ROW; ++col)
            {
                const auto byteIndex = rowOffset + col;
                if (byteIndex < m_memoryBlock->data.size())
                {
                    const bool isReadable = byteIndex < m_memoryBlock->readable.size()
                        ? m_memoryBlock->readable[byteIndex]
                        : true;
                    const auto byte = m_memoryBlock->data[byteIndex];

                    if (!isReadable)
                    {
                        asciiText += '?';
                    }
                    else if (byte >= 32 && byte < 127)
                    {
                        asciiText += static_cast<char>(byte);
                    }
                    else
                    {
                        asciiText += '.';
                    }
                }
                else
                {
                    asciiText += ' ';
                }
            }

            return wxString::FromUTF8(asciiText);
        }

        return EMPTY_STRING;
    }

    MemoryPanel::MemoryPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void MemoryPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_addressBarSizer = new wxBoxSizer(wxHORIZONTAL);
        m_interpretationSizer = new wxBoxSizer(wxVERTICAL);

        m_addressInput = new wxTextCtrl(this, wxID_ANY, "0x", wxDefaultPosition,
                                         wxSize(FromDIP(150), -1), wxTE_PROCESS_ENTER);
        m_goButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.go")));

        m_memoryList = new MemoryVirtualListCtrl(this);
        m_memoryList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_memoryList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(100));
        m_memoryList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnHex")), wxLIST_FORMAT_LEFT, FromDIP(250));
        m_memoryList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnAscii")), wxLIST_FORMAT_LEFT, FromDIP(100));

        m_interpretationHeader = new wxStaticText(
            this,
            wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.interpretationTitle")));
        m_interpretationValue = new wxStaticText(
            this,
            wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.interpretationNoSelection")));
        m_interpretationValue->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    }

    void MemoryPanel::layout_controls()
    {
        m_addressBarSizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.address"))),
                                StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_addressInput, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_goButton, StandardWidgetValues::NO_PROPORTION);

        m_mainSizer->Add(m_addressBarSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_memoryList, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                          StandardWidgetValues::STANDARD_BORDER);
        m_interpretationSizer->Add(m_interpretationHeader, StandardWidgetValues::NO_PROPORTION, wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_interpretationSizer->Add(m_interpretationValue, StandardWidgetValues::NO_PROPORTION, wxEXPAND);
        m_mainSizer->Add(m_interpretationSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void MemoryPanel::bind_events()
    {
        m_goButton->Bind(wxEVT_BUTTON, &MemoryPanel::on_goto_address, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &MemoryPanel::on_goto_address, this);
        m_memoryList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &MemoryPanel::on_item_activated, this);
        m_memoryList->Bind(wxEVT_LIST_ITEM_SELECTED, &MemoryPanel::on_item_selected, this);
        m_memoryList->Bind(wxEVT_LIST_ITEM_DESELECTED, &MemoryPanel::on_item_deselected, this);
    }

    void MemoryPanel::update_memory(const ::Vertex::Debugger::MemoryBlock& block)
    {
        m_memoryBlock = block;
        m_memoryList->set_memory_block(&m_memoryBlock);

        const auto rowCount = static_cast<long>((block.data.size() + MemoryViewValues::BYTES_PER_ROW - 1) / MemoryViewValues::BYTES_PER_ROW);
        m_memoryList->SetItemCount(rowCount);
        if (rowCount > 0)
        {
            m_memoryList->RefreshItems(0, rowCount - 1);
        }
        else
        {
            m_memoryList->Refresh();
        }

        update_data_interpretation(std::nullopt);
    }

    void MemoryPanel::set_address(const std::uint64_t address) const {
        m_addressInput->SetValue(fmt::format("0x{:X}", address));
    }

    void MemoryPanel::set_navigate_callback(NavigateCallback callback)
    {
        m_navigateCallback = std::move(callback);
    }

    void MemoryPanel::set_write_callback(WriteMemoryCallback callback)
    {
        m_writeCallback = std::move(callback);
    }

    void MemoryPanel::on_goto_address([[maybe_unused]] wxCommandEvent& event)
    {
        const wxString input = m_addressInput->GetValue();
        unsigned long long address{};
        if (input.ToULongLong(&address, 16) && m_navigateCallback)
        {
            m_navigateCallback(address);
        }
    }

    std::optional<std::vector<std::uint8_t>> MemoryPanel::parse_hex_bytes(const std::string_view text)
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

    void MemoryPanel::on_item_activated(const wxListEvent& event)
    {
        if (!m_writeCallback)
        {
            return;
        }

        const long index = event.GetIndex();
        if (index < 0)
        {
            return;
        }

        const auto rowOffset = static_cast<std::size_t>(index) * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock.data.size())
        {
            return;
        }

        const auto rowByteCount = std::min<std::size_t>(MemoryViewValues::BYTES_PER_ROW, m_memoryBlock.data.size() - rowOffset);
        for (std::size_t i{}; i < rowByteCount; ++i)
        {
            const auto byteIndex = rowOffset + i;
            const bool isReadable = byteIndex < m_memoryBlock.readable.size()
                ? m_memoryBlock.readable[byteIndex]
                : true;
            if (!isReadable)
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

        const auto targetAddress = m_memoryBlock.baseAddress + rowOffset;
        const auto writeStatus = m_writeCallback(targetAddress, *parsed);
        if (writeStatus != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(fmt::format(
                    fmt::runtime(m_languageService.fetch_translation("debugger.memory.writeFailed")),
                    static_cast<int>(writeStatus))),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR,
                this);
            return;
        }

        if (m_memoryBlock.modified.size() != m_memoryBlock.data.size())
        {
            m_memoryBlock.modified.resize(m_memoryBlock.data.size(), false);
        }
        if (m_memoryBlock.readable.size() != m_memoryBlock.data.size())
        {
            m_memoryBlock.readable.resize(m_memoryBlock.data.size(), false);
        }

        std::ranges::copy(*parsed, m_memoryBlock.data.begin() + static_cast<std::ptrdiff_t>(rowOffset));
        std::fill_n(
            m_memoryBlock.modified.begin() + static_cast<std::ptrdiff_t>(rowOffset),
            rowByteCount,
            true);
        std::fill_n(
            m_memoryBlock.readable.begin() + static_cast<std::ptrdiff_t>(rowOffset),
            rowByteCount,
            true);

        m_memoryList->RefreshItem(index);
        update_data_interpretation(static_cast<std::size_t>(index));
        wxLogStatus(this, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.writeSuccess")));
    }

    void MemoryPanel::on_item_selected(const wxListEvent& event)
    {
        const auto index = event.GetIndex();
        if (index < 0)
        {
            update_data_interpretation(std::nullopt);
            return;
        }

        update_data_interpretation(static_cast<std::size_t>(index));
    }

    void MemoryPanel::on_item_deselected([[maybe_unused]] const wxListEvent& event)
    {
        if (m_memoryList->GetSelectedItemCount() == 0)
        {
            update_data_interpretation(std::nullopt);
        }
    }

    const ::Vertex::Debugger::MemoryRegionSlice* MemoryPanel::find_region_for_offset(const std::size_t rowOffset) const
    {
        if (rowOffset >= m_memoryBlock.data.size()
            || rowOffset > std::numeric_limits<std::uint64_t>::max() - m_memoryBlock.baseAddress)
        {
            return nullptr;
        }

        const auto address = m_memoryBlock.baseAddress + static_cast<std::uint64_t>(rowOffset);
        for (const auto& region : m_memoryBlock.regions)
        {
            if (address >= region.startAddress && address < region.endAddress)
            {
                return &region;
            }
        }

        return nullptr;
    }

    bool MemoryPanel::is_span_readable(const std::size_t offset, const std::size_t size) const
    {
        if (offset > m_memoryBlock.data.size() || size > m_memoryBlock.data.size() - offset)
        {
            return false;
        }

        for (std::size_t i{}; i < size; ++i)
        {
            const auto byteIndex = offset + i;
            if (byteIndex < m_memoryBlock.readable.size() && !m_memoryBlock.readable[byteIndex])
            {
                return false;
            }
        }

        return true;
    }

    std::string MemoryPanel::build_interpretation_text(const std::size_t rowOffset) const
    {
        const auto unavailable = m_languageService.fetch_translation("debugger.memory.interpretationUnavailable");
        const auto invalidUtf8 = m_languageService.fetch_translation("debugger.memory.interpretationInvalidUtf8");

        const auto read_unsigned = [this, rowOffset](const std::size_t width, const bool bigEndian) -> std::optional<std::uint64_t>
        {
            if (rowOffset > m_memoryBlock.data.size() || width > m_memoryBlock.data.size() - rowOffset)
            {
                return std::nullopt;
            }
            if (!is_span_readable(rowOffset, width))
            {
                return std::nullopt;
            }

            std::uint64_t value{};
            if (bigEndian)
            {
                for (std::size_t i{}; i < width; ++i)
                {
                    value = (value << 8) | m_memoryBlock.data[rowOffset + i];
                }
                return value;
            }

            for (std::size_t i{}; i < width; ++i)
            {
                value |= static_cast<std::uint64_t>(m_memoryBlock.data[rowOffset + i]) << (i * 8);
            }
            return value;
        };

        const auto to_signed = [](std::uint64_t value, const std::size_t width) -> std::int64_t
        {
            if (width >= sizeof(std::int64_t))
            {
                return static_cast<std::int64_t>(value);
            }

            const auto bitCount = width * 8;
            const auto signBit = std::uint64_t{1} << (bitCount - 1);
            if ((value & signBit) != 0)
            {
                value |= ~((std::uint64_t{1} << bitCount) - 1);
            }

            return static_cast<std::int64_t>(value);
        };

        const auto format_unsigned = [&unavailable](const std::optional<std::uint64_t>& value) -> std::string
        {
            return value.has_value() ? fmt::format("{}", *value) : unavailable;
        };

        const auto format_signed = [&unavailable, &to_signed](const std::optional<std::uint64_t>& value, const std::size_t width) -> std::string
        {
            return value.has_value() ? fmt::format("{}", to_signed(*value, width)) : unavailable;
        };

        const auto format_float32 = [&unavailable](const std::optional<std::uint64_t>& value) -> std::string
        {
            if (!value.has_value())
            {
                return unavailable;
            }
            const auto raw = static_cast<std::uint32_t>(*value);
            const auto decoded = std::bit_cast<float>(raw);
            return std::isfinite(decoded) ? fmt::format("{}", decoded) : unavailable;
        };

        const auto format_float64 = [&unavailable](const std::optional<std::uint64_t>& value) -> std::string
        {
            if (!value.has_value())
            {
                return unavailable;
            }
            const auto decoded = std::bit_cast<double>(*value);
            return std::isfinite(decoded) ? fmt::format("{}", decoded) : unavailable;
        };

        std::vector<std::uint8_t> stringBytes{};
        stringBytes.reserve(MemoryViewValues::INTERPRETATION_MAX_STRING_BYTES);
        for (std::size_t i{}; i < MemoryViewValues::INTERPRETATION_MAX_STRING_BYTES; ++i)
        {
            const auto byteIndex = rowOffset + i;
            if (byteIndex >= m_memoryBlock.data.size())
            {
                break;
            }
            if (!is_span_readable(byteIndex, 1))
            {
                break;
            }

            const auto byte = m_memoryBlock.data[byteIndex];
            if (byte == 0)
            {
                break;
            }
            stringBytes.push_back(byte);
        }

        std::string asciiString{};
        asciiString.reserve(stringBytes.size());
        for (const auto byte : stringBytes)
        {
            if (byte >= 32 && byte < 127)
            {
                asciiString.push_back(static_cast<char>(byte));
            }
            else
            {
                asciiString.push_back('.');
            }
        }

        wxString utf8Text{};
        if (!stringBytes.empty())
        {
            utf8Text = wxString::FromUTF8(
                reinterpret_cast<const char*>(stringBytes.data()),
                static_cast<wxString::size_type>(stringBytes.size()));
        }
        const bool utf8Valid = stringBytes.empty() || !utf8Text.empty();
        const auto utf8String = utf8Valid ? utf8Text.ToStdString() : invalidUtf8;
        const auto asciiDisplay = asciiString.empty() ? "\"\"" : asciiString;
        const auto utf8Display = utf8String.empty() ? "\"\"" : utf8String;

        const auto u8 = read_unsigned(1, false);
        const auto u16le = read_unsigned(2, false);
        const auto u16be = read_unsigned(2, true);
        const auto u32le = read_unsigned(4, false);
        const auto u32be = read_unsigned(4, true);
        const auto u64le = read_unsigned(8, false);
        const auto u64be = read_unsigned(8, true);

        return fmt::format(
            "u8: {}\n"
            "i8: {}\n"
            "u16 LE: {}\n"
            "i16 LE: {}\n"
            "u16 BE: {}\n"
            "i16 BE: {}\n"
            "u32 LE: {}\n"
            "i32 LE: {}\n"
            "u32 BE: {}\n"
            "i32 BE: {}\n"
            "u64 LE: {}\n"
            "i64 LE: {}\n"
            "u64 BE: {}\n"
            "i64 BE: {}\n"
            "f32 LE: {}\n"
            "f32 BE: {}\n"
            "f64 LE: {}\n"
            "f64 BE: {}\n"
            "ASCII: {}\n"
            "UTF-8: {}",
            format_unsigned(u8),
            format_signed(u8, 1),
            format_unsigned(u16le),
            format_signed(u16le, 2),
            format_unsigned(u16be),
            format_signed(u16be, 2),
            format_unsigned(u32le),
            format_signed(u32le, 4),
            format_unsigned(u32be),
            format_signed(u32be, 4),
            format_unsigned(u64le),
            format_signed(u64le, 8),
            format_unsigned(u64be),
            format_signed(u64be, 8),
            format_float32(u32le),
            format_float32(u32be),
            format_float64(u64le),
            format_float64(u64be),
            asciiDisplay,
            utf8Display);
    }

    void MemoryPanel::update_data_interpretation(const std::optional<std::size_t> rowIndex)
    {
        if (!rowIndex.has_value())
        {
            m_interpretationValue->SetLabel(wxString::FromUTF8(
                m_languageService.fetch_translation("debugger.memory.interpretationNoSelection")));
            Layout();
            return;
        }

        const auto rowOffset = *rowIndex * MemoryViewValues::BYTES_PER_ROW;
        if (rowOffset >= m_memoryBlock.data.size())
        {
            m_interpretationValue->SetLabel(wxString::FromUTF8(
                m_languageService.fetch_translation("debugger.memory.interpretationNoSelection")));
            Layout();
            return;
        }

        std::string regionLine{"Region: <unmapped>"};
        if (const auto* region = find_region_for_offset(rowOffset); region != nullptr)
        {
            const auto module = region->moduleName.empty()
                ? m_languageService.fetch_translation("debugger.ui.unknown")
                : region->moduleName;
            regionLine = fmt::format(
                "Region: 0x{:X}-0x{:X}  Module: {}",
                region->startAddress,
                region->endAddress,
                module);
        }

        m_interpretationValue->SetLabel(wxString::FromUTF8(fmt::format(
            "{}\n{}",
            regionLine,
            build_interpretation_text(rowOffset))));
        Layout();
    }

}
