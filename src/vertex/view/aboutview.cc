//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/aboutview.hh>
#include <vertex/utility.hh>

#include <wx/statbox.h>
#include <wx/statline.h>

namespace Vertex::View
{
    namespace
    {
        constexpr int ABOUT_DIALOG_WIDTH = 500;
        constexpr int ABOUT_DIALOG_HEIGHT = 550;
        constexpr int CREDITS_SCROLL_HEIGHT = 250;
        constexpr int HEADER_FONT_SIZE_LARGE = 18;
        constexpr int HEADER_FONT_SIZE_MEDIUM = 11;
        constexpr int CREDIT_ENTRY_SPACING = 2;
        constexpr int DESCRIPTION_WRAP_MARGIN = 40;
        constexpr int SCROLL_RATE_HORIZONTAL = 0;
        constexpr int SCROLL_RATE_VERTICAL = 10;
        constexpr int DEFAULT_WIDTH = -1;
        constexpr unsigned char GRAY_COLOR_VALUE = 128;
    }

    AboutView::AboutView(wxWindow* parent, Language::ILanguage& languageService, const AboutInfo& aboutInfo)
        : wxDialog(parent,
                   wxID_ANY,
                   wxString::FromUTF8(languageService.fetch_translation("aboutWindow.title")),
                   wxDefaultPosition,
                   wxSize(FromDIP(ABOUT_DIALOG_WIDTH), FromDIP(ABOUT_DIALOG_HEIGHT)),
                   wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER),
          m_languageService(languageService),
          m_aboutInfo(aboutInfo)
    {
        create_controls();
        layout_controls();
        bind_events();

        CenterOnParent();
    }

