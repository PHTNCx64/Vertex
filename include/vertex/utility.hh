//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <cstddef>

namespace Vertex
{
    constexpr auto* EMPTY_STRING = "";
    constexpr int INVALID_STATE = -1;

    namespace NumericSystem
    {
        constexpr int BINARY = 2;
        constexpr int OCTAL = 8;
        constexpr int DECIMAL = 10;
        constexpr int HEXADECIMAL = 16;
    }

    namespace StandardWidgetValues
    {
        constexpr int STANDARD_X_DIP = 800;
        constexpr int STANDARD_Y_DIP = 600;
        constexpr int STANDARD_BORDER = 5;
        constexpr int BORDER_TWICE = 10;
        constexpr int STANDARD_PROPORTION = 1;
        constexpr int NO_PROPORTION = 0;
        constexpr int ICON_SIZE = 24;
        constexpr int GAUGE_MAX_VALUE = 100;
        constexpr int GAUGE_HEIGHT = 16;
        constexpr int TIMER_INTERVAL_MS = 500;
        constexpr int SCAN_PROGRESS_INTERVAL_MS = 50;
        constexpr int SPIN_MIN_VALUE = 0;
        constexpr int SPIN_MAX_VALUE = 1024;
        constexpr int SPIN_DEFAULT_VALUE = 4;
        constexpr int COLUMN_PROPORTION_LARGE = 3;
        constexpr int AUI_TOOLBAR_ROW = 1;
        constexpr int TELETYPE_FONT_SIZE = 9;
        constexpr int COLUMN_WIDTH_FUNCTION = 150;
        constexpr int COLUMN_WIDTH_ADDRESS = 100;
        constexpr int COLUMN_WIDTH_MODULE = 100;
        constexpr int COLUMN_WIDTH_ORDINAL = 60;
        constexpr int INJECTOR_X_DIP = 450;
        constexpr int INJECTOR_Y_DIP = 300;
        constexpr int SLIDER_SCALE_FACTOR = 100;
        constexpr int GRID_COLUMNS = 2;
    }

    namespace MemoryViewValues
    {
        constexpr std::size_t BYTES_PER_ROW = 16;
        constexpr std::size_t INTERPRETATION_MAX_STRING_BYTES = 256;
    }

    namespace DisassemblyIndicatorValues
    {
        constexpr int LOADING_ANIM_INTERVAL_MS = 300;
        constexpr int LOADING_DOT_COUNT = 3;
        constexpr int INDICATOR_HEIGHT = 24;
    }

    namespace NewProcessDialogValues
    {
        constexpr int DIALOG_WIDTH = 450;
        constexpr int DIALOG_HEIGHT = 180;
    }

    namespace BreakpointConditionDialogValues
    {
        constexpr int DIALOG_WIDTH = 400;
        constexpr int DIALOG_HEIGHT = 350;
    }

    namespace AddAddressDialogValues
    {
        constexpr int DIALOG_WIDTH = 400;
        constexpr int DIALOG_HEIGHT = 200;
    }

    namespace ScriptingViewValues
    {
        constexpr int EDITOR_WIDTH = 700;
        constexpr int EDITOR_HEIGHT = 500;
        constexpr int LINE_NUMBER_MARGIN = 0;
        constexpr int LINE_NUMBER_MARGIN_WIDTH = 48;
        constexpr int FOLD_MARGIN = 1;
        constexpr int FOLD_MARGIN_WIDTH = 18;
        constexpr int DIAGNOSTIC_MARGIN = 2;
        constexpr int DIAGNOSTIC_MARGIN_WIDTH = 12;
        constexpr int DIAGNOSTIC_MARKER_ERROR = 3;
        constexpr int DIAGNOSTIC_MARKER_WARNING = 4;
        constexpr int SCRIPT_BREAKPOINT_MARKER = 5;
        constexpr int TAB_WIDTH = 4;
        constexpr int CARET_WIDTH = 2;
        constexpr int EDGE_COLUMN = 120;
        constexpr int CONTEXT_LIST_HEIGHT = 150;
        constexpr int VARIABLE_INSPECTOR_HEIGHT = 120;
        constexpr int OUTPUT_PANEL_HEIGHT = 140;
        constexpr int OUTPUT_STYLE_INFO = 20;
        constexpr int OUTPUT_STYLE_WARNING = 21;
        constexpr int OUTPUT_STYLE_ERROR = 22;
        constexpr int COLUMN_WIDTH_NAME = 300;
        constexpr int COLUMN_WIDTH_STATE = 120;
        constexpr int COLUMN_WIDTH_VARIABLE_NAME = 160;
        constexpr int COLUMN_WIDTH_VARIABLE_TYPE = 220;
        constexpr int COLUMN_WIDTH_VARIABLE_VALUE = 260;
        constexpr int MINIMAP_WIDTH = 140;
    }

    namespace ApplicationAppearance
    {
        constexpr int SYSTEM = 0;
        constexpr int LIGHT = 1;
        constexpr int DARK = 2;
    }

    namespace ViewModelName
    {
        constexpr auto* MAIN = "MainViewModel";
        constexpr auto* PROCESSLIST = "ProcessListViewModel";
        constexpr auto* SETTINGS = "SettingsViewModel";
        constexpr auto* MEMORYATTRIBUTES = "MemoryAttributesViewModel";
        constexpr auto* ANALYTICS = "AnalyticsViewModel";
        constexpr auto* DEBUGGER = "DebuggerViewModel";
        constexpr auto* INJECTOR = "InjectorViewModel";
        constexpr auto* PLUGINCONFIG = "PluginConfigViewModel";
        constexpr auto* SCRIPTING = "ScriptingViewModel";
        constexpr auto* ACCESS_TRACKER = "AccessTrackerViewModel";
    }

