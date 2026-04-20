//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstddef>
#include <functional>

#include <vertex/customwidgets/accesstrackerdatamodel.hh>
#include <vertex/customwidgets/base/vertexdataviewctrl.hh>
#include <vertex/language/ilanguage.hh>
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

namespace Vertex::CustomWidgets
{
    class AccessTrackerControl final : public Base::VertexDataViewCtrl
    {
    public:
        using RowCallback = std::function<void(std::size_t rowIndex)>;

        AccessTrackerControl(wxWindow* parent,
                             Language::ILanguage& languageService,
                             const ViewModel::AccessTrackerViewModel& viewModel);
        ~AccessTrackerControl() override = default;

        void notify_entries_changed();

        void set_view_in_disassembly_callback(RowCallback callback);
        void set_show_call_stack_callback(RowCallback callback);
        void set_copy_address_callback(RowCallback callback);
        void set_copy_registers_callback(RowCallback callback);

    private:
        void on_context_menu(wxDataViewEvent& event);

        [[nodiscard]] std::size_t get_selected_row() const;

        static constexpr int MENU_ID_VIEW_IN_DISASSEMBLY{1};
        static constexpr int MENU_ID_SHOW_CALL_STACK{2};
        static constexpr int MENU_ID_COPY_ADDRESS{3};
        static constexpr int MENU_ID_COPY_REGISTERS{4};

        Language::ILanguage& m_languageService;
        const ViewModel::AccessTrackerViewModel& m_viewModel;
        wxObjectDataPtr<AccessTrackerDataModel> m_dataModel{};

        RowCallback m_viewInDisassemblyCallback{};
        RowCallback m_showCallStackCallback{};
        RowCallback m_copyAddressCallback{};
        RowCallback m_copyRegistersCallback{};
    };
}
