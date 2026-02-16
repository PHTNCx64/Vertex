//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/dialog.h>
#include <wx/sizer.h>
#include <wx/combobox.h>
#include <wx/stattext.h>
#include <wx/button.h>

#include <memory>
#include <vertex/language/language.hh>
#include <vertex/viewmodel/injectorviewmodel.hh>
#include <vertex/event/eventbus.hh>

namespace Vertex::View
{
    class InjectorView final : public wxDialog
    {
    public:
        InjectorView(Language::ILanguage& languageService, std::unique_ptr<ViewModel::InjectorViewModel> viewModel);

    private:
        void vertex_event_callback(Event::EventId eventId, const Event::VertexEvent& event);
        void create_controls();
        void layout_controls();
        void bind_events();
        [[nodiscard]] bool toggle_view();
        void populate_methods() const;
        void update_description();
        void on_inject_clicked();
        wxString build_file_filter() const;

        wxBoxSizer* m_mainSizer{};
        wxStaticText* m_methodLabel{};
        wxComboBox* m_methodComboBox{};
        wxStaticText* m_descriptionLabel{};
        wxStaticText* m_descriptionText{};
        wxBoxSizer* m_buttonSizer{};
        wxButton* m_injectButton{};
        wxButton* m_cancelButton{};

        std::unique_ptr<ViewModel::InjectorViewModel> m_viewModel{};
        Language::ILanguage& m_languageService;
    };
}
