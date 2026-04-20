//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <wx/scrolwin.h>
#include <wx/dcbuffer.h>
#include <wx/font.h>
#include <wx/panel.h>
#include <wx/timer.h>

#include <vector>
#include <array>
#include <memory>
#include <variant>
#include <unordered_map>
#include <functional>
#include <optional>

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/language/language.hh>

namespace Vertex::View::Debugger
{
    struct BreakpointVisualInfo final
    {
        std::uint32_t id{};
        ::Vertex::Debugger::BreakpointState state{::Vertex::Debugger::BreakpointState::Enabled};
        bool hasCondition{};
        ::BreakpointConditionType conditionType{VERTEX_BP_COND_NONE};
        std::string condition{};
        std::uint32_t hitCount{};
        std::uint32_t hitCountTarget{};
    };

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
            wxColour headerBackground{};
            wxColour headerBorder{};
            wxColour headerText{};
            wxColour separatorHover{};
            wxColour dragIndicator{};
            wxColour draggedColumn{};
        } m_colors;

        void load_system_colors();
    };

    class DisassemblyControl final : public wxScrolledWindow
    {
    public:
        using NavigateCallback = std::function<void(std::uint64_t address)>;
        using BreakpointToggleCallback = std::function<void(std::uint64_t address)>;
        using BreakpointEnableCallback = std::function<void(std::uint32_t id, bool enable)>;
        using BreakpointRemoveCallback = std::function<void(std::uint32_t id)>;
        using BreakpointEditConditionCallback = std::function<void(std::uint32_t id, const ::Vertex::Debugger::Breakpoint& bp)>;
        using RunToCursorCallback = std::function<void(std::uint64_t address)>;
        using SelectionChangeCallback = std::function<void(std::uint64_t address)>;
        using ScrollBoundaryCallback = std::function<void(std::uint64_t boundaryAddress, bool isTop)>;
        using ShowInMemoryCallback = std::function<void(std::uint64_t address)>;
        using XrefResultHandler = std::function<void(std::vector<::Vertex::Debugger::XrefEntry>)>;
        using XrefQueryCallback = std::function<void(std::uint64_t address,
            ::Vertex::Debugger::XrefDirection direction, XrefResultHandler onResult)>;

        explicit DisassemblyControl(wxWindow* parent, Language::ILanguage& languageService, DisassemblyHeader* header = nullptr);
        ~DisassemblyControl() override = default;

        void set_header(DisassemblyHeader* header);
        [[nodiscard]] DisassemblyHeader* get_header() const { return m_header; }

        void on_columns_changed();

        void set_disassembly(const ::Vertex::Debugger::DisassemblyRange& range);
        void set_current_instruction(std::uint64_t address);
        void set_breakpoints(const std::vector<::Vertex::Debugger::Breakpoint>& breakpoints);
        void scroll_to_address(std::uint64_t address);
        void select_address(std::uint64_t address);

        [[nodiscard]] std::uint64_t get_selected_address() const;
        [[nodiscard]] std::optional<std::size_t> get_line_at_address(std::uint64_t address) const;

        enum class EdgeState : std::uint8_t
        {
            Idle = 0,
            Loading,
            EndOfRange,
            Error
        };

        void set_navigate_callback(NavigateCallback callback);
        void set_breakpoint_toggle_callback(BreakpointToggleCallback callback);
        void set_breakpoint_enable_callback(BreakpointEnableCallback callback);
        void set_breakpoint_remove_callback(BreakpointRemoveCallback callback);
        void set_breakpoint_edit_condition_callback(BreakpointEditConditionCallback callback);
        void set_run_to_cursor_callback(RunToCursorCallback callback);
        void set_selection_change_callback(SelectionChangeCallback callback);
        void set_scroll_boundary_callback(ScrollBoundaryCallback callback);
        void set_show_in_memory_callback(ShowInMemoryCallback callback);
        void set_xref_query_callback(XrefQueryCallback callback);

        void set_extension_result(bool isTop, ::Vertex::Debugger::ExtensionResult result);

        [[nodiscard]] EdgeState get_top_edge_state() const { return m_topEdgeState; }
        [[nodiscard]] EdgeState get_bottom_edge_state() const { return m_bottomEdgeState; }
        [[nodiscard]] bool is_fetching_more() const { return m_fetchingMore; }
        [[nodiscard]] bool is_loading_timer_running() const { return m_loadingAnimTimer.IsRunning(); }

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
        void render_edge_indicator(wxDC& dc, bool isTop) const;
        void on_loading_anim_timer(wxTimerEvent& event);
        void retry_extension(bool isTop);

        void render(wxDC& dc) const;
        void render_background(wxDC& dc) const;
        void render_arrow_gutter(wxDC& dc, int startLine, int endLine) const;
        void render_lines(wxDC& dc, int startLine, int endLine) const;
        void render_line(wxDC& dc, std::size_t lineIndex, int y) const;
        void render_breakpoint_marker(wxDC& dc, int x, int y, const BreakpointVisualInfo& info) const;
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

        void show_xrefs_dialog(const std::vector<::Vertex::Debugger::XrefEntry>& xrefs,
                               ::Vertex::Debugger::XrefDirection direction);

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

        static constexpr int MENU_ID_TOGGLE_BREAKPOINT = 1001;
        static constexpr int MENU_ID_RUN_TO_CURSOR = 1002;
        static constexpr int MENU_ID_FOLLOW_JUMP = 1003;
        static constexpr int MENU_ID_COPY_ADDRESS = 1004;
        static constexpr int MENU_ID_COPY_LINE = 1005;
        static constexpr int MENU_ID_XREFS_TO = 1006;
        static constexpr int MENU_ID_XREFS_FROM = 1007;
        static constexpr int MENU_ID_ENABLE_BREAKPOINT = 1008;
        static constexpr int MENU_ID_REMOVE_BREAKPOINT = 1009;
        static constexpr int MENU_ID_EDIT_CONDITION = 1010;
        static constexpr int MENU_ID_SHOW_IN_MEMORY = 1011;

        void load_system_colors();

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
            wxColour symbolLabel{0x4E, 0xC9, 0xB0};
            wxColour moduleContext{0x80, 0x80, 0x80};
            wxColour functionEntryLine{0x3E, 0x3E, 0x3E};

            wxColour arrowUnconditional{0x56, 0x9C, 0xD6};
            wxColour arrowConditional{0xC5, 0x86, 0xC0};
            wxColour arrowCall{0x4E, 0xC9, 0xB0};
            wxColour arrowLoop{0xD7, 0xBA, 0x7D};

            wxColour breakpointMarker{0xE5, 0x1A, 0x1A};
            wxColour breakpointMarkerDisabled{0xE5, 0x1A, 0x1A};
            wxColour breakpointMarkerPending{0xD4, 0xA0, 0x17};
            wxColour breakpointLineDisabled{0x3A, 0x2A, 0x2A};
            wxColour breakpointLinePending{0x3A, 0x35, 0x1F};
            wxColour currentMarker{0xFF, 0xD7, 0x00};

            wxColour gutter{0x2D, 0x2D, 0x2D};
            wxColour gutterBorder{0x3E, 0x3E, 0x3E};

            wxColour loadingText{0x56, 0x9C, 0xD6};
            wxColour endOfRangeText{0x6A, 0x99, 0x55};
            wxColour errorText{0xE5, 0x1A, 0x1A};
            wxColour errorRetryText{0x56, 0x9C, 0xD6};
            wxColour edgeIndicatorBg{0x1A, 0x1A, 0x2E};
        } m_colors;

        ::Vertex::Debugger::DisassemblyRange m_range{};
        std::unordered_map<std::uint64_t, std::size_t> m_addressToLine{};
        std::unordered_map<std::uint64_t, BreakpointVisualInfo> m_breakpointMap{};
        std::vector<ArrowInfo> m_arrows{};

        std::size_t m_selectedLine{};
        std::uint64_t m_currentInstructionAddress{};

        wxFont m_codeFont{};
        wxFont m_codeFontBold{};

        NavigateCallback m_navigateCallback{};
        BreakpointToggleCallback m_breakpointToggleCallback{};
        BreakpointEnableCallback m_breakpointEnableCallback{};
        BreakpointRemoveCallback m_breakpointRemoveCallback{};
        BreakpointEditConditionCallback m_breakpointEditConditionCallback{};
        RunToCursorCallback m_runToCursorCallback{};
        SelectionChangeCallback m_selectionChangeCallback{};
        ScrollBoundaryCallback m_scrollBoundaryCallback{};
        ShowInMemoryCallback m_showInMemoryCallback{};
        XrefQueryCallback m_xrefQueryCallback{};

        bool m_fetchingMore{};
        int m_wheelAccumulator{};
        int m_previousScrollY{};

        EdgeState m_topEdgeState{};
        EdgeState m_bottomEdgeState{};
        wxTimer m_loadingAnimTimer;
        int m_loadingAnimFrame{};

        std::shared_ptr<std::monostate> m_lifetimeSentinel{std::make_shared<std::monostate>()};

        DisassemblyHeader* m_header{};
        Language::ILanguage& m_languageService;
    };
} // namespace Vertex::View::Debugger
