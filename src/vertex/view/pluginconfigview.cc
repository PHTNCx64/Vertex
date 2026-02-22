//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/pluginconfigview.hh>

#include <vertex/utility.hh>
#include <wx/filedlg.h>
#include <wx/dirdlg.h>
#include <wx/statline.h>
#include <algorithm>
#include <ranges>
#include <span>
#include <tuple>

namespace Vertex::View
{
    PluginConfigView::PluginConfigView(wxWindow* parent, Language::ILanguage& languageService,
                                       std::unique_ptr<ViewModel::PluginConfigViewModel> viewModel)
        : wxPanel(parent, wxID_ANY),
          m_viewModel{std::move(viewModel)},
          m_languageService{languageService}
    {
        create_controls();
        layout_controls();
        bind_events();
        rebuild_ui();
    }

    void PluginConfigView::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_scrollPanel = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL);
        m_scrollPanel->SetScrollRate(0, StandardWidgetValues::STANDARD_BORDER);

        m_contentSizer = new wxBoxSizer(wxVERTICAL);

        m_buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        m_applyButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("general.apply")));
        m_resetButton = new wxButton(this, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("general.resetToDefaults")));
        m_applyButton->Enable(false);
    }

    void PluginConfigView::layout_controls()
    {
        m_scrollPanel->SetSizer(m_contentSizer);

        m_buttonSizer->AddStretchSpacer();
        m_buttonSizer->Add(m_resetButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_buttonSizer->Add(m_applyButton, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);

        m_mainSizer->Add(m_scrollPanel, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        m_mainSizer->Add(m_buttonSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);

        SetSizer(m_mainSizer);
    }

    void PluginConfigView::bind_events()
    {
        m_applyButton->Bind(wxEVT_BUTTON, &PluginConfigView::on_apply_clicked, this);
        m_resetButton->Bind(wxEVT_BUTTON, &PluginConfigView::on_reset_clicked, this);
    }

    bool PluginConfigView::has_panels() const
    {
        return m_viewModel->has_panels();
    }

    void PluginConfigView::rebuild_ui()
    {
        m_fieldControls.clear();
        m_panelIds.clear();
        m_viewModel->clear_pending_values();
        m_contentSizer->Clear(true);

        const auto panels = m_viewModel->get_panels();

        if (panels.empty())
        {
            m_noPanelsLabel = new wxStaticText(m_scrollPanel, wxID_ANY,
                wxString::FromUTF8(m_languageService.fetch_translation("pluginConfig.noPanels")));
            m_contentSizer->Add(m_noPanelsLabel, StandardWidgetValues::NO_PROPORTION,
                wxALL | wxALIGN_CENTER_HORIZONTAL, StandardWidgetValues::BORDER_TWICE);
            m_applyButton->Enable(false);
            m_resetButton->Enable(false);
        }
        else
        {
            m_resetButton->Enable(true);
            for (const auto& snapshot : panels)
            {
                build_panel_ui(snapshot.panel);
            }
            load_values_from_viewmodel();
        }

        m_scrollPanel->FitInside();
        Layout();
    }

    void PluginConfigView::build_panel_ui(const UIPanel& panel)
    {
        const std::string panelId{panel.panelId};
        m_panelIds.push_back(panelId);

        std::ignore = m_viewModel->load_persisted(panelId);

        auto* panelBox = new wxStaticBox(m_scrollPanel, wxID_ANY, wxString::FromUTF8(panel.title));
        auto* panelSizer = new wxStaticBoxSizer(panelBox, wxVERTICAL);

        for (const auto& section : std::span{panel.sections, panel.sectionCount})
        {
            build_section_ui(panelBox, panelSizer, section, panelId);
        }

        m_contentSizer->Add(panelSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    void PluginConfigView::build_section_ui(wxWindow* parent, wxBoxSizer* parentSizer,
                                             const UISection& section, const std::string_view panelId)
    {
        auto* sectionBox = new wxStaticBox(parent, wxID_ANY, wxString::FromUTF8(section.title));
        auto* sectionSizer = new wxStaticBoxSizer(sectionBox, wxVERTICAL);

        const auto fields = std::span{section.fields, section.fieldCount};
        auto fieldGroups = fields | std::views::chunk_by(
            [](const UIField& a, const UIField& b) {
                return a.layoutOrientation == b.layoutOrientation;
            });

        for (const auto& group : fieldGroups)
        {
            if (std::ranges::begin(group)->layoutOrientation == VERTEX_UI_LAYOUT_HORIZONTAL)
            {
                auto* hSizer = new wxBoxSizer(wxHORIZONTAL);
                for (const auto& field : group)
                {
                    build_horizontal_field_ui(sectionBox, hSizer, field, panelId);
                }
                sectionSizer->Add(hSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            }
            else
            {
                auto* gridSizer = new wxFlexGridSizer(StandardWidgetValues::GRID_COLUMNS, StandardWidgetValues::STANDARD_BORDER, StandardWidgetValues::BORDER_TWICE);
                gridSizer->AddGrowableCol(StandardWidgetValues::STANDARD_PROPORTION);
                for (const auto& field : group)
                {
                    build_field_ui(sectionBox, gridSizer, field, panelId);
                }
                sectionSizer->Add(gridSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            }
        }

        parentSizer->Add(sectionSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
    }

    void PluginConfigView::build_field_ui(wxWindow* parent, wxFlexGridSizer* gridSizer,
                                           const UIField& field, const std::string_view panelId)
    {
        std::string fieldPanelId{panelId};
        std::string fieldId{field.fieldId};

        switch (field.type)
        {
        case VERTEX_UI_FIELD_SEPARATOR:
        {
            gridSizer->Add(new wxStaticLine(parent, wxID_ANY), StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            gridSizer->AddSpacer(0);
            return;
        }
        case VERTEX_UI_FIELD_LABEL:
        {
            auto* label = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(field.label));
            gridSizer->Add(label, StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALL, StandardWidgetValues::STANDARD_BORDER);
            gridSizer->AddSpacer(0);
            return;
        }
        case VERTEX_UI_FIELD_BUTTON:
        {
            auto* button = new wxButton(parent, wxID_ANY, wxString::FromUTF8(field.label));

            if (field.tooltip[0] != '\0')
            {
                button->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            button->Bind(wxEVT_BUTTON,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    UIValue empty {};
                    std::ignore = m_viewModel->apply_field(fieldPanelId, fieldId, empty);
                });

            gridSizer->Add(button, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            gridSizer->AddSpacer(0);
            return;
        }
        default:
            break;
        }

        auto* label = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(field.label));
        gridSizer->Add(label, StandardWidgetValues::NO_PROPORTION, wxALIGN_CENTER_VERTICAL | wxALL, StandardWidgetValues::STANDARD_BORDER);

        wxWindow* control{};

        switch (field.type)
        {
        case VERTEX_UI_FIELD_TEXT:
        case VERTEX_UI_FIELD_PATH_FILE:
        case VERTEX_UI_FIELD_PATH_DIR:
        {
            auto* textCtrl = new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(field.defaultValue.stringValue));

            if (field.type == VERTEX_UI_FIELD_PATH_FILE || field.type == VERTEX_UI_FIELD_PATH_DIR)
            {
                auto* pathSizer = new wxBoxSizer(wxHORIZONTAL);
                pathSizer->Add(textCtrl, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);

                auto* browseBtn = new wxButton(parent, wxID_ANY, "...", wxDefaultPosition, wxSize(FromDIP(30), -1));

                const bool isFileType = field.type == VERTEX_UI_FIELD_PATH_FILE;
                browseBtn->Bind(wxEVT_BUTTON,
                    [textCtrl, isFileType, this, fieldPanelId, fieldId](wxCommandEvent&)
                    {
                        wxString path;
                        if (isFileType)
                        {
                            wxFileDialog dlg(this, wxString::FromUTF8(m_languageService.fetch_translation("general.selectFile")));
                            if (dlg.ShowModal() == wxID_OK)
                            {
                                path = dlg.GetPath();
                            }
                        }
                        else
                        {
                            wxDirDialog dlg(this, wxString::FromUTF8(m_languageService.fetch_translation("general.selectDirectory")));
                            if (dlg.ShowModal() == wxID_OK)
                            {
                                path = dlg.GetPath();
                            }
                        }

                        if (!path.empty())
                        {
                            textCtrl->SetValue(path);
                            on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_TEXT);
                        }
                    });

                pathSizer->Add(browseBtn, StandardWidgetValues::NO_PROPORTION);
                gridSizer->Add(pathSizer, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            }
            else
            {
                gridSizer->Add(textCtrl, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            }

            textCtrl->Bind(wxEVT_TEXT,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_TEXT);
                });

            if (field.tooltip[0] != '\0')
            {
                textCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            control = textCtrl;
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_INT:
        {
            auto* spinCtrl = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                static_cast<int>(field.minValue.intValue), static_cast<int>(field.maxValue.intValue),
                static_cast<int>(field.defaultValue.intValue));

            spinCtrl->Bind(wxEVT_SPINCTRL,
                [this, fieldPanelId, fieldId](wxSpinEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_NUMBER_INT);
                });

            if (field.tooltip[0] != '\0')
            {
                spinCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(spinCtrl, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = spinCtrl;
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_FLOAT:
        {
            auto* spinCtrl = new wxSpinCtrlDouble(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                field.minValue.floatValue, field.maxValue.floatValue, field.defaultValue.floatValue, 0.1);

            spinCtrl->Bind(wxEVT_SPINCTRLDOUBLE,
                [this, fieldPanelId, fieldId](wxSpinDoubleEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_NUMBER_FLOAT);
                });

            if (field.tooltip[0] != '\0')
            {
                spinCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(spinCtrl, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = spinCtrl;
            break;
        }
        case VERTEX_UI_FIELD_CHECKBOX:
        {
            auto* checkBox = new wxCheckBox(parent, wxID_ANY, wxEmptyString);
            checkBox->SetValue(field.defaultValue.boolValue != 0);

            checkBox->Bind(wxEVT_CHECKBOX,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_CHECKBOX);
                });

            if (field.tooltip[0] != '\0')
            {
                checkBox->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(checkBox, StandardWidgetValues::NO_PROPORTION, wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = checkBox;
            break;
        }
        case VERTEX_UI_FIELD_DROPDOWN:
        {
            auto* choice = new wxChoice(parent, wxID_ANY);
            for (const auto& option : std::span{field.options, field.optionCount})
            {
                choice->Append(wxString::FromUTF8(option.label));
            }

            if (field.optionCount > 0)
            {
                choice->SetSelection(0);
            }

            choice->Bind(wxEVT_CHOICE,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_DROPDOWN);
                });

            if (field.tooltip[0] != '\0')
            {
                choice->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(choice, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = choice;
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_INT:
        {
            auto* slider = new wxSlider(parent, wxID_ANY,
                static_cast<int>(field.defaultValue.intValue),
                static_cast<int>(field.minValue.intValue),
                static_cast<int>(field.maxValue.intValue));

            slider->Bind(wxEVT_SLIDER,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_SLIDER_INT);
                });

            if (field.tooltip[0] != '\0')
            {
                slider->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(slider, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = slider;
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_FLOAT:
        {
            auto* slider = new wxSlider(parent, wxID_ANY,
                static_cast<int>(field.defaultValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR),
                static_cast<int>(field.minValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR),
                static_cast<int>(field.maxValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR));

            slider->Bind(wxEVT_SLIDER,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_SLIDER_FLOAT);
                });

            if (field.tooltip[0] != '\0')
            {
                slider->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            gridSizer->Add(slider, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
            control = slider;
            break;
        }
        default:
            gridSizer->AddSpacer(0);
            return;
        }

        m_fieldControls.push_back({fieldPanelId, fieldId, field.type, control});
    }

    void PluginConfigView::build_horizontal_field_ui(wxWindow* parent, wxBoxSizer* hSizer,
                                                      const UIField& field, const std::string_view panelId)
    {
        std::string fieldPanelId{panelId};
        std::string fieldId{field.fieldId};

        const int border = field.layoutBorder > 0 ? field.layoutBorder : StandardWidgetValues::STANDARD_BORDER;
        const int proportion = field.layoutProportion;

        switch (field.type)
        {
        case VERTEX_UI_FIELD_BUTTON:
        {
            auto* button = new wxButton(parent, wxID_ANY, wxString::FromUTF8(field.label));

            if (field.tooltip[0] != '\0')
            {
                button->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            button->Bind(wxEVT_BUTTON,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    UIValue empty {};
                    std::ignore = m_viewModel->apply_field(fieldPanelId, fieldId, empty);
                });

            hSizer->Add(button, proportion, wxALL, border);
            break;
        }
        case VERTEX_UI_FIELD_TEXT:
        case VERTEX_UI_FIELD_PATH_FILE:
        case VERTEX_UI_FIELD_PATH_DIR:
        {
            auto* textCtrl = new wxTextCtrl(parent, wxID_ANY, wxString::FromUTF8(field.defaultValue.stringValue));

            textCtrl->Bind(wxEVT_TEXT,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_TEXT);
                });

            if (field.tooltip[0] != '\0')
            {
                textCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(textCtrl, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, textCtrl});
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_INT:
        {
            auto* spinCtrl = new wxSpinCtrl(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                static_cast<int>(field.minValue.intValue), static_cast<int>(field.maxValue.intValue),
                static_cast<int>(field.defaultValue.intValue));

            spinCtrl->Bind(wxEVT_SPINCTRL,
                [this, fieldPanelId, fieldId](wxSpinEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_NUMBER_INT);
                });

            if (field.tooltip[0] != '\0')
            {
                spinCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(spinCtrl, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, spinCtrl});
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_FLOAT:
        {
            auto* spinCtrl = new wxSpinCtrlDouble(parent, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxSP_ARROW_KEYS,
                field.minValue.floatValue, field.maxValue.floatValue, field.defaultValue.floatValue, 0.1);

            spinCtrl->Bind(wxEVT_SPINCTRLDOUBLE,
                [this, fieldPanelId, fieldId](wxSpinDoubleEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_NUMBER_FLOAT);
                });

            if (field.tooltip[0] != '\0')
            {
                spinCtrl->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(spinCtrl, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, spinCtrl});
            break;
        }
        case VERTEX_UI_FIELD_CHECKBOX:
        {
            auto* checkBox = new wxCheckBox(parent, wxID_ANY, wxString::FromUTF8(field.label));
            checkBox->SetValue(field.defaultValue.boolValue != 0);

            checkBox->Bind(wxEVT_CHECKBOX,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_CHECKBOX);
                });

            if (field.tooltip[0] != '\0')
            {
                checkBox->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(checkBox, proportion, wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, checkBox});
            break;
        }
        case VERTEX_UI_FIELD_DROPDOWN:
        {
            auto* choice = new wxChoice(parent, wxID_ANY);
            for (const auto& option : std::span{field.options, field.optionCount})
            {
                choice->Append(wxString::FromUTF8(option.label));
            }

            if (field.optionCount > 0)
            {
                choice->SetSelection(0);
            }

            choice->Bind(wxEVT_CHOICE,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_DROPDOWN);
                });

            if (field.tooltip[0] != '\0')
            {
                choice->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(choice, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, choice});
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_INT:
        {
            auto* slider = new wxSlider(parent, wxID_ANY,
                static_cast<int>(field.defaultValue.intValue),
                static_cast<int>(field.minValue.intValue),
                static_cast<int>(field.maxValue.intValue));

            slider->Bind(wxEVT_SLIDER,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_SLIDER_INT);
                });

            if (field.tooltip[0] != '\0')
            {
                slider->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(slider, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, slider});
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_FLOAT:
        {
            auto* slider = new wxSlider(parent, wxID_ANY,
                static_cast<int>(field.defaultValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR),
                static_cast<int>(field.minValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR),
                static_cast<int>(field.maxValue.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR));

            slider->Bind(wxEVT_SLIDER,
                [this, fieldPanelId, fieldId](wxCommandEvent&)
                {
                    on_field_changed(fieldPanelId, fieldId, VERTEX_UI_FIELD_SLIDER_FLOAT);
                });

            if (field.tooltip[0] != '\0')
            {
                slider->SetToolTip(wxString::FromUTF8(field.tooltip));
            }

            hSizer->Add(slider, proportion, wxEXPAND | wxALL, border);
            m_fieldControls.push_back({fieldPanelId, fieldId, field.type, slider});
            break;
        }
        case VERTEX_UI_FIELD_SEPARATOR:
        {
            hSizer->Add(new wxStaticLine(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_VERTICAL),
                StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, border);
            break;
        }
        case VERTEX_UI_FIELD_LABEL:
        {
            auto* label = new wxStaticText(parent, wxID_ANY, wxString::FromUTF8(field.label));
            hSizer->Add(label, proportion, wxALIGN_CENTER_VERTICAL | wxALL, border);
            break;
        }
        default:
            break;
        }
    }

    void PluginConfigView::on_field_changed(const std::string& panelId, const std::string& fieldId, [[maybe_unused]] const UIFieldType type)
    {
        const auto it = std::ranges::find_if(m_fieldControls,
            [&panelId, &fieldId](const FieldControl& fc)
            {
                return fc.panelId == panelId && fc.fieldId == fieldId;
            });

        if (it != m_fieldControls.end())
        {
            const auto value = read_control_value(*it);
            m_viewModel->set_pending_value(panelId, fieldId, value);
            m_applyButton->Enable(true);
        }
    }

    UIValue PluginConfigView::read_control_value(const FieldControl& fieldCtrl) const
    {
        UIValue value{};

        switch (fieldCtrl.type)
        {
        case VERTEX_UI_FIELD_TEXT:
        case VERTEX_UI_FIELD_PATH_FILE:
        case VERTEX_UI_FIELD_PATH_DIR:
        {
            auto* textCtrl = dynamic_cast<wxTextCtrl*>(fieldCtrl.control);
            if (textCtrl)
            {
                auto str = textCtrl->GetValue().ToUTF8();
                std::strncpy(value.stringValue, str.data(), VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1);
                value.stringValue[VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1] = '\0';
            }
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_INT:
        {
            auto* spinCtrl = dynamic_cast<wxSpinCtrl*>(fieldCtrl.control);
            if (spinCtrl)
            {
                value.intValue = spinCtrl->GetValue();
            }
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_FLOAT:
        {
            auto* spinCtrl = dynamic_cast<wxSpinCtrlDouble*>(fieldCtrl.control);
            if (spinCtrl)
            {
                value.floatValue = spinCtrl->GetValue();
            }
            break;
        }
        case VERTEX_UI_FIELD_CHECKBOX:
        {
            auto* checkBox = dynamic_cast<wxCheckBox*>(fieldCtrl.control);
            if (checkBox)
            {
                value.boolValue = checkBox->GetValue() ? 1 : 0;
            }
            break;
        }
        case VERTEX_UI_FIELD_DROPDOWN:
        {
            auto* choice = dynamic_cast<wxChoice*>(fieldCtrl.control);
            if (choice)
            {
                auto selection = choice->GetStringSelection().ToUTF8();
                std::strncpy(value.stringValue, selection.data(), VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1);
                value.stringValue[VERTEX_UI_MAX_STRING_VALUE_LENGTH - 1] = '\0';
            }
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_INT:
        {
            auto* slider = dynamic_cast<wxSlider*>(fieldCtrl.control);
            if (slider)
            {
                value.intValue = slider->GetValue();
            }
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_FLOAT:
        {
            auto* slider = dynamic_cast<wxSlider*>(fieldCtrl.control);
            if (slider)
            {
                value.floatValue = static_cast<double>(slider->GetValue()) / StandardWidgetValues::SLIDER_SCALE_FACTOR;
            }
            break;
        }
        default:
            break;
        }

        return value;
    }

    void PluginConfigView::write_control_value(const FieldControl& fieldCtrl, const UIValue& value) const
    {
        switch (fieldCtrl.type)
        {
        case VERTEX_UI_FIELD_TEXT:
        case VERTEX_UI_FIELD_PATH_FILE:
        case VERTEX_UI_FIELD_PATH_DIR:
        {
            auto* textCtrl = dynamic_cast<wxTextCtrl*>(fieldCtrl.control);
            if (textCtrl)
            {
                textCtrl->ChangeValue(wxString::FromUTF8(value.stringValue));
            }
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_INT:
        {
            auto* spinCtrl = dynamic_cast<wxSpinCtrl*>(fieldCtrl.control);
            if (spinCtrl)
            {
                spinCtrl->SetValue(static_cast<int>(value.intValue));
            }
            break;
        }
        case VERTEX_UI_FIELD_NUMBER_FLOAT:
        {
            auto* spinCtrl = dynamic_cast<wxSpinCtrlDouble*>(fieldCtrl.control);
            if (spinCtrl)
            {
                spinCtrl->SetValue(value.floatValue);
            }
            break;
        }
        case VERTEX_UI_FIELD_CHECKBOX:
        {
            auto* checkBox = dynamic_cast<wxCheckBox*>(fieldCtrl.control);
            if (checkBox)
            {
                checkBox->SetValue(value.boolValue != 0);
            }
            break;
        }
        case VERTEX_UI_FIELD_DROPDOWN:
        {
            auto* choice = dynamic_cast<wxChoice*>(fieldCtrl.control);
            if (choice)
            {
                choice->SetStringSelection(wxString::FromUTF8(value.stringValue));
            }
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_INT:
        {
            auto* slider = dynamic_cast<wxSlider*>(fieldCtrl.control);
            if (slider)
            {
                slider->SetValue(static_cast<int>(value.intValue));
            }
            break;
        }
        case VERTEX_UI_FIELD_SLIDER_FLOAT:
        {
            auto* slider = dynamic_cast<wxSlider*>(fieldCtrl.control);
            if (slider)
            {
                slider->SetValue(static_cast<int>(value.floatValue * StandardWidgetValues::SLIDER_SCALE_FACTOR));
            }
            break;
        }
        default:
            break;
        }
    }

    void PluginConfigView::load_values_from_viewmodel()
    {
        for (const auto& fc : m_fieldControls)
        {
            auto value = m_viewModel->get_field_value(fc.panelId, fc.fieldId);
            if (value.has_value())
            {
                write_control_value(fc, *value);
            }
        }
        m_applyButton->Enable(false);
    }

    void PluginConfigView::on_apply_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        for (const auto& panelId : m_panelIds)
        {
            std::ignore = m_viewModel->apply_all(panelId);
        }

        m_applyButton->Enable(false);
    }

    void PluginConfigView::on_reset_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        for (const auto& panelId : m_panelIds)
        {
            std::ignore = m_viewModel->reset_panel(panelId);
        }

        load_values_from_viewmodel();
        m_applyButton->Enable(false);
    }
}
