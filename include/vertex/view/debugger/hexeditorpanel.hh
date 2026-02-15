//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/scrolwin.h>
#include <wx/stattext.h>

#include <functional>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class HexEditorPanel final : public wxPanel
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using WriteMemoryCallback = std::function<void(std::uint64_t address, const std::vector<std::uint8_t>& data)>;

        HexEditorPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_data(const ::Vertex::Debugger::MemoryBlock& block);
        void set_address(std::uint64_t address);

        void set_navigate_callback(NavigateCallback callback);
        void set_write_callback(WriteMemoryCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_goto_address(wxCommandEvent& event);
        void refresh_display();

        wxTextCtrl* m_hexDisplay{};
        wxTextCtrl* m_asciiDisplay{};
        wxTextCtrl* m_addressInput{};
        wxButton* m_goButton{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_addressBarSizer{};
        wxBoxSizer* m_contentSizer{};

        ::Vertex::Debugger::MemoryBlock m_memoryBlock{};
        std::uint64_t m_baseAddress{};
        NavigateCallback m_navigateCallback{};
        WriteMemoryCallback m_writeCallback{};

        Language::ILanguage& m_languageService;
    };
}
