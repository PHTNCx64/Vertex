//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <functional>
#include <string>

#include <wx/timer.h>

#include <vertex/customwidgets/base/vertexdataviewctrl.hh>
#include <vertex/customwidgets/savedaddressesdatamodel.hh>
#include <vertex/language/language.hh>
#include <vertex/viewmodel/mainviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class SavedAddressesControl final : public Base::VertexDataViewCtrl
    {
    public:
        using SelectionChangeCallback = std::function<void(int index)>;
        using FreezeToggleCallback = std::function<void(int index, bool frozen)>;
        using ValueEditCallback = std::function<void(int index, const std::string& newValue)>;
        using DeleteCallback = std::function<void(int index)>;
        using ViewInDisassemblyCallback = std::function<void(std::uint64_t address)>;
        using FindAccessCallback = std::function<void(std::uint64_t address, std::uint32_t size)>;

        explicit SavedAddressesControl(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel
        );
        ~SavedAddressesControl() override;

        void refresh_list();
        void clear_list();
        void start_auto_refresh();
        void stop_auto_refresh();

        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_freeze_toggle_callback(FreezeToggleCallback callback);
        void set_value_edit_callback(ValueEditCallback callback);
        void set_delete_callback(DeleteCallback callback);
        void set_view_in_disassembly_callback(ViewInDisassemblyCallback callback);
        void set_find_access_callback(FindAccessCallback callback);

        [[nodiscard]] int get_selected_index() const;

    private:
        void on_context_menu(wxDataViewEvent& event);
        void on_selection_changed(wxDataViewEvent& event);
        void on_key_down(wxKeyEvent& event);
        void on_refresh_timer(wxTimerEvent& event);
        void on_editing_started(wxDataViewEvent& event);
        void on_editing_done(wxDataViewEvent& event);

        void delete_selected();
        void toggle_freeze_selected();
        void refresh_values();

        static constexpr int AUTO_REFRESH_INTERVAL_MS{250};
        static constexpr int REFRESH_OVERSCAN_ROWS{32};
        static constexpr int MIN_SAFE_VIEWPORT_HEIGHT_PX{28};
        static constexpr int COLUMN_WIDTH_FREEZE_DIP{60};
        static constexpr int COLUMN_WIDTH_ADDRESS_DIP{140};
        static constexpr int COLUMN_WIDTH_TYPE_DIP{100};
        static constexpr int COLUMN_WIDTH_VALUE_DIP{180};

        static constexpr int MENU_ID_TOGGLE_FREEZE{1001};
        static constexpr int MENU_ID_COPY_ADDRESS{1002};
        static constexpr int MENU_ID_COPY_VALUE{1003};
        static constexpr int MENU_ID_DELETE{1004};
        static constexpr int MENU_ID_VIEW_IN_DISASSEMBLY{1006};
        static constexpr int MENU_ID_FIND_ACCESS{1007};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
        wxObjectDataPtr<SavedAddressesDataModel> m_dataModel{};

        wxTimer* m_refreshTimer{};
        bool m_isEditing{false};

        SelectionChangeCallback m_selectionChangeCallback{};
        FreezeToggleCallback m_freezeToggleCallback{};
        ValueEditCallback m_valueEditCallback{};
        DeleteCallback m_deleteCallback{};
        ViewInDisassemblyCallback m_viewInDisassemblyCallback{};
        FindAccessCallback m_findAccessCallback{};
    };
}
