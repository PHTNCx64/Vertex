//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/sizer.h>
#include <wx/collpane.h>
#include <wx/menu.h>

#include <functional>
#include <unordered_map>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/runtime/iregistry.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class RegistersPanel final : public wxPanel
    {
    public:
        using SetRegisterCallback = std::function<void(const std::string& name, std::uint64_t value)>;
        using RefreshCallback = std::function<void()>;

        RegistersPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_registers(const ::Vertex::Debugger::RegisterSet& registers);
        void clear();

        void configure_from_registry(
            const std::vector<Runtime::RegisterCategoryInfo>& categories,
            const std::vector<Runtime::RegisterInfo>& registerDefs);

        void set_flag_bits(const std::string& flagsRegisterName,
                           const std::vector<Runtime::FlagBitInfo>& flagBits);

        void set_register_callback(SetRegisterCallback callback);
        void set_refresh_callback(RefreshCallback callback);

        [[nodiscard]] bool is_configured() const { return m_isConfigured; }

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_item_activated(const wxListEvent& event);
        void on_context_menu(wxContextMenuEvent& event);
        void on_refresh_clicked(wxCommandEvent& event);

        static constexpr int ID_REFRESH = wxID_HIGHEST + 1;

        [[nodiscard]] std::string format_register_value(std::uint64_t value, std::uint8_t bitWidth) const;
        [[nodiscard]] std::string build_flags_tooltip(std::uint64_t value) const;

        wxListCtrl* m_registerList{};
        wxBoxSizer* m_mainSizer{};

        ::Vertex::Debugger::RegisterSet m_registers{};
        SetRegisterCallback m_setRegisterCallback{};
        RefreshCallback m_refreshCallback{};

        std::vector<Runtime::RegisterCategoryInfo> m_categories{};
        std::vector<Runtime::RegisterInfo> m_registerDefs{};
        std::unordered_map<std::string, std::vector<Runtime::FlagBitInfo>> m_flagBits{};
        bool m_isConfigured{};

        std::unordered_map<std::string, long> m_registerIndexMap{};

        Language::ILanguage& m_languageService;
    };
}
