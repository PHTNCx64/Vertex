//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/choice.h>
#include <wx/slider.h>
#include <wx/spinctrl.h>
#include <wx/button.h>
#include <wx/statbox.h>
#include <wx/scrolwin.h>

#include <memory>
#include <string>
#include <vector>

#include <vertex/language/ilanguage.hh>
#include <vertex/viewmodel/pluginconfigviewmodel.hh>

#include <sdk/ui.h>

namespace Vertex::View
{
    class PluginConfigView final : public wxPanel
    {
    public:
        PluginConfigView(wxWindow* parent, Language::ILanguage& languageService,
                         std::unique_ptr<ViewModel::PluginConfigViewModel> viewModel);

        void rebuild_ui();
        [[nodiscard]] bool has_panels() const;

    private:
        struct FieldControl
        {
            std::string panelId;
            std::string fieldId;
            UIFieldType type;
            wxWindow* control{};
        };

        void create_controls();
        void layout_controls();
        void bind_events();

        void build_panel_ui(const UIPanel& panel);
        void build_section_ui(wxWindow* parent, wxBoxSizer* parentSizer, const UISection& section, std::string_view panelId);
        void build_field_ui(wxWindow* parent, wxFlexGridSizer* gridSizer, const UIField& field, std::string_view panelId);
        void build_horizontal_field_ui(wxWindow* parent, wxBoxSizer* hSizer, const UIField& field, std::string_view panelId);

        void on_apply_clicked(wxCommandEvent& event);
        void on_reset_clicked(wxCommandEvent& event);
        void on_field_changed(const std::string& panelId, const std::string& fieldId, UIFieldType type);

        [[nodiscard]] UIValue read_control_value(const FieldControl& fieldCtrl) const;
        void write_control_value(const FieldControl& fieldCtrl, const UIValue& value) const;

        void load_values_from_viewmodel();

        wxScrolledWindow* m_scrollPanel{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_contentSizer{};
        wxBoxSizer* m_buttonSizer{};
        wxButton* m_applyButton{};
        wxButton* m_resetButton{};
        wxStaticText* m_noPanelsLabel{};

        std::vector<FieldControl> m_fieldControls{};
        std::vector<std::string> m_panelIds{};

        std::unique_ptr<ViewModel::PluginConfigViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;
    };
}
