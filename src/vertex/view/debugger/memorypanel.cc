//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/memorypanel.hh>
#include <vertex/utility.hh>
#include <fmt/format.h>

namespace Vertex::View::Debugger
{
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

        m_addressInput = new wxTextCtrl(this, wxID_ANY, "0x", wxDefaultPosition,
                                         wxSize(FromDIP(150), -1), wxTE_PROCESS_ENTER);
        m_goButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.go")));

        m_memoryList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                       wxLC_REPORT | wxLC_SINGLE_SEL);
        m_memoryList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_memoryList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnAddress")), wxLIST_FORMAT_LEFT, FromDIP(100));
        m_memoryList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnHex")), wxLIST_FORMAT_LEFT, FromDIP(250));
        m_memoryList->InsertColumn(2, wxString::FromUTF8(m_languageService.fetch_translation("debugger.memory.columnAscii")), wxLIST_FORMAT_LEFT, FromDIP(100));
    }

    void MemoryPanel::layout_controls()
    {
        m_addressBarSizer->Add(new wxStaticText(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.ui.address"))),
                                0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_addressInput, 0, wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_addressBarSizer->Add(m_goButton, 0);

        m_mainSizer->Add(m_addressBarSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_memoryList, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM,
                          StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void MemoryPanel::bind_events()
    {
        m_goButton->Bind(wxEVT_BUTTON, &MemoryPanel::on_goto_address, this);
        m_addressInput->Bind(wxEVT_TEXT_ENTER, &MemoryPanel::on_goto_address, this);
    }

    void MemoryPanel::update_memory(const ::Vertex::Debugger::MemoryBlock& block)
    {
        m_memoryBlock = block;
        m_memoryList->DeleteAllItems();

        constexpr std::size_t bytesPerRow = 16;
        const std::size_t rows = (block.data.size() + bytesPerRow - 1) / bytesPerRow;

        for (std::size_t row = 0; row < rows; ++row)
        {
            const std::uint64_t rowAddress = block.baseAddress + (row * bytesPerRow);
            const long idx = m_memoryList->InsertItem(static_cast<long>(row), fmt::format("0x{:X}", rowAddress));

            std::string hexStr;
            std::string asciiStr;
            for (std::size_t col = 0; col < bytesPerRow; ++col)
            {
                const std::size_t byteIdx = row * bytesPerRow + col;
                if (byteIdx < block.data.size())
                {
                    const std::uint8_t byte = block.data[byteIdx];
                    hexStr += fmt::format("{:02X} ", byte);

                    if (byte >= 32 && byte < 127)
                    {
                        asciiStr += static_cast<char>(byte);
                    }
                    else
                    {
                        asciiStr += '.';
                    }
                }
                else
                {
                    hexStr += "   ";
                    asciiStr += ' ';
                }
            }

            m_memoryList->SetItem(idx, 1, hexStr);
            m_memoryList->SetItem(idx, 2, asciiStr);
        }
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

}
