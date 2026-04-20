//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/richtext/richtextctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/checkbox.h>
#include <wx/sizer.h>
#include <wx/combobox.h>

#include <functional>
#include <deque>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class ConsolePanel final : public wxPanel
    {
    public:
        using CommandCallback = std::function<void(const std::string& command)>;

        explicit ConsolePanel(wxWindow* parent, Language::ILanguage& languageService);

        void append_log(const ::Vertex::Debugger::LogEntry& entry);
        void append_logs(const std::vector<::Vertex::Debugger::LogEntry>& entries);
        void clear_log();

        void set_command_callback(CommandCallback callback);

        void set_show_debug(bool show);
        void set_show_info(bool show);
        void set_show_warning(bool show);
        void set_show_error(bool show);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_command_enter(wxCommandEvent& event);
        void on_clear_clicked(wxCommandEvent& event);
        void on_filter_changed(wxCommandEvent& event);

        void refresh_display();
        [[nodiscard]] wxString format_entry(const ::Vertex::Debugger::LogEntry& entry) const;
        [[nodiscard]] wxColour get_level_color(::Vertex::Debugger::LogLevel level) const;
        [[nodiscard]] wxString get_level_prefix(::Vertex::Debugger::LogLevel level) const;
        [[nodiscard]] bool should_show_entry(const ::Vertex::Debugger::LogEntry& entry) const;

        wxRichTextCtrl* m_logCtrl{};
        wxTextCtrl* m_commandInput{};
        wxButton* m_clearButton{};
        wxCheckBox* m_showDebug{};
        wxCheckBox* m_showInfo{};
        wxCheckBox* m_showWarning{};
        wxCheckBox* m_showError{};

        wxBoxSizer* m_mainSizer{};

        std::deque<::Vertex::Debugger::LogEntry> m_entries{};
        static constexpr std::size_t MAX_ENTRIES = 10000;

        bool m_filterDebug{true};
        bool m_filterInfo{true};
        bool m_filterWarning{true};
        bool m_filterError{true};

        std::deque<std::string> m_commandHistory{};
        std::size_t m_historyIndex{};
        static constexpr std::size_t MAX_HISTORY = 100;

        CommandCallback m_commandCallback{};
        Language::ILanguage& m_languageService;
    };
}