    void AboutView::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        create_header_section();
        create_credits_section();
        create_footer_section();
    }

    void AboutView::layout_controls()
    {
        layout_header_section();
        layout_credits_section();
        layout_footer_section();

        SetSizer(m_mainSizer);
        Layout();
    }

    void AboutView::bind_events()
    {
        m_closeButton->Bind(wxEVT_BUTTON, [this]([[maybe_unused]] wxCommandEvent& event)
        {
            EndModal(wxID_OK);
        });

        Bind(wxEVT_CHAR_HOOK, [this](wxKeyEvent& event)
        {
            if (event.GetKeyCode() == WXK_ESCAPE)
            {
                EndModal(wxID_OK);
            }
            else
            {
                event.Skip();
            }
        });
    }

    void AboutView::create_header_section()
    {
        m_headerPanel = new wxPanel(this, wxID_ANY);
        m_headerSizer = new wxBoxSizer(wxVERTICAL);

        m_productNameLabel = new wxStaticText(m_headerPanel, wxID_ANY, wxString::FromUTF8(m_aboutInfo.productName));
        wxFont productFont = m_productNameLabel->GetFont();
        productFont.SetPointSize(HEADER_FONT_SIZE_LARGE);
        productFont.SetWeight(wxFONTWEIGHT_BOLD);
        m_productNameLabel->SetFont(productFont);

        const wxString versionText = wxString::Format("%s %s",
            wxString::FromUTF8(m_languageService.fetch_translation("aboutWindow.version")),
            wxString::FromUTF8(m_aboutInfo.version));
        m_versionLabel = new wxStaticText(m_headerPanel, wxID_ANY, versionText);
        wxFont versionFont = m_versionLabel->GetFont();
        versionFont.SetPointSize(HEADER_FONT_SIZE_MEDIUM);
        m_versionLabel->SetFont(versionFont);

        m_vendorLabel = new wxStaticText(m_headerPanel, wxID_ANY, wxString::FromUTF8(m_aboutInfo.vendor));

        m_copyrightLabel = new wxStaticText(m_headerPanel, wxID_ANY, wxString::FromUTF8(m_aboutInfo.copyright));
        m_copyrightLabel->SetForegroundColour(wxColour(GRAY_COLOR_VALUE, GRAY_COLOR_VALUE, GRAY_COLOR_VALUE));

        m_descriptionLabel = new wxStaticText(m_headerPanel, wxID_ANY, wxString::FromUTF8(m_aboutInfo.description),
                                              wxDefaultPosition, wxDefaultSize, wxALIGN_CENTER | wxST_NO_AUTORESIZE);
        m_descriptionLabel->Wrap(FromDIP(ABOUT_DIALOG_WIDTH - DESCRIPTION_WRAP_MARGIN));
    }

    void AboutView::create_credits_section()
    {
        m_creditsScrollWindow = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition,
                                                     wxSize(DEFAULT_WIDTH, FromDIP(CREDITS_SCROLL_HEIGHT)),
                                                     wxVSCROLL);
        m_creditsScrollWindow->SetScrollRate(SCROLL_RATE_HORIZONTAL, SCROLL_RATE_VERTICAL);
        m_creditsSizer = new wxBoxSizer(wxVERTICAL);

        if (!m_aboutInfo.developers.empty())
        {
            m_developersGroup = create_credits_group(
                m_languageService.fetch_translation("aboutWindow.credits.developers"),
                m_aboutInfo.developers,
                m_creditsScrollWindow);
        }

        if (!m_aboutInfo.contributors.empty())
        {
            m_contributorsGroup = create_credits_group(
                m_languageService.fetch_translation("aboutWindow.credits.contributors"),
                m_aboutInfo.contributors,
                m_creditsScrollWindow);
        }

        if (!m_aboutInfo.testers.empty())
        {
            m_testersGroup = create_credits_group(
                m_languageService.fetch_translation("aboutWindow.credits.testers"),
                m_aboutInfo.testers,
                m_creditsScrollWindow);
        }

        if (!m_aboutInfo.specialThanks.empty())
        {
            m_specialThanksGroup = create_credits_group(
                m_languageService.fetch_translation("aboutWindow.credits.specialThanks"),
                m_aboutInfo.specialThanks,
                m_creditsScrollWindow);
        }
    }

    void AboutView::create_footer_section()
    {
        m_footerPanel = new wxPanel(this, wxID_ANY);
        m_footerSizer = new wxBoxSizer(wxVERTICAL);

        if (!m_aboutInfo.website.empty())
        {
            m_websiteLink = new wxHyperlinkCtrl(m_footerPanel, wxID_ANY,
                                                wxString::FromUTF8(m_aboutInfo.website),
                                                wxString::FromUTF8(m_aboutInfo.website));
        }

        if (!m_aboutInfo.license.empty())
        {
            const wxString licenseText = wxString::Format("%s: %s",
                wxString::FromUTF8(m_languageService.fetch_translation("aboutWindow.license")),
                wxString::FromUTF8(m_aboutInfo.license));
            m_licenseLabel = new wxStaticText(m_footerPanel, wxID_ANY, licenseText);
            m_licenseLabel->SetForegroundColour(wxColour(GRAY_COLOR_VALUE, GRAY_COLOR_VALUE, GRAY_COLOR_VALUE));
        }

        m_closeButton = new wxButton(m_footerPanel, wxID_OK,
                                     wxString::FromUTF8(m_languageService.fetch_translation("general.close")));
    }

    void AboutView::layout_header_section()
    {
        m_headerSizer->Add(m_productNameLabel, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_CENTER_HORIZONTAL | wxTOP, StandardWidgetValues::BORDER_TWICE);
        m_headerSizer->Add(m_versionLabel, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_CENTER_HORIZONTAL | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        m_headerSizer->Add(m_vendorLabel, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_CENTER_HORIZONTAL | wxTOP, CREDIT_ENTRY_SPACING);
        m_headerSizer->Add(m_copyrightLabel, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_CENTER_HORIZONTAL | wxTOP, CREDIT_ENTRY_SPACING);
        m_headerSizer->Add(m_descriptionLabel, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_CENTER_HORIZONTAL | wxALL, StandardWidgetValues::BORDER_TWICE);

        m_headerPanel->SetSizer(m_headerSizer);

        m_mainSizer->Add(m_headerPanel, StandardWidgetValues::NO_PROPORTION,
                        wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);

        auto* headerSeparator = new wxStaticLine(this, wxID_ANY);
        m_mainSizer->Add(headerSeparator, StandardWidgetValues::NO_PROPORTION,
                        wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::BORDER_TWICE);
    }

    void AboutView::layout_credits_section() const
    {
        if (m_developersGroup)
        {
            m_creditsSizer->Add(m_developersGroup, StandardWidgetValues::NO_PROPORTION,
                               wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        if (m_contributorsGroup)
        {
            m_creditsSizer->Add(m_contributorsGroup, StandardWidgetValues::NO_PROPORTION,
                               wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        if (m_testersGroup)
        {
            m_creditsSizer->Add(m_testersGroup, StandardWidgetValues::NO_PROPORTION,
                               wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        if (m_specialThanksGroup)
        {
            m_creditsSizer->Add(m_specialThanksGroup, StandardWidgetValues::NO_PROPORTION,
                               wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        }

        m_creditsScrollWindow->SetSizer(m_creditsSizer);
        m_creditsScrollWindow->FitInside();

        m_mainSizer->Add(m_creditsScrollWindow, StandardWidgetValues::STANDARD_PROPORTION,
                        wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    void AboutView::layout_footer_section()
    {
        auto* footerSeparator = new wxStaticLine(this, wxID_ANY);
        m_mainSizer->Add(footerSeparator, StandardWidgetValues::NO_PROPORTION,
                        wxEXPAND | wxLEFT | wxRIGHT, StandardWidgetValues::BORDER_TWICE);

        auto* footerContentSizer = new wxBoxSizer(wxHORIZONTAL);

        auto* infoSizer = new wxBoxSizer(wxVERTICAL);
        if (m_websiteLink)
        {
            infoSizer->Add(m_websiteLink, StandardWidgetValues::NO_PROPORTION,
                          wxALIGN_LEFT | wxBOTTOM, CREDIT_ENTRY_SPACING);
        }
        if (m_licenseLabel)
        {
            infoSizer->Add(m_licenseLabel, StandardWidgetValues::NO_PROPORTION, wxALIGN_LEFT);
        }
        footerContentSizer->Add(infoSizer, StandardWidgetValues::NO_PROPORTION,
                               wxALIGN_CENTER_VERTICAL);

        footerContentSizer->AddStretchSpacer();

        footerContentSizer->Add(m_closeButton, StandardWidgetValues::NO_PROPORTION,
                               wxALIGN_CENTER_VERTICAL);

        m_footerSizer->Add(footerContentSizer, StandardWidgetValues::STANDARD_PROPORTION,
                          wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_footerPanel->SetSizer(m_footerSizer);
        m_mainSizer->Add(m_footerPanel, StandardWidgetValues::NO_PROPORTION,
                        wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    wxStaticBoxSizer* AboutView::create_credits_group(const std::string_view title,
                                                       const std::vector<AboutInfo::CreditEntry>& entries,
                                                       wxWindow* parent)
    {
        const auto staticBox = new wxStaticBox(parent, wxID_ANY, wxString::FromUTF8(std::string{title}));
        const auto groupSizer = new wxStaticBoxSizer(staticBox, wxVERTICAL);

        for (const auto& [name, role] : entries)
        {
            wxBoxSizer* entrySizer = new wxBoxSizer(wxHORIZONTAL);

            wxStaticText* nameLabel = new wxStaticText(staticBox, wxID_ANY, wxString::FromUTF8(name));
            wxFont nameFont = nameLabel->GetFont();
            nameFont.SetWeight(wxFONTWEIGHT_BOLD);
            nameLabel->SetFont(nameFont);

            entrySizer->Add(nameLabel, StandardWidgetValues::NO_PROPORTION,
                           wxALIGN_CENTER_VERTICAL);

            if (!role.empty())
            {
                wxStaticText* roleLabel = new wxStaticText(staticBox, wxID_ANY, wxString::Format(" - %s", wxString::FromUTF8(role)));
                roleLabel->SetForegroundColour(wxColour(GRAY_COLOR_VALUE, GRAY_COLOR_VALUE, GRAY_COLOR_VALUE));
                entrySizer->Add(roleLabel, StandardWidgetValues::NO_PROPORTION,
                               wxALIGN_CENTER_VERTICAL);
            }

            groupSizer->Add(entrySizer, StandardWidgetValues::NO_PROPORTION,
                           wxLEFT | wxRIGHT | wxTOP, StandardWidgetValues::STANDARD_BORDER);
        }

        groupSizer->AddSpacer(StandardWidgetValues::STANDARD_BORDER);

        return groupSizer;
    }
}
