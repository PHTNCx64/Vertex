//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/utility.hh>
#include <vertex/view/analyticsview.hh>
#include <wx/sizer.h>
#include <wx/app.h>
#include <wx/settings.h>
#include <wx/button.h>
#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <fmt/format.h>

namespace Vertex::View
{
    namespace
    {
        constexpr int BUTTON_SPACING = 4;
        constexpr int STANDARD_PADDING = 8;
        constexpr int DARK_MODE_THRESHOLD = 128;
        constexpr int RGB_COMPONENT_COUNT = 3;

        constexpr unsigned char INFO_DARK_R = 100;
        constexpr unsigned char INFO_DARK_G = 220;
        constexpr unsigned char INFO_DARK_B = 100;
        constexpr unsigned char INFO_LIGHT_R = 0;
        constexpr unsigned char INFO_LIGHT_G = 128;
        constexpr unsigned char INFO_LIGHT_B = 0;

        constexpr unsigned char WARN_DARK_R = 255;
        constexpr unsigned char WARN_DARK_G = 200;
        constexpr unsigned char WARN_DARK_B = 50;
        constexpr unsigned char WARN_LIGHT_R = 180;
        constexpr unsigned char WARN_LIGHT_G = 130;
        constexpr unsigned char WARN_LIGHT_B = 0;

        constexpr unsigned char ERROR_DARK_R = 255;
        constexpr unsigned char ERROR_DARK_G = 100;
        constexpr unsigned char ERROR_DARK_B = 100;
        constexpr unsigned char ERROR_LIGHT_R = 180;
        constexpr unsigned char ERROR_LIGHT_G = 0;
        constexpr unsigned char ERROR_LIGHT_B = 0;

        constexpr unsigned char DEFAULT_DARK_R = 200;
        constexpr unsigned char DEFAULT_DARK_G = 200;
        constexpr unsigned char DEFAULT_DARK_B = 200;
        constexpr unsigned char DEFAULT_LIGHT_R = 0;
        constexpr unsigned char DEFAULT_LIGHT_G = 0;
        constexpr unsigned char DEFAULT_LIGHT_B = 0;
    }

