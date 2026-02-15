//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/consolepanel.hh>
#include <vertex/utility.hh>
#include <wx/stattext.h>
#include <fmt/format.h>
#include <chrono>

namespace Vertex::View::Debugger
{
    ConsolePanel::ConsolePanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void ConsolePanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        auto* filterSizer = new wxBoxSizer(wxHORIZONTAL);

        m_showDebug = new wxCheckBox(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.filterDebug")));
        m_showDebug->SetValue(true);
        m_showInfo = new wxCheckBox(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.filterInfo")));
        m_showInfo->SetValue(true);
        m_showWarning = new wxCheckBox(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.filterWarning")));
        m_showWarning->SetValue(true);
        m_showError = new wxCheckBox(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.filterError")));
        m_showError->SetValue(true);

        m_clearButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.clear")), wxDefaultPosition, wxSize(FromDIP(60), -1));

        filterSizer->Add(m_showDebug, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        filterSizer->Add(m_showInfo, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        filterSizer->Add(m_showWarning, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        filterSizer->Add(m_showError, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        filterSizer->AddStretchSpacer();
        filterSizer->Add(m_clearButton, 0);

        m_logCtrl = new wxRichTextCtrl(this, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize,
                                        wxRE_MULTILINE | wxRE_READONLY | wxHSCROLL | wxVSCROLL);
        m_logCtrl->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL, false, "Consolas"));
        m_logCtrl->SetBackgroundColour(wxColour(0x1E, 0x1E, 0x1E));
        m_logCtrl->SetForegroundColour(wxColour(0xDC, 0xDC, 0xDC));

        auto* commandSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* promptLabel = new wxStaticText(this, wxID_ANY, ">");
        promptLabel->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));

        m_commandInput = new wxTextCtrl(this, wxID_ANY, EMPTY_STRING, wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
        m_commandInput->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        m_commandInput->SetHint(wxString::FromUTF8(m_languageService.fetch_translation("debugger.console.enterCommand")));

        commandSizer->Add(promptLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        commandSizer->Add(m_commandInput, 1, wxEXPAND);

        m_mainSizer->Add(filterSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_logCtrl, 1, wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(commandSizer, 0, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    void ConsolePanel::layout_controls()
    {
        SetSizer(m_mainSizer);
    }

    void ConsolePanel::bind_events()
    {
        m_commandInput->Bind(wxEVT_TEXT_ENTER, &ConsolePanel::on_command_enter, this);
        m_clearButton->Bind(wxEVT_BUTTON, &ConsolePanel::on_clear_clicked, this);
        m_showDebug->Bind(wxEVT_CHECKBOX, &ConsolePanel::on_filter_changed, this);
        m_showInfo->Bind(wxEVT_CHECKBOX, &ConsolePanel::on_filter_changed, this);
        m_showWarning->Bind(wxEVT_CHECKBOX, &ConsolePanel::on_filter_changed, this);
        m_showError->Bind(wxEVT_CHECKBOX, &ConsolePanel::on_filter_changed, this);

        m_commandInput->Bind(wxEVT_KEY_DOWN, [this](wxKeyEvent& event)
        {
            if (event.GetKeyCode() == WXK_UP && !m_commandHistory.empty())
            {
                if (m_historyIndex > 0)
                {
                    --m_historyIndex;
                    m_commandInput->SetValue(m_commandHistory[m_historyIndex]);
                    m_commandInput->SetInsertionPointEnd();
                }
            }
            else if (event.GetKeyCode() == WXK_DOWN && !m_commandHistory.empty())
            {
                if (m_historyIndex < m_commandHistory.size() - 1)
                {
                    ++m_historyIndex;
                    m_commandInput->SetValue(m_commandHistory[m_historyIndex]);
                    m_commandInput->SetInsertionPointEnd();
                }
                else
                {
                    m_historyIndex = m_commandHistory.size();
                    m_commandInput->Clear();
                }
            }
            else
            {
                event.Skip();
            }
        });
    }

    void ConsolePanel::append_log(const ::Vertex::Debugger::LogEntry& entry)
    {
        m_entries.push_back(entry);
        if (m_entries.size() > MAX_ENTRIES)
        {
            m_entries.pop_front();
        }

        if (should_show_entry(entry))
        {
            m_logCtrl->SetInsertionPointEnd();
            m_logCtrl->BeginTextColour(get_level_color(entry.level));
            m_logCtrl->WriteText(format_entry(entry));
            m_logCtrl->EndTextColour();
            m_logCtrl->ShowPosition(m_logCtrl->GetLastPosition());
        }
    }

    void ConsolePanel::append_logs(const std::vector<::Vertex::Debugger::LogEntry>& entries)
    {
        for (const auto& entry : entries)
        {
            append_log(entry);
        }
    }

    void ConsolePanel::clear_log()
    {
        m_entries.clear();
        m_logCtrl->Clear();
    }

    void ConsolePanel::set_command_callback(CommandCallback callback)
    {
        m_commandCallback = std::move(callback);
    }

    void ConsolePanel::set_show_debug(bool show)
    {
        m_filterDebug = show;
        m_showDebug->SetValue(show);
        refresh_display();
    }

    void ConsolePanel::set_show_info(bool show)
    {
        m_filterInfo = show;
        m_showInfo->SetValue(show);
        refresh_display();
    }

    void ConsolePanel::set_show_warning(bool show)
    {
        m_filterWarning = show;
        m_showWarning->SetValue(show);
        refresh_display();
    }

    void ConsolePanel::set_show_error(bool show)
    {
        m_filterError = show;
        m_showError->SetValue(show);
        refresh_display();
    }

    void ConsolePanel::on_command_enter([[maybe_unused]] wxCommandEvent& event)
    {
        const wxString cmd = m_commandInput->GetValue().Trim();
        if (cmd.empty())
        {
            return;
        }

        m_commandHistory.push_back(cmd.ToStdString());
        if (m_commandHistory.size() > MAX_HISTORY)
        {
            m_commandHistory.pop_front();
        }
        m_historyIndex = m_commandHistory.size();

        ::Vertex::Debugger::LogEntry echoEntry{};
        echoEntry.level = ::Vertex::Debugger::LogLevel::Info;
        echoEntry.message = "> " + cmd.ToStdString();
        echoEntry.timestamp = static_cast<std::uint64_t>(
            std::chrono::system_clock::now().time_since_epoch().count());
        append_log(echoEntry);

        if (m_commandCallback)
        {
            m_commandCallback(cmd.ToStdString());
        }

        m_commandInput->Clear();
    }

    void ConsolePanel::on_clear_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        clear_log();
    }

    void ConsolePanel::on_filter_changed([[maybe_unused]] wxCommandEvent& event)
    {
        m_filterDebug = m_showDebug->GetValue();
        m_filterInfo = m_showInfo->GetValue();
        m_filterWarning = m_showWarning->GetValue();
        m_filterError = m_showError->GetValue();
        refresh_display();
    }

    void ConsolePanel::refresh_display()
    {
        m_logCtrl->Clear();

        for (const auto& entry : m_entries)
        {
            if (should_show_entry(entry))
            {
                m_logCtrl->SetInsertionPointEnd();
                m_logCtrl->BeginTextColour(get_level_color(entry.level));
                m_logCtrl->WriteText(format_entry(entry));
                m_logCtrl->EndTextColour();
            }
        }

        m_logCtrl->ShowPosition(m_logCtrl->GetLastPosition());
    }

    wxString ConsolePanel::format_entry(const ::Vertex::Debugger::LogEntry& entry) const
    {
        const auto timePoint = std::chrono::system_clock::time_point(
            std::chrono::system_clock::duration(entry.timestamp));
        const auto time = std::chrono::system_clock::to_time_t(timePoint);
        std::tm tm{};
#ifdef _WIN32
        localtime_s(&tm, &time);
#else
        localtime_r(&time, &tm);
#endif

        return fmt::format("[{:02d}:{:02d}:{:02d}] {} {}\n",
            tm.tm_hour, tm.tm_min, tm.tm_sec,
            get_level_prefix(entry.level).ToStdString(),
            entry.message);
    }

    wxColour ConsolePanel::get_level_color(::Vertex::Debugger::LogLevel level) const
    {
        switch (level)
        {
            case ::Vertex::Debugger::LogLevel::Debug:   return {0x80, 0x80, 0x80};
            case ::Vertex::Debugger::LogLevel::Info:    return {0xDC, 0xDC, 0xDC};
            case ::Vertex::Debugger::LogLevel::Warning: return {0xFF, 0xD7, 0x00};
            case ::Vertex::Debugger::LogLevel::Error:   return {0xE5, 0x1A, 0x1A};
            case ::Vertex::Debugger::LogLevel::Output:  return {0x4E, 0xC9, 0xB0};
            default: return {0xDC, 0xDC, 0xDC};
        }
    }

    wxString ConsolePanel::get_level_prefix(::Vertex::Debugger::LogLevel level) const
    {
        switch (level)
        {
            case ::Vertex::Debugger::LogLevel::Debug:   return "[DBG]";
            case ::Vertex::Debugger::LogLevel::Info:    return "[INF]";
            case ::Vertex::Debugger::LogLevel::Warning: return "[WRN]";
            case ::Vertex::Debugger::LogLevel::Error:   return "[ERR]";
            case ::Vertex::Debugger::LogLevel::Output:  return "[OUT]";
            default: return "[???]";
        }
    }

    bool ConsolePanel::should_show_entry(const ::Vertex::Debugger::LogEntry& entry) const
    {
        switch (entry.level)
        {
            case ::Vertex::Debugger::LogLevel::Debug:   return m_filterDebug;
            case ::Vertex::Debugger::LogLevel::Info:    return m_filterInfo;
            case ::Vertex::Debugger::LogLevel::Warning: return m_filterWarning;
            case ::Vertex::Debugger::LogLevel::Error:   return m_filterError;
            case ::Vertex::Debugger::LogLevel::Output:  return true;
            default: return true;
        }
    }

}
