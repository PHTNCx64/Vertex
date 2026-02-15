//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/sizer.h>

#include <vertex/customwidgets/savedaddressescontrol.hh>
#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class SavedAddressesPanel final : public wxPanel
    {
    public:
        explicit SavedAddressesPanel(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel
        );
        ~SavedAddressesPanel() override = default;

        void refresh_list() const;
        void clear_list() const;
        void start_auto_refresh() const;
        void stop_auto_refresh() const;

        void set_selection_change_callback(SavedAddressesControl::SelectionChangeCallback callback) const;
        void set_freeze_toggle_callback(SavedAddressesControl::FreezeToggleCallback callback) const;
        void set_value_edit_callback(SavedAddressesControl::ValueEditCallback callback) const;
        void set_delete_callback(SavedAddressesControl::DeleteCallback callback) const;
        void set_pointer_scan_callback(SavedAddressesControl::PointerScanCallback callback) const;
        void set_view_in_disassembly_callback(SavedAddressesControl::ViewInDisassemblyCallback callback) const;
        void set_find_access_callback(SavedAddressesControl::FindAccessCallback callback) const;

        [[nodiscard]] int get_selected_index() const;

        [[nodiscard]] SavedAddressesControl* get_control() const
        {
            return m_control;
        }

        [[nodiscard]] SavedAddressesHeader* get_header() const
        {
            return m_header;
        }

    private:
        void create_layout();

        SavedAddressesHeader* m_header{};
        SavedAddressesControl* m_control{};
        wxBoxSizer* m_sizer{};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
    };
}