    AnalyticsView::AnalyticsView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::AnalyticsViewModel> viewModel)
        : wxDialog(wxTheApp->GetTopWindow(), wxID_ANY, wxString::FromUTF8(languageService.fetch_translation("analyticsWindow.title")), wxDefaultPosition, wxSize(StandardWidgetValues::STANDARD_X_DIP, StandardWidgetValues::STANDARD_Y_DIP), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_languageService(languageService),
          m_viewModel(std::move(viewModel))
    {
        create_controls();
        layout_controls();
        bind_events();
        setup_event_callback();
        refresh_logs();
    }

    void AnalyticsView::initialize_view()
    {
        create_controls();
        layout_controls();
        bind_events();
        setup_event_callback();
        refresh_logs();
    }

    void AnalyticsView::create_controls()
    {
        m_clearButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.clearButton")));
        m_saveButton = new wxButton(this, wxID_ANY, wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveButton")));
        m_logTextCtrl = new wxRichTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxRE_MULTILINE | wxTE_READONLY);
    }

    void AnalyticsView::layout_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);

        m_buttonSizer->Add(m_clearButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, BUTTON_SPACING);
        m_buttonSizer->Add(m_saveButton, StandardWidgetValues::NO_PROPORTION, wxRIGHT, BUTTON_SPACING);
        m_buttonSizer->AddStretchSpacer();

        m_mainSizer->Add(m_buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, STANDARD_PADDING);
        m_mainSizer->Add(m_logTextCtrl, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, STANDARD_PADDING);

        SetSizerAndFit(m_mainSizer);
    }

    void AnalyticsView::bind_events()
    {
        m_clearButton->Bind(wxEVT_BUTTON, &AnalyticsView::on_clear_clicked, this);
        m_saveButton->Bind(wxEVT_BUTTON, &AnalyticsView::on_save_clicked, this);
    }

    void AnalyticsView::setup_event_callback()
    {
        if (m_viewModel)
        {
            m_viewModel->set_event_callback([this](const Event::EventId id, const Event::VertexEvent& event)
            {
                vertex_event_callback(id, event);
            });
        }
    }

    void AnalyticsView::vertex_event_callback(const Event::EventId eventId, const Event::VertexEvent& event)
    {
        switch (eventId)
        {
        case Event::VIEW_EVENT:
            std::ignore = toggle_view();
            break;
        default:;
        }
    }

    bool AnalyticsView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        refresh_logs();
        Show();
        return true;
    }

    wxColour AnalyticsView::get_log_color(const Log::LogLevel level, const bool isDarkMode)
    {
        switch (level)
        {
        case Log::LogLevel::INFO_LOG:
            return isDarkMode ? wxColour(INFO_DARK_R, INFO_DARK_G, INFO_DARK_B) : wxColour(INFO_LIGHT_R, INFO_LIGHT_G, INFO_LIGHT_B);
        case Log::LogLevel::WARN_LOG:
            return isDarkMode ? wxColour(WARN_DARK_R, WARN_DARK_G, WARN_DARK_B) : wxColour(WARN_LIGHT_R, WARN_LIGHT_G, WARN_LIGHT_B);
        case Log::LogLevel::ERROR_LOG:
            return isDarkMode ? wxColour(ERROR_DARK_R, ERROR_DARK_G, ERROR_DARK_B) : wxColour(ERROR_LIGHT_R, ERROR_LIGHT_G, ERROR_LIGHT_B);
        default:
            return isDarkMode ? wxColour(DEFAULT_DARK_R, DEFAULT_DARK_G, DEFAULT_DARK_B) : wxColour(DEFAULT_LIGHT_R, DEFAULT_LIGHT_G, DEFAULT_LIGHT_B);
        }
    }

    void AnalyticsView::refresh_logs()
    {
        m_logTextCtrl->Freeze();
        m_logTextCtrl->Clear();

        m_cachedEntries = m_viewModel->get_log_entries();

        const wxColour bgColor = wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOW);
        const bool isDarkMode = (bgColor.Red() + bgColor.Green() + bgColor.Blue()) / RGB_COMPONENT_COUNT < DARK_MODE_THRESHOLD;

        for (const auto& entry : m_cachedEntries)
        {
            const wxColour color = get_log_color(entry.level, isDarkMode);
            const std::string timestamp = Log::TimestampFormatter::format(entry.timestamp);

            std::string_view levelStr = m_languageService.fetch_translation("analyticsWindow.logLevels.info");
            switch (entry.level)
            {
            case Log::LogLevel::INFO_LOG:
                levelStr = m_languageService.fetch_translation("analyticsWindow.logLevels.info");
                break;
            case Log::LogLevel::WARN_LOG:
                levelStr = m_languageService.fetch_translation("analyticsWindow.logLevels.warn");
                break;
            case Log::LogLevel::ERROR_LOG:
                levelStr = m_languageService.fetch_translation("analyticsWindow.logLevels.error");
                break;
            }

            const auto logLine = fmt::format("[{}] [{}] {}", timestamp, levelStr, entry.message);

            m_logTextCtrl->BeginTextColour(color);
            m_logTextCtrl->WriteText(wxString::FromUTF8(logLine));
            m_logTextCtrl->EndTextColour();
            m_logTextCtrl->WriteText("\n");
        }

        m_logTextCtrl->Thaw();

        m_logTextCtrl->ShowPosition(m_logTextCtrl->GetLastPosition());
    }

    void AnalyticsView::on_clear_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_viewModel->clear_logs();
        m_cachedEntries.clear();
        m_logTextCtrl->Clear();
    }

    void AnalyticsView::on_save_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        wxFileDialog saveDialog(
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.title")),
            EMPTY_STRING,
            wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.defaultFilename")),
            wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.fileTypes")),
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT
        );

        if (saveDialog.ShowModal() == wxID_CANCEL)
        {
            return;
        }

        const std::string filePath = saveDialog.GetPath().ToStdString();
        if (m_viewModel->save_logs_to_file(filePath))
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.successMessage")),
                wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.title")),
                wxOK | wxICON_INFORMATION
            );
        }
        else
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.failureMessage")),
                wxString::FromUTF8(m_languageService.fetch_translation("analyticsWindow.saveDialog.title")),
                wxOK | wxICON_ERROR
            );
        }
    }

}
