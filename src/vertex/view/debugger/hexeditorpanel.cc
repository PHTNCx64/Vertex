//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/hexeditorpanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

namespace Vertex::View::Debugger
{
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
        m_contentSizer = new wxBoxSizer(wxHORIZONTAL);

        m_addressInput = new wxTextCtrl(this, wxID_ANY, "0x", wxDefaultPosition, wxSize(FromDIP(150), -1), wxTE_PROCESS_ENTER);
        m_goButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.go")));

        m_hexDisplay = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        m_hexDisplay->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_asciiDisplay = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY | wxTE_DONTWRAP);
        m_asciiDisplay->SetFont(wxFont(10, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
    }

    void HexEditorPanel::layout_controls()
    {
        m_addressBarSizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.address"))), StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_addressInput, StandardWidgetValues::NO_PROPORTION, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_goButton, StandardWidgetValues::NO_PROPORTION);

        m_contentSizer->Add(m_hexDisplay, StandardWidgetValues::STANDARD_PROPORTION * 3, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_contentSizer->Add(m_asciiDisplay, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

        m_mainSizer->Add(m_addressBarSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_contentSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void HexEditorPanel::bind_events()
    {
        m_goButton->Bind(wxEVT_BUTTON, &HexEditorPanel::on_goto_address, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &HexEditorPanel::on_goto_address, this);
    }

    void HexEditorPanel::update_data(const ::Vertex::Debugger::MemoryBlock& block)
    {
        m_memoryBlock = block;
        m_baseAddress = block.baseAddress;
        refresh_display();
    }

    void HexEditorPanel::set_address(const std::uint64_t address)
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
        const wxString input = m_addressInput->GetValue();
        unsigned long long address{};
        if (input.ToULongLong(&address, 16) && m_navigateCallback)
        {
            m_navigateCallback(address);
        }
    }

    void HexEditorPanel::refresh_display()
    {
        wxString hexText;
        wxString asciiText;

        constexpr std::size_t bytesPerRow = 16;
        const std::size_t rows = (m_memoryBlock.data.size() + bytesPerRow - 1) / bytesPerRow;

        for (std::size_t row = 0; row < rows; ++row)
        {
            hexText += fmt::format("{:08X}: ", m_baseAddress + row * bytesPerRow);

            std::string rowAscii;
            for (std::size_t col = 0; col < bytesPerRow; ++col)
            {
                const std::size_t byteIdx = row * bytesPerRow + col;
                if (byteIdx < m_memoryBlock.data.size())
                {
                    const std::uint8_t byte = m_memoryBlock.data[byteIdx];
                    hexText += fmt::format("{:02X} ", byte);

                    if (byte >= 32 && byte < 127)
                    {
                        rowAscii += static_cast<char>(byte);
                    }
                    else
                    {
                        rowAscii += '.';
                    }
                }
                else
                {
                    hexText += "   ";
                    rowAscii += ' ';
                }
            }

            hexText += "\n";
            asciiText += rowAscii + "\n";
        }

        m_hexDisplay->SetValue(hexText);
        m_asciiDisplay->SetValue(asciiText);
    }

}
