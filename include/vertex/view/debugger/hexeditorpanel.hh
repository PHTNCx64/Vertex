//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/panel.h>
#include <wx/scrolwin.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/sizer.h>
#include <wx/event.h>
#include <wx/stattext.h>

#include <functional>
#include <optional>
#include <string_view>
#include <unordered_map>
#include <vector>
#include <sdk/statuscode.h>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    class HexCanvas final : public wxScrolledWindow
    {
    public:
        explicit HexCanvas(wxWindow* parent);

        void set_memory_block(const ::Vertex::Debugger::MemoryBlock* block);
        void set_selected_row(std::optional<std::size_t> rowIndex);
        void set_cursor(std::optional<std::size_t> byteIndex, bool asciiMode);
        void set_pending_edits(const std::unordered_map<std::size_t, std::uint8_t>* pendingEdits);
        void set_selection_range(std::optional<std::pair<std::size_t, std::size_t>> selectionRange);
        [[nodiscard]] std::optional<std::size_t> hit_test_row(const wxPoint& clientPoint) const;
        [[nodiscard]] std::optional<std::pair<std::size_t, bool>> hit_test_byte(const wxPoint& clientPoint) const;
        [[nodiscard]] std::size_t get_visible_rows() const;
        void ensure_row_visible(std::size_t rowIndex);

    private:
        void on_paint(wxPaintEvent& event);
        void update_virtual_geometry();

        const ::Vertex::Debugger::MemoryBlock* m_memoryBlock{};
        const std::unordered_map<std::size_t, std::uint8_t>* m_pendingEdits{};
        std::optional<std::pair<std::size_t, std::size_t>> m_selectionRange{};
        std::optional<std::size_t> m_selectedRow{};
        std::optional<std::size_t> m_cursorByteIndex{};
        bool m_cursorAsciiMode{};
        wxFont m_font{};
        int m_charWidth{};
        int m_charHeight{};
    };

    class HexEditorPanel final : public wxPanel
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using WriteMemoryCallback = std::function<StatusCode(std::uint64_t address, const std::vector<std::uint8_t>& data)>;

        HexEditorPanel(wxWindow* parent, Language::ILanguage& languageService);

        void update_data(const ::Vertex::Debugger::MemoryBlock& block);
        void set_address(std::uint64_t address) const;

        void set_navigate_callback(NavigateCallback callback);
        void set_write_callback(WriteMemoryCallback callback);

    private:
        void create_controls();
        void layout_controls();
        void bind_events();

        void on_goto_address(wxCommandEvent& event);
        void on_canvas_double_click(wxMouseEvent& event);
        void on_canvas_left_down(wxMouseEvent& event);
        void on_canvas_left_up(wxMouseEvent& event);
        void on_canvas_capture_lost(wxMouseCaptureLostEvent& event);
        void on_canvas_mouse_move(wxMouseEvent& event);
        void on_canvas_scroll(wxScrollWinEvent& event);
        void on_canvas_mouse_wheel(wxMouseEvent& event);
        void on_context_menu(wxContextMenuEvent& event);
        void on_canvas_key_down(wxKeyEvent& event);
        void on_copy_hex(wxCommandEvent& event);
        void on_copy_ascii(wxCommandEvent& event);
        void on_copy_address(wxCommandEvent& event);
        void on_goto_row_address(wxCommandEvent& event);
        void refresh_display();
        void update_region_tooltip(std::optional<std::size_t> byteIndex);
        void check_continuous_fetch(std::optional<std::ptrdiff_t> cursorDelta = std::nullopt);
        void request_adjacent_block(bool fetchTop);
        void merge_memory_block(const ::Vertex::Debugger::MemoryBlock& block);
        void edit_row(std::size_t rowIndex);
        void move_cursor(std::ptrdiff_t delta, bool extendSelection = false);
        void set_cursor(
            std::size_t byteIndex,
            bool asciiMode,
            std::optional<std::ptrdiff_t> navigationDelta = std::nullopt,
            bool allowContinuousFetch = true);
        [[nodiscard]] bool apply_pending_edit(std::size_t byteIndex, std::uint8_t value);
        [[nodiscard]] bool commit_pending_edits();
        void discard_pending_edits();
        void undo_pending_edits();
        [[nodiscard]] bool is_byte_editable(std::size_t byteIndex) const;
        void clear_selection();
        [[nodiscard]] std::optional<std::pair<std::size_t, std::size_t>> get_selection_bounds() const;
        [[nodiscard]] wxString build_selection_hex_text() const;
        [[nodiscard]] wxString build_selection_ascii_text() const;

        [[nodiscard]] std::optional<std::size_t> get_context_row_index() const;
        [[nodiscard]] std::optional<std::uint64_t> get_context_row_address() const;
        [[nodiscard]] wxString build_row_hex_text(std::size_t rowIndex) const;
        [[nodiscard]] wxString build_row_ascii_text(std::size_t rowIndex) const;
        [[nodiscard]] bool copy_text_to_clipboard(const wxString& text) const;

        [[nodiscard]] static std::optional<std::vector<std::uint8_t>> parse_hex_bytes(std::string_view text);

        HexCanvas* m_canvas{};
        wxTextCtrl* m_addressInput{};
        wxButton* m_goButton{};
        wxStaticText* m_regionInfoText{};
        wxBoxSizer* m_mainSizer{};
        wxBoxSizer* m_addressBarSizer{};

        ::Vertex::Debugger::MemoryBlock m_memoryBlock{};
        std::uint64_t m_baseAddress{};
        NavigateCallback m_navigateCallback{};
        WriteMemoryCallback m_writeCallback{};
        std::optional<std::size_t> m_contextRowIndex{};
        std::optional<std::size_t> m_selectedRowIndex{};
        std::optional<std::size_t> m_cursorByteIndex{};
        std::optional<std::size_t> m_selectionAnchorByteIndex{};
        std::optional<std::size_t> m_selectionEndByteIndex{};
        bool m_selectionDragging{};
        bool m_forceReplaceOnNextUpdate{};
        bool m_asciiInputMode{};
        bool m_pendingLowNibble{};
        std::unordered_map<std::size_t, std::uint8_t> m_pendingEdits{};
        std::vector<std::unordered_map<std::size_t, std::uint8_t>> m_pendingUndoSnapshots{};
        std::optional<std::size_t> m_previousTopRow{};
        std::optional<std::uint64_t> m_lastContinuousFetchAddress{};
        std::uint64_t m_lastContinuousFetchWindowBase{};
        std::size_t m_lastContinuousFetchWindowSize{};

        Language::ILanguage& m_languageService;
    };
}
