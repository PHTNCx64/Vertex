//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/listctrl.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

#include <functional>
#include <optional>
#include <string_view>
#include <sdk/statuscode.h>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class MemoryVirtualListCtrl final : public wxListCtrl
    {
    public:
        explicit MemoryVirtualListCtrl(wxWindow* parent);

        void set_memory_block(const ::Vertex::Debugger::MemoryBlock* memoryBlock);

    private:
        [[nodiscard]] wxListItemAttr* OnGetItemAttr(long item) const override;
        [[nodiscard]] wxString OnGetItemText(long item, long column) const override;
        void rebuild_row_region_cache();

        const ::Vertex::Debugger::MemoryBlock* m_memoryBlock{};
        std::vector<int> m_rowRegionIndices{};
        std::vector<bool> m_rowStartsAtBoundary{};
        mutable wxListItemAttr m_mappedAttrA{};
        mutable wxListItemAttr m_mappedAttrB{};
        mutable wxListItemAttr m_boundaryAttrA{};
        mutable wxListItemAttr m_boundaryAttrB{};
    };

    class MemoryPanel final : public wxPanel
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using WriteMemoryCallback = std::function<StatusCode(std::uint64_t address, const std::vector<std::uint8_t>& data)>;

        MemoryPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_memory(const ::Vertex::Debugger::MemoryBlock& block);
        void set_address(std::uint64_t address) const;

        void set_navigate_callback(NavigateCallback callback);
        void set_write_callback(WriteMemoryCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_goto_address(wxCommandEvent& event);
        void on_item_activated(const wxListEvent& event);
        void on_item_selected(const wxListEvent& event);
        void on_item_deselected(const wxListEvent& event);
        void update_data_interpretation(std::optional<std::size_t> rowIndex);
        [[nodiscard]] const ::Vertex::Debugger::MemoryRegionSlice* find_region_for_offset(std::size_t rowOffset) const;

        [[nodiscard]] bool is_span_readable(std::size_t offset, std::size_t size) const;
        [[nodiscard]] std::string build_interpretation_text(std::size_t rowOffset) const;

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_hex_bytes(std::string_view text);

        MemoryVirtualListCtrl* m_memoryList{};
        wxTextCtrl* m_addressInput{};
        wxButton* m_goButton{};
        wxStaticText* m_interpretationHeader{};
        wxStaticText* m_interpretationValue{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_addressBarSizer{};
        wxBoxSizer* m_interpretationSizer{};

        ::Vertex::Debugger::MemoryBlock m_memoryBlock{};
        NavigateCallback m_navigateCallback{};
        WriteMemoryCallback m_writeCallback{};

        Language::ILanguage& m_languageService;
    };
}
