//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/scrolwin.h>
#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <wx/panel.h>

#include <functional>
#include <optional>

#include <vertex/viewmodel/mainviewmodel.hh>
#include <vertex/language/language.hh>

namespace Vertex::CustomWidgets
{
    class ScannedValuesHeader final : public wxPanel
    {
    public:
        using ColumnResizeCallback = std::function<void()>;

        explicit ScannedValuesHeader(
            wxWindow* parent,
            Language::ILanguage& languageService
        );

        void set_horizontal_scroll_offset(int offset);
        void set_column_resize_callback(ColumnResizeCallback callback);

        [[nodiscard]] int get_header_height() const
        {
            return m_headerHeight;
        }

        [[nodiscard]] int get_address_width() const
        {
            return m_addressWidth;
        }

        [[nodiscard]] int get_value_width() const
        {
            return m_valueWidth;
        }

        [[nodiscard]] int get_first_value_width() const
        {
            return m_firstValueWidth;
        }

        [[nodiscard]] int get_previous_value_width() const
        {
            return m_previousValueWidth;
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
        int m_addressWidth{};
        int m_valueWidth{};
        int m_firstValueWidth{};
        int m_previousValueWidth{};
        int m_hScrollOffset{};

        int m_resizingColumn{-1};
        int m_resizeStartX{};
        int m_resizeStartWidth{};

        wxFont m_codeFont{};
        wxFont m_codeFontBold{};

        wxString m_headerAddress{};
        wxString m_headerValue{};
        wxString m_headerFirstValue{};
        wxString m_headerPreviousValue{};

        ColumnResizeCallback m_columnResizeCallback{};

        struct Colors
        {
            wxColour headerBackground{0x2D, 0x2D, 0x2D};
            wxColour headerBorder{0x3E, 0x3E, 0x3E};
            wxColour headerText{0xCC, 0xCC, 0xCC};
            wxColour separatorHover{0x56, 0x9C, 0xD6};
        } m_colors{};
    };

    class ScannedValuesControl final : public wxScrolledWindow
    {
    public:
        using SelectionChangeCallback = std::function<void(int index, std::uint64_t address)>;
        using AddToTableCallback = std::function<void(int index, std::uint64_t address)>;

        explicit ScannedValuesControl(
            wxWindow* parent,
            Language::ILanguage& languageService,
            const std::shared_ptr<ViewModel::MainViewModel>& viewModel,
            ScannedValuesHeader* header
        );
        ~ScannedValuesControl() override;

        void refresh_list();
        void clear_list();
        void start_auto_refresh() const;
        void stop_auto_refresh() const;

        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_add_to_table_callback(AddToTableCallback callback);

        [[nodiscard]] int get_selected_index() const;
        [[nodiscard]] std::optional<std::uint64_t> get_selected_address() const;

        void on_columns_resized();

    private:
        void on_paint(wxPaintEvent& event);
        void on_size(wxSizeEvent& event);
        void on_mouse_left_down(const wxMouseEvent& event);
        void on_mouse_left_dclick(wxMouseEvent& event);
        void on_mouse_right_down(wxMouseEvent& event);
        void on_mouse_wheel(wxMouseEvent& event);
        void on_key_down(wxKeyEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_scrollwin(wxScrollWinEvent& event);

        void on_refresh_timer(wxTimerEvent& event);
        void on_scroll_timer(wxTimerEvent& event);

        void render(wxDC& dc);
        void render_background(wxDC& dc) const;
        void render_lines(wxDC& dc, int startLine, int endLine) const;
        void render_line(wxDC& dc, int lineIndex, int y) const;

        [[nodiscard]] int get_line_at_y(int y) const;
        [[nodiscard]] int get_y_for_line(int lineIndex) const;
        [[nodiscard]] int get_visible_line_count() const;
        void update_virtual_size();
        void ensure_line_visible(int lineIndex);
        void refresh_visible_items();
        void sync_header_scroll() const;

        static constexpr int ADDRESS_COLUMN{};
        static constexpr int VALUE_COLUMN{1};
        static constexpr int FIRST_VALUE_COLUMN{2};
        static constexpr int PREVIOUS_VALUE_COLUMN{3};

        static constexpr int MAX_DISPLAYED_ITEMS{10000};

        int m_lineHeight{};

        struct Colors
        {
            wxColour background{0x1E, 0x1E, 0x1E};
            wxColour backgroundAlt{0x25, 0x25, 0x25};
            wxColour selectedLine{0x26, 0x4F, 0x78};

            wxColour address{0x56, 0x9C, 0xD6};
            wxColour value{0xB5, 0xCE, 0xA8};
            wxColour firstValue{0x9C, 0xDC, 0xFE};
            wxColour previousValue{0x80, 0x80, 0x80};

            wxColour changedValue{0xE5, 0x1A, 0x1A};
            wxColour separator{0x3E, 0x3E, 0x3E};
        } m_colors{};

        int m_itemCount{};
        int m_selectedLine{-1};

        wxFont m_codeFont{};

        Language::ILanguage& m_languageService;
        std::shared_ptr<ViewModel::MainViewModel> m_viewModel{};
        ScannedValuesHeader* m_header{};

        SelectionChangeCallback m_selectionChangeCallback{};
        AddToTableCallback m_addToTableCallback{};

        wxTimer* m_refreshTimer{};
        wxTimer* m_scrollStopTimer{};
        bool m_isScrolling{};
    };
}
