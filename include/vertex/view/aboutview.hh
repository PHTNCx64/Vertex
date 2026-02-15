//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/statbmp.h>
#include <wx/button.h>
#include <wx/scrolwin.h>
#include <wx/hyperlink.h>

#include <string>
#include <string_view>
#include <vector>

#include <vertex/language/language.hh>

namespace Vertex::View
{
    struct AboutInfo final
    {
        std::string productName{"Vertex"};
        std::string version{"1.0.0"};
        std::string vendor{"PHTNC<>"};
        std::string copyright{"Copyright (c) 2025-2026"};
        std::string description{"Open Source Dynamic Software Reverse Engineering Suite."};
        std::string website{"https://github.com/PHTNCx64/Vertex"};
        std::string license{"AGPLv3 License"};

        struct CreditEntry final
        {
            std::string name{};
            std::string role{};
        };

        std::vector<CreditEntry> developers;
        std::vector<CreditEntry> contributors;
        std::vector<CreditEntry> testers;
        std::vector<CreditEntry> specialThanks;

        AboutInfo& add_developer(std::string_view name, std::string_view role = {})
        {
            developers.push_back({std::string{name}, std::string{role}});
            return *this;
        }

        AboutInfo& add_contributor(std::string_view name, std::string_view role = {})
        {
            contributors.push_back({std::string{name}, std::string{role}});
            return *this;
        }

        AboutInfo& add_tester(std::string_view name, std::string_view role = {})
        {
            testers.push_back({std::string{name}, std::string{role}});
            return *this;
        }

        AboutInfo& add_special_thanks(std::string_view name, std::string_view role = {})
        {
            specialThanks.push_back({std::string{name}, std::string{role}});
            return *this;
        }
    };

    class AboutView final : public wxDialog
    {
      public:
        AboutView(wxWindow* parent, Language::ILanguage& languageService, const AboutInfo& aboutInfo);

      private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void create_header_section();
        void create_credits_section();
        void create_footer_section();

        void layout_header_section();
        void layout_credits_section() const;
        void layout_footer_section();

        [[nodiscard]] wxStaticBoxSizer* create_credits_group(std::string_view title,
                                               const std::vector<AboutInfo::CreditEntry>& entries,
                                               wxWindow* parent);

        wxBoxSizer* m_mainSizer{};

        wxPanel* m_headerPanel{};
        wxBoxSizer* m_headerSizer{};
        wxStaticText* m_productNameLabel{};
        wxStaticText* m_versionLabel{};
        wxStaticText* m_vendorLabel{};
        wxStaticText* m_copyrightLabel{};
        wxStaticText* m_descriptionLabel{};

        wxScrolledWindow* m_creditsScrollWindow{};
        wxBoxSizer* m_creditsSizer{};
        wxStaticBoxSizer* m_developersGroup{};
        wxStaticBoxSizer* m_contributorsGroup{};
        wxStaticBoxSizer* m_testersGroup{};
        wxStaticBoxSizer* m_specialThanksGroup{};

        wxPanel* m_footerPanel{};
        wxBoxSizer* m_footerSizer{};
        wxHyperlinkCtrl* m_websiteLink{};
        wxStaticText* m_licenseLabel{};
        wxButton* m_closeButton{};

        Language::ILanguage& m_languageService;
        AboutInfo m_aboutInfo;
    };
}
