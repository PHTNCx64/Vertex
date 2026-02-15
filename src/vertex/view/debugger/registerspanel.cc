//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/view/debugger/registerspanel.hh>
#include <vertex/utility.hh>
#include <wx/textdlg.h>
#include <wx/colour.h>
#include <fmt/format.h>
#include <algorithm>

namespace Vertex::View::Debugger
{
    RegistersPanel::RegistersPanel(wxWindow* parent, Language::ILanguage& languageService)
        : wxPanel(parent, wxID_ANY)
        , m_languageService(languageService)
    {
        create_controls();
        layout_controls();
        bind_events();
    }

    void RegistersPanel::create_controls()
    {
        m_mainSizer = new wxBoxSizer(wxVERTICAL);

        m_registerList = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
                                         wxLC_REPORT | wxLC_SINGLE_SEL);
        m_registerList->SetFont(wxFont(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));

        m_registerList->InsertColumn(0, wxString::FromUTF8(m_languageService.fetch_translation("debugger.registers.columnRegister")), wxLIST_FORMAT_LEFT, FromDIP(60));
        m_registerList->InsertColumn(1, wxString::FromUTF8(m_languageService.fetch_translation("debugger.registers.columnValue")), wxLIST_FORMAT_LEFT, FromDIP(140));
    }

    void RegistersPanel::layout_controls()
    {
        m_mainSizer->Add(m_registerList, 1, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        SetSizer(m_mainSizer);
    }

    void RegistersPanel::bind_events()
    {
        m_registerList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &RegistersPanel::on_item_activated, this);
        m_registerList->Bind(wxEVT_CONTEXT_MENU, &RegistersPanel::on_context_menu, this);
    }

    void RegistersPanel::configure_from_registry(
        const std::vector<Runtime::RegisterCategoryInfo>& categories,
        const std::vector<Runtime::RegisterInfo>& registerDefs)
    {
        m_categories = categories;
        m_registerDefs = registerDefs;
        m_isConfigured = !registerDefs.empty();

        m_registerList->DeleteAllItems();
        m_registerIndexMap.clear();

        if (!m_isConfigured)
        {
            return;
        }

        long idx{};

        auto sortedCategories = categories;
        std::ranges::sort(sortedCategories,
            [](const Runtime::RegisterCategoryInfo& a, const Runtime::RegisterCategoryInfo& b)
            {
                return a.displayOrder < b.displayOrder;
            });

        for (const auto& category : sortedCategories)
        {
            m_registerList->InsertItem(idx, wxString::Format("-- %s --", category.displayName));
            m_registerList->SetItem(idx, 1, EMPTY_STRING);
            m_registerList->SetItemBackgroundColour(idx, wxColour(60, 60, 60));
            m_registerList->SetItemTextColour(idx, wxColour(180, 180, 180));
            ++idx;

            std::vector<Runtime::RegisterInfo> categoryRegs;
            for (const auto& reg : registerDefs)
            {
                if (reg.categoryId == category.categoryId)
                {
                    categoryRegs.push_back(reg);
                }
            }

            std::ranges::sort(categoryRegs,
                [](const Runtime::RegisterInfo& a, const Runtime::RegisterInfo& b)
                {
                    return a.displayOrder < b.displayOrder;
                });

            for (const auto& reg : categoryRegs)
            {
                m_registerList->InsertItem(idx, reg.name);
                m_registerList->SetItem(idx, 1, format_register_value(0, reg.bitWidth));
                m_registerIndexMap[reg.name] = idx;
                ++idx;
            }
        }
    }

    void RegistersPanel::set_flag_bits(const std::string& flagsRegisterName,
                                        const std::vector<Runtime::FlagBitInfo>& flagBits)
    {
        m_flagBits[flagsRegisterName] = flagBits;
    }

    void RegistersPanel::clear()
    {
        m_registerList->DeleteAllItems();
        m_registerIndexMap.clear();
        m_registers = ::Vertex::Debugger::RegisterSet{};
        m_isConfigured = false;
    }

    std::string RegistersPanel::format_register_value(std::uint64_t value, const std::uint8_t bitWidth) const
    {
        switch (bitWidth)
        {
            case 8:
                return fmt::format("{:02X}", value & 0xFF);
            case 16:
                return fmt::format("{:04X}", value & 0xFFFF);
            case 32:
                return fmt::format("{:08X}", value & 0xFFFFFFFF);
            case 64:
            default:
                return fmt::format("{:016X}", value);
            case 128:
                return fmt::format("{:016X}", value);
            case 256:
                return fmt::format("{:016X}...", value);
            case 512:
                return fmt::format("{:016X}...", value);
        }
    }

    std::string RegistersPanel::build_flags_tooltip(std::uint64_t value) const
    {
        std::string flagsRegName;
        for (const auto& reg : m_registerDefs)
        {
            if (reg.flags & VERTEX_REG_FLAG_FLAGS_REGISTER)
            {
                flagsRegName = reg.name;
                break;
            }
        }

        if (flagsRegName.empty())
        {
            return EMPTY_STRING;
        }

        const auto it = m_flagBits.find(flagsRegName);
        if (it == m_flagBits.end())
        {
            return EMPTY_STRING;
        }

        std::string tooltip;
        for (const auto& flagBit : it->second)
        {
            const bool isSet = (value >> flagBit.bitPosition) & 1;
            tooltip += fmt::format("{}: {} (bit {})\n",
                flagBit.bitName,
                isSet ? "1" : "0",
                flagBit.bitPosition);
        }

        return tooltip;
    }

    void RegistersPanel::update_registers(const ::Vertex::Debugger::RegisterSet& registers)
    {
        m_registers = registers;

        if (m_isConfigured)
        {
            std::unordered_map<std::string, std::pair<std::uint64_t, bool>> valueMap;

            for (const auto& reg : registers.generalPurpose)
            {
                valueMap[reg.name] = {reg.value, reg.modified};
            }
            for (const auto& reg : registers.segment)
            {
                valueMap[reg.name] = {reg.value, reg.modified};
            }
            for (const auto& reg : registers.flags)
            {
                valueMap[reg.name] = {reg.value, reg.modified};
            }

            for (const auto& regDef : m_registerDefs)
            {
                const auto indexIt = m_registerIndexMap.find(regDef.name);
                if (indexIt == m_registerIndexMap.end())
                {
                    continue;
                }

                const long idx = indexIt->second;
                std::uint64_t value{};
                bool modified{};

                const auto valueIt = valueMap.find(regDef.name);
                if (valueIt != valueMap.end())
                {
                    value = valueIt->second.first;
                    modified = valueIt->second.second;
                }

                m_registerList->SetItem(idx, 1, format_register_value(value, regDef.bitWidth));

                if (modified)
                {
                    m_registerList->SetItemTextColour(idx, *wxRED);
                }
                else
                {
                    m_registerList->SetItemTextColour(idx, *wxWHITE);
                }
            }
        }
        else
        {
            m_registerList->DeleteAllItems();
            m_registerIndexMap.clear();

            long idx{};

            for (const auto& reg : registers.generalPurpose)
            {
                m_registerList->InsertItem(idx, reg.name);
                m_registerList->SetItem(idx, 1, fmt::format("{:016X}", reg.value));
                m_registerIndexMap[reg.name] = idx;

                if (reg.modified)
                {
                    m_registerList->SetItemTextColour(idx, *wxRED);
                }
                ++idx;
            }

            m_registerList->InsertItem(idx, "RIP");
            m_registerList->SetItem(idx, 1, fmt::format("{:016X}", registers.instructionPointer));
            ++idx;

            m_registerList->InsertItem(idx, "RSP");
            m_registerList->SetItem(idx, 1, fmt::format("{:016X}", registers.stackPointer));
            ++idx;

            m_registerList->InsertItem(idx, "RBP");
            m_registerList->SetItem(idx, 1, fmt::format("{:016X}", registers.basePointer));
        }
    }

    void RegistersPanel::set_register_callback(SetRegisterCallback callback)
    {
        m_setRegisterCallback = std::move(callback);
    }

    void RegistersPanel::set_refresh_callback(RefreshCallback callback)
    {
        m_refreshCallback = std::move(callback);
    }

    void RegistersPanel::on_context_menu([[maybe_unused]] wxContextMenuEvent& event)
    {
        wxMenu menu;
        menu.Append(ID_REFRESH, wxString::FromUTF8(m_languageService.fetch_translation("debugger.registers.refresh")), wxString::FromUTF8(m_languageService.fetch_translation("debugger.registers.refreshTooltip")));

        menu.Bind(wxEVT_MENU, &RegistersPanel::on_refresh_clicked, this, ID_REFRESH);

        PopupMenu(&menu);
    }

    void RegistersPanel::on_refresh_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (m_refreshCallback)
        {
            m_refreshCallback();
        }
    }

    void RegistersPanel::on_item_activated(const wxListEvent& event)
    {
        const long idx = event.GetIndex();
        if (idx < 0)
        {
            return;
        }

        const wxString regName = m_registerList->GetItemText(idx, 0);

        if (regName.StartsWith("--"))
        {
            return;
        }

        const wxString currentValue = m_registerList->GetItemText(idx, 1);

        wxTextEntryDialog dialog(this,
            wxString::FromUTF8(fmt::format("{}: {}", m_languageService.fetch_translation("debugger.registers.enterNewValue"), regName.ToStdString())),
            wxString::FromUTF8(m_languageService.fetch_translation("debugger.registers.setRegisterValue")),
            currentValue);

        if (dialog.ShowModal() == wxID_OK && m_setRegisterCallback)
        {
            unsigned long long newValue{};
            if (dialog.GetValue().ToULongLong(&newValue, 16))
            {
                m_setRegisterCallback(regName.ToStdString(), newValue);
            }
        }
    }

}
