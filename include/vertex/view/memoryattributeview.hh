//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <vector>
#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/checkbox.h>
#include <wx/button.h>
#include <wx/statbox.h>

#include <vertex/viewmodel/memoryattributeviewmodel.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/vertexevent.hh>

namespace Vertex::View
{
    class MemoryAttributeView final : public wxDialog
    {
      public:
        explicit MemoryAttributeView(std::unique_ptr<ViewModel::MemoryAttributeViewModel> viewModel, Language::ILanguage& languageService);

        ~MemoryAttributeView() override = default;

      private:
        void create_ui_elements();
        void layout_ui_elements();
        void load_memory_attributes_from_viewmodel();

        void add_checkbox_for_attribute(const Model::MemoryAttributeOptionData& option);
        void show_error_and_close(std::string_view message);
        void reset_attribute_checkboxes();

        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        [[nodiscard]] bool toggle_view();

        bool Show(bool show = true) override;

        std::unique_ptr<ViewModel::MemoryAttributeViewModel> m_viewModel;

        Language::ILanguage& m_languageService;

        wxStaticBox* m_protectionBox{};
        wxStaticBox* m_stateBox{};
        wxStaticBox* m_typeBox{};

        wxStaticBoxSizer* m_protectionGroupSizer{};
        wxStaticBoxSizer* m_stateGroupSizer{};
        wxStaticBoxSizer* m_typeGroupSizer{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_memoryAttributeGroupSizer{};
        wxBoxSizer* m_buttonSizer{};

        wxButton* m_okButton{};
        wxButton* m_cancelButton{};

        std::vector<wxCheckBox*> m_protectionCheckboxes{};
        std::vector<wxCheckBox*> m_stateCheckboxes{};
        std::vector<wxCheckBox*> m_typeCheckboxes{};

        std::vector<VertexOptionState_t> m_stateFunctions{};

        std::vector<Model::MemoryAttributeOptionData> m_loadedOptions{};

        void on_ok_clicked(wxCommandEvent& event);
        void on_cancel_clicked(wxCommandEvent& event);

        void apply_checkbox_states();
    };
}
