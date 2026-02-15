//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/scrolwin.h>
#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <wx/panel.h>

#include <vector>
#include <array>
#include <unordered_map>
#include <unordered_set>
#include <functional>
#include <optional>

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    enum class DisassemblyColumn : int
    {
        Address = 0,
        Bytes,
        Mnemonic,
        Operands,
        Comment,
        COUNT
    };

    class DisassemblyHeader final : public wxPanel
    {
    public:
        using ColumnResizeCallback = std::function<void()>;
        using ColumnReorderCallback = std::function<void()>;

        static constexpr int COLUMN_COUNT = static_cast<int>(DisassemblyColumn::COUNT);

        explicit DisassemblyHeader(
            wxWindow* parent,
            Language::ILanguage& languageService
        );

        void set_horizontal_scroll_offset(int offset);
        void set_column_resize_callback(ColumnResizeCallback callback);
        void set_column_reorder_callback(ColumnReorderCallback callback);
        void set_left_offset(int offset);

        [[nodiscard]] int get_header_height() const { return m_headerHeight; }
        [[nodiscard]] int get_char_width() const { return m_charWidth; }
        [[nodiscard]] int get_column_padding() const { return m_columnPadding; }
        [[nodiscard]] int get_left_offset() const { return m_leftOffset; }
        [[nodiscard]] int get_column_width(DisassemblyColumn column) const;
        void set_column_width(DisassemblyColumn column, int width);
        [[nodiscard]] const std::array<DisassemblyColumn, COLUMN_COUNT>& get_column_order() const { return m_columnOrder; }
        void set_column_order(const std::array<DisassemblyColumn, COLUMN_COUNT>& order);
        [[nodiscard]] int get_total_width() const;
        [[nodiscard]] int get_column_start_x(int visualIndex) const;

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
        [[nodiscard]] int get_column_at_x(int x) const;
        void draw_drag_indicator(wxDC& dc, int x);
        [[nodiscard]] const wxString& get_column_header(DisassemblyColumn column) const;

        static constexpr int MIN_COLUMN_WIDTH = 40;
        static constexpr int SEPARATOR_HIT_TOLERANCE = 4;
        static constexpr int DRAG_THRESHOLD = 5;

        int m_headerHeight{};
        int m_charWidth{};
        int m_columnPadding{};
        int m_hScrollOffset{};
        int m_leftOffset{};

        std::array<int, COLUMN_COUNT> m_columnWidths{};
        std::array<DisassemblyColumn, COLUMN_COUNT> m_columnOrder{};

        int m_resizingColumn{-1};
        int m_resizeStartX{};
        int m_resizeStartWidth{};

        bool m_dragging{};
        int m_dragSourceIndex{-1};
        int m_dragStartX{};
        int m_dragCurrentX{};
        int m_dragTargetIndex{-1};

        wxFont m_codeFont{};
        wxFont m_codeFontBold{};

        wxString m_headerAddress;
        wxString m_headerBytes;
        wxString m_headerMnemonic;
        wxString m_headerOperands;
        wxString m_headerComment;

        ColumnResizeCallback m_columnResizeCallback{};
        ColumnReorderCallback m_columnReorderCallback{};

        struct Colors
        {
            wxColour headerBackground{0x2D, 0x2D, 0x2D};
            wxColour headerBorder{0x3E, 0x3E, 0x3E};
            wxColour headerText{0xCC, 0xCC, 0xCC};
            wxColour separatorHover{0x56, 0x9C, 0xD6};
            wxColour dragIndicator{0x56, 0x9C, 0xD6};
            wxColour draggedColumn{0x3A, 0x3D, 0x41};
        } m_colors;
    };

    class DisassemblyControl final : public wxScrolledWindow
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using BreakpointToggleCallback = std::function<void(std::uint64_t address)>;
        using SelectionChangeCallback = std::function<void(std::uint64_t address)>;
        using ScrollBoundaryCallback = std::function<void(std::uint64_t boundaryAddress, bool isTop)>;

        explicit DisassemblyControl(wxWindow* parent, Language::ILanguage& languageService, DisassemblyHeader* header = nullptr);
        ~DisassemblyControl() override = default;

        void set_header(DisassemblyHeader* header);
        [[nodiscard]] DisassemblyHeader* get_header() const { return m_header; }

        void on_columns_changed();

        void set_disassembly(const ::Vertex::Debugger::DisassemblyRange& range);
        void set_current_instruction(std::uint64_t address);
        void set_breakpoints(const std::vector<std::uint64_t>& addresses);
        void scroll_to_address(std::uint64_t address);
        void select_address(std::uint64_t address);

        [[nodiscard]] std::uint64_t get_selected_address() const;
        [[nodiscard]] std::optional<std::size_t> get_line_at_address(std::uint64_t address) const;

        void set_navigate_callback(NavigateCallback callback);
        void set_breakpoint_toggle_callback(BreakpointToggleCallback callback);
        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_scroll_boundary_callback(ScrollBoundaryCallback callback);

    private:
        void on_paint(wxPaintEvent& event);
        void on_size(wxSizeEvent& event);
        void on_mouse_left_down(const wxMouseEvent& event);
        void on_mouse_left_dclick(const wxMouseEvent& event);
        void on_mouse_right_down(const wxMouseEvent& event);
        void on_mouse_wheel(const wxMouseEvent& event);
        void on_key_down(wxKeyEvent& event);
        void on_erase_background(wxEraseEvent& event);
        void on_scroll(wxScrollWinEvent& event);

        void check_scroll_boundaries();

        void render(wxDC& dc) const;
        void render_background(wxDC& dc) const;
        void render_arrow_gutter(wxDC& dc, int startLine, int endLine) const;
        void render_lines(wxDC& dc, int startLine, int endLine) const;
        void render_line(wxDC& dc, std::size_t lineIndex, int y) const;
        void render_breakpoint_marker(wxDC& dc, int x, int y) const;
        void render_current_instruction_marker(wxDC& dc, int x, int y) const;

        struct ArrowInfo
        {
            std::size_t sourceLineIndex{};
            std::size_t targetLineIndex{};
            std::uint64_t targetAddress{};
            ::Vertex::Debugger::BranchType branchType{};
            int nestingLevel{};
            bool targetOutOfBounds{};
            bool targetIsAbove{};
        };

        void calculate_arrows();
        void render_arrow(wxDC& dc, const ArrowInfo& arrow, int startLine, int endLine) const;
        [[nodiscard]] wxColour get_arrow_color(::Vertex::Debugger::BranchType type) const;

        [[nodiscard]] int get_line_at_y(int y) const;
        [[nodiscard]] int get_y_for_line(std::size_t lineIndex) const;
        [[nodiscard]] int get_visible_line_count() const;
        void update_virtual_size();
        void sync_header_scroll() const;

        void render_column_content(wxDC& dc, const ::Vertex::Debugger::DisassemblyLine& line,
                                   DisassemblyColumn column, int x, int y) const;

        int m_lineHeight{};
        int m_charWidth{};
        int m_gutterWidth{};
        int m_arrowGutterWidth{};
        int m_addressWidth{};
        int m_bytesWidth{};
        int m_mnemonicWidth{};
        int m_operandsWidth{};

        static constexpr int ARROW_GUTTER_BASE_WIDTH = 60;
        static constexpr int ARROW_SPACING = 8;
        static constexpr int MAX_ARROW_NESTING = 6;
        static constexpr int SCROLL_BOUNDARY_THRESHOLD = 5;

        struct Colors
        {
            wxColour background{0x1E, 0x1E, 0x1E};
            wxColour backgroundAlt{0x25, 0x25, 0x25};
            wxColour selectedLine{0x26, 0x4F, 0x78};
            wxColour currentLine{0x3A, 0x3D, 0x41};
            wxColour breakpointLine{0x5C, 0x1F, 0x1F};

            wxColour address{0x56, 0x9C, 0xD6};
            wxColour bytes{0x80, 0x80, 0x80};
            wxColour mnemonicNormal{0xDC, 0xDC, 0xDC};
            wxColour mnemonicJump{0xC5, 0x86, 0xC0};
            wxColour mnemonicCall{0x4E, 0xC9, 0xB0};
            wxColour mnemonicRet{0xD7, 0xBA, 0x7D};
            wxColour mnemonicMov{0x9C, 0xDC, 0xFE};
            wxColour mnemonicArith{0xB5, 0xCE, 0xA8};
            wxColour operands{0xCE, 0x91, 0x78};
            wxColour operandReg{0x4F, 0xC1, 0xFF};
            wxColour operandImm{0xB5, 0xCE, 0xA8};
            wxColour operandMem{0xD7, 0xBA, 0x7D};
            wxColour comment{0x6A, 0x99, 0x55};

            wxColour arrowUnconditional{0x56, 0x9C, 0xD6};
            wxColour arrowConditional{0xC5, 0x86, 0xC0};
            wxColour arrowCall{0x4E, 0xC9, 0xB0};
            wxColour arrowLoop{0xD7, 0xBA, 0x7D};

            wxColour breakpointMarker{0xE5, 0x1A, 0x1A};
            wxColour currentMarker{0xFF, 0xD7, 0x00};

            wxColour gutter{0x2D, 0x2D, 0x2D};
            wxColour gutterBorder{0x3E, 0x3E, 0x3E};
        } m_colors;

        ::Vertex::Debugger::DisassemblyRange m_range{};
        std::unordered_map<std::uint64_t, std::size_t> m_addressToLine{};
        std::unordered_set<std::uint64_t> m_breakpointAddresses{};
        std::vector<ArrowInfo> m_arrows{};

        std::size_t m_selectedLine{};
        std::uint64_t m_currentInstructionAddress{};

        wxFont m_codeFont{};
        wxFont m_codeFontBold{};

        NavigateCallback m_navigateCallback{};
        BreakpointToggleCallback m_breakpointToggleCallback{};
        SelectionChangeCallback m_selectionChangeCallback{};
        ScrollBoundaryCallback m_scrollBoundaryCallback{};

        bool m_fetchingMore{};

        DisassemblyHeader* m_header{};
        Language::ILanguage& m_languageService;

        wxColour m_separatorColor{0x3E, 0x3E, 0x3E};
    };
} // namespace Vertex::View::Debugger
