//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/sizer.h>

#include <vertex/customwidgets/scannedvaluescontrol.hh>
#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class ScannedValuesPanel final : public wxPanel
    {
    public:
        explicit ScannedValuesPanel(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel
        );
        ~ScannedValuesPanel() override = default;

        void refresh_list() const;
        void clear_list() const;
        void start_auto_refresh() const;
        void stop_auto_refresh() const;

        void set_selection_change_callback(ScannedValuesControl::SelectionChangeCallback callback) const;
        void set_add_to_table_callback(ScannedValuesControl::AddToTableCallback callback) const;

        [[nodiscard]] int get_selected_index() const;
        [[nodiscard]] std::optional<std::uint64_t> get_selected_address() const;

        [[nodiscard]] ScannedValuesControl* get_control() const
        {
            return m_control;
        }

        [[nodiscard]] ScannedValuesHeader* get_header() const
        {
            return m_header;
        }

    private:
        void create_layout();

        ScannedValuesHeader* m_header{};
        ScannedValuesControl* m_control{};
        wxBoxSizer* m_sizer{};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
    };
}