    enum class ViewUpdateFlags : unsigned int
    {
        NONE = 0,
        PROCESS_INFO = 1 << 0,
        SCAN_PROGRESS = 1 << 1,
        SCANNED_VALUES = 1 << 2,
        BUTTON_STATES = 1 << 3,
        INPUT_VISIBILITY = 1 << 4,
        DATATYPES = 1 << 5,
        SCAN_MODES = 1 << 6,
        SCAN_COMPLETED = 1 << 7,
        ALL = PROCESS_INFO | SCAN_PROGRESS | SCANNED_VALUES | BUTTON_STATES | INPUT_VISIBILITY | DATATYPES | SCAN_MODES,

        DEBUGGER_DISASSEMBLY = 1 << 10,
        DEBUGGER_BREAKPOINTS = 1 << 11,
        DEBUGGER_REGISTERS = 1 << 12,
        DEBUGGER_STACK = 1 << 13,
        DEBUGGER_MEMORY = 1 << 14,
        DEBUGGER_IMPORTS_EXPORTS = 1 << 15,
        DEBUGGER_STATE = 1 << 16,
        DEBUGGER_THREADS = 1 << 17,
        DEBUGGER_WATCHPOINTS = 1 << 18,
        DEBUGGER_MODULES = 1 << 19,
        DEBUGGER_WATCH = 1 << 20,
        DEBUGGER_CONSOLE = 1 << 21,
        DEBUGGER_ALL = DEBUGGER_DISASSEMBLY | DEBUGGER_BREAKPOINTS | DEBUGGER_REGISTERS |
                       DEBUGGER_STACK | DEBUGGER_MEMORY | DEBUGGER_IMPORTS_EXPORTS | DEBUGGER_STATE |
                       DEBUGGER_THREADS | DEBUGGER_WATCHPOINTS | DEBUGGER_MODULES | DEBUGGER_WATCH |
                       DEBUGGER_CONSOLE
    };

    [[nodiscard]] constexpr ViewUpdateFlags operator|(ViewUpdateFlags a, ViewUpdateFlags b)
    {
        return static_cast<ViewUpdateFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    [[nodiscard]] constexpr ViewUpdateFlags operator&(ViewUpdateFlags a, ViewUpdateFlags b)
    {
        return static_cast<ViewUpdateFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    [[nodiscard]] constexpr bool has_flag(const ViewUpdateFlags flags, const ViewUpdateFlags flag)
    {
        return (flags & flag) == flag;
    }

    namespace StandardMenuIds::MainViewIds
    {
        enum
        {
            ID_NEW_PROJECT = 6001,
            ID_OPEN_PROJECT,
            ID_EXIT_APPLICATION,
            ID_SETTINGS,
            ID_PROCESS_LIST,
            ID_KILL_PROCESS,
            ID_CLOSE_PROCESS,
            ID_NEW_PROCESS,
            ID_DEBUGGER,
            ID_HELP_ABOUT,
            ID_ANALYTICS,
            ID_INJECTOR,
            ID_SCRIPTING,
        };
    }

    namespace AccessTrackerValues
    {
        constexpr int COL_INSTRUCTION_WIDTH = 200;
        constexpr int COL_MNEMONIC_WIDTH = 220;
        constexpr int COL_MODULE_WIDTH = 160;
        constexpr int COL_FUNCTION_WIDTH = 220;
        constexpr int COL_HITS_WIDTH = 80;
        constexpr int COL_ACCESS_WIDTH = 60;
        constexpr int COL_SIZE_WIDTH = 60;
        constexpr int COL_REGISTERS_WIDTH = 360;
        constexpr int COL_CALLER_WIDTH = 220;
        constexpr int TOOLBAR_GAP_SMALL = 4;
        constexpr int TOOLBAR_GAP_MEDIUM = 8;
        constexpr int TOOLBAR_GAP_LARGE = 16;
        constexpr int STATUS_LABEL_GRAY = 128;
        constexpr std::size_t REGISTERS_PREVIEW_LIMIT = 4;
    }

    namespace AccessTrackerCallStackDialogValues
    {
        constexpr int DIALOG_WIDTH = 720;
        constexpr int DIALOG_HEIGHT = 420;
        constexpr int COLUMN_WIDTH_INDEX = 60;
        constexpr int COLUMN_WIDTH_ADDRESS = 160;
        constexpr int COLUMN_WIDTH_MODULE = 180;
        constexpr int COLUMN_WIDTH_FUNCTION = 300;
    }

    namespace XrefDialogValues
    {
        constexpr int DIALOG_WIDTH = 500;
        constexpr int DIALOG_HEIGHT = 350;
        constexpr int COLUMN_WIDTH_TYPE = 120;
        constexpr int COLUMN_WIDTH_ADDRESS = 150;
        constexpr int COLUMN_WIDTH_SYMBOL = 200;
    }

    namespace FileTypes
    {
#if defined (_WIN32) || defined(_WIN64)
        constexpr auto* PLUGIN_EXTENSION = ".dll";
#elif defined (__linux) || defined (linux) || defined(__linux__)
        constexpr auto* PLUGIN_EXTENSION = ".so";
#endif

        constexpr auto* CONFIGURATION_EXTENSION = ".json";
        constexpr auto* SCRIPTING_EXTENSION = ".vscr";
    }
}
