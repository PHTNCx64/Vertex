//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstdint>
#include <functional>
#include <optional>

#include <wx/timer.h>

#include <vertex/customwidgets/base/vertexdataviewctrl.hh>
#include <vertex/customwidgets/scannedvaluesdatamodel.hh>
#include <vertex/language/language.hh>
#include <vertex/viewmodel/mainviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class ScannedValuesControl final : public Base::VertexDataViewCtrl
    {
    public:
        using SelectionChangeCallback = std::function<void(int index, std::uint64_t address)>;
        using AddToTableCallback = std::function<void(int index, std::uint64_t address)>;
        using FindAccessCallback = std::function<void(std::uint64_t address, std::uint32_t size)>;

        explicit ScannedValuesControl(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel
        );
        ~ScannedValuesControl() override;

        void refresh_list();
        void clear_list();
        void start_auto_refresh();
        void stop_auto_refresh();

        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_add_to_table_callback(AddToTableCallback callback);
        void set_find_access_callback(FindAccessCallback callback);

        [[nodiscard]] int get_selected_index() const;
        [[nodiscard]] std::optional<std::uint64_t> get_selected_address() const;

    private:
        void on_item_activated(wxDataViewEvent& event);
        void on_context_menu(wxDataViewEvent& event);
        void on_selection_changed(wxDataViewEvent& event);
        void on_refresh_timer(wxTimerEvent& event);

        void refresh_visible_range();
        [[nodiscard]] std::optional<std::uint64_t> get_address_for_row(int modelRowIndex) const;

        static constexpr int COLUMN_WIDTH_ADDRESS_DIP{140};
        static constexpr int COLUMN_WIDTH_VALUE_DIP{120};
        static constexpr int COLUMN_WIDTH_FIRST_VALUE_DIP{120};
        static constexpr int COLUMN_WIDTH_PREVIOUS_VALUE_DIP{120};

        static constexpr long MAX_DISPLAYED_ITEMS{10000};
        static constexpr int AUTO_REFRESH_INTERVAL_MS{250};
        static constexpr int CACHE_OVERSCAN{4};
        static constexpr int MIN_SAFE_VIEWPORT_HEIGHT_PX{28};

        static constexpr int MENU_ID_ADD_TO_TABLE{1001};
        static constexpr int MENU_ID_COPY_ADDRESS{1002};
        static constexpr int MENU_ID_COPY_VALUE{1003};
        static constexpr int MENU_ID_COPY_ALL{1004};
        static constexpr int MENU_ID_FIND_ACCESS{1005};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
        wxObjectDataPtr<ScannedValuesDataModel> m_dataModel{};

        wxTimer* m_refreshTimer{};

        SelectionChangeCallback m_selectionChangeCallback{};
        AddToTableCallback m_addToTableCallback{};
        FindAccessCallback m_findAccessCallback{};
    };
}
