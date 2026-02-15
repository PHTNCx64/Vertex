//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/scrolwin.h>
#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <wx/panel.h>

#include <wx/combobox.h>

#include <vector>
#include <functional>
#include <optional>
#include <cstdint>

#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class SavedAddressesHeader final : public wxPanel
    {
    public:
        using ColumnResizeCallback = std::function<void()>;

        explicit SavedAddressesHeader(
            wxWindow* parent,
            Language::ILanguage& languageService
        );

        void set_horizontal_scroll_offset(int offset);
        void set_column_resize_callback(ColumnResizeCallback callback);

        [[nodiscard]] int get_header_height() const
        {
            return m_headerHeight;
        }

        [[nodiscard]] int get_freeze_width() const
        {
            return m_freezeWidth;
        }

        [[nodiscard]] int get_address_width() const
        {
            return m_addressWidth;
        }

        [[nodiscard]] int get_type_width() const
        {
            return m_typeWidth;
        }

        [[nodiscard]] int get_value_width() const
        {
            return m_valueWidth;
        }

        [[nodiscard]] int get_char_width() const
        {
            return m_charWidth;
        }

        [[nodiscard]] int get_column_padding() const
        {
            return m_columnPadding;
        }

    private:
        void on_paint(wxPaintEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_mouse_motion(wxMouseEvent& event);
        void on_mouse_left_down(wxMouseEvent& event);
        void on_mouse_left_up(wxMouseEvent& event);
        void on_mouse_capture_lost(wxMouseCaptureLostEvent& event);
        void on_mouse_leave(wxMouseEvent& event);

        [[nodiscard]] int get_separator_at_x(int x) const;
        [[nodiscard]] int get_separator_x(int separatorIndex) const;

        static constexpr int MIN_COLUMN_WIDTH{50};
        static constexpr int SEPARATOR_HIT_TOLERANCE{4};

        int m_headerHeight{};
        int m_charWidth{};
        int m_columnPadding{};
        int m_freezeWidth{};
        int m_addressWidth{};
        int m_typeWidth{};
        int m_valueWidth{};
        int m_hScrollOffset{};

        int m_resizingColumn{-1};
        int m_resizeStartX{};
        int m_resizeStartWidth{};

        wxFont m_codeFont{};
        wxFont m_codeFontBold{};

        wxString m_headerFreeze{};
        wxString m_headerAddress{};
        wxString m_headerType{};
        wxString m_headerValue{};

        ColumnResizeCallback m_columnResizeCallback{};

        struct Colors
        {
            wxColour headerBackground{0x2D, 0x2D, 0x2D};
            wxColour headerBorder{0x3E, 0x3E, 0x3E};
            wxColour headerText{0xCC, 0xCC, 0xCC};
            wxColour separatorHover{0x56, 0x9C, 0xD6};
        } m_colors{};
    };

    class SavedAddressesControl final : public wxScrolledWindow
    {
    public:
        using SelectionChangeCallback = std::function<void(int index)>;
        using FreezeToggleCallback = std::function<void(int index, bool frozen)>;
        using ValueEditCallback = std::function<void(int index, const std::string& newValue)>;
        using DeleteCallback = std::function<void(int index)>;
        using PointerScanCallback = std::function<void(std::uint64_t address)>;
        using ViewInDisassemblyCallback = std::function<void(std::uint64_t address)>;
        using FindAccessCallback = std::function<void(std::uint64_t address, std::uint32_t size)>;

        explicit SavedAddressesControl(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel,
            SavedAddressesHeader* header
        );
        ~SavedAddressesControl() override;

        void refresh_list();
        void clear_list();
        void start_auto_refresh() const;
        void stop_auto_refresh() const;

        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_freeze_toggle_callback(FreezeToggleCallback callback);
        void set_value_edit_callback(ValueEditCallback callback);
        void set_delete_callback(DeleteCallback callback);
        void set_pointer_scan_callback(PointerScanCallback callback);
        void set_view_in_disassembly_callback(ViewInDisassemblyCallback callback);
        void set_find_access_callback(FindAccessCallback callback);

        [[nodiscard]] int get_selected_index() const;

        void on_columns_resized();

    private:
        void on_paint(wxPaintEvent& event);
        void on_size(wxSizeEvent& event);
        void on_mouse_left_down(const wxMouseEvent& event);
        void on_mouse_left_dclick(const wxMouseEvent& event);
        void on_mouse_right_down(wxMouseEvent& event);
        void on_mouse_wheel(const wxMouseEvent& event);
        void on_key_down(wxKeyEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_scrollwin(wxScrollWinEvent& event);

        void on_refresh_timer(wxTimerEvent& event);
        void on_scroll_timer(wxTimerEvent& event);

        void render(wxDC& dc);
        void render_background(wxDC& dc) const;
        void render_lines(wxDC& dc, int startLine, int endLine);
        void render_line(wxDC& dc, int lineIndex, int y);
        void render_checkbox(wxDC& dc, int x, int y, bool checked, bool hovered);

        [[nodiscard]] int get_line_at_y(int y) const;
        [[nodiscard]] int get_y_for_line(int lineIndex) const;
        [[nodiscard]] int get_visible_line_count() const;
        void update_virtual_size();
        void ensure_line_visible(int lineIndex);
        void refresh_visible_items();
        void sync_header_scroll() const;

        [[nodiscard]] bool is_click_on_checkbox(int x) const;
        [[nodiscard]] int get_column_at_x(int x) const;

        void show_address_edit_dialog(int lineIndex);
        void show_value_edit_dialog(int lineIndex);
        void show_type_combo_popup(int lineIndex, int x, int y);
        void on_type_combo_selection(wxCommandEvent& event);
        void hide_type_combo();

        static constexpr int FREEZE_COLUMN{};
        static constexpr int ADDRESS_COLUMN{1};
        static constexpr int TYPE_COLUMN{2};
        static constexpr int VALUE_COLUMN{3};

        static constexpr int CHECKBOX_SIZE{14};
        static constexpr int CHECKBOX_MARGIN{4};

        int m_lineHeight{};

        struct Colors
        {
            wxColour background{0x1E, 0x1E, 0x1E};
            wxColour backgroundAlt{0x25, 0x25, 0x25};
            wxColour selectedLine{0x26, 0x4F, 0x78};

            wxColour address{0x56, 0x9C, 0xD6};
            wxColour type{0xC5, 0x86, 0xC0};
            wxColour value{0xB5, 0xCE, 0xA8};
            wxColour frozenValue{0x4E, 0xC9, 0xB0};

            wxColour separator{0x3E, 0x3E, 0x3E};
        } m_colors{};

        int m_itemCount{};
        int m_selectedLine{-1};

        wxFont m_codeFont{};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
        SavedAddressesHeader* m_header{};

        SelectionChangeCallback m_selectionChangeCallback{};
        FreezeToggleCallback m_freezeToggleCallback{};
        ValueEditCallback m_valueEditCallback{};
        DeleteCallback m_deleteCallback{};
        PointerScanCallback m_pointerScanCallback{};
        ViewInDisassemblyCallback m_viewInDisassemblyCallback{};
        FindAccessCallback m_findAccessCallback{};

        wxTimer* m_refreshTimer{};
        wxTimer* m_scrollStopTimer{};
        bool m_isScrolling{};

        wxComboBox* m_typeCombo{};
        int m_editingLine{-1};
    };
}
