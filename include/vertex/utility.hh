//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

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
        constexpr int TIMER_INTERVAL_MS = 500;
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
        constexpr auto* POINTERSCAN = "PointerScanViewModel";
        constexpr auto* POINTERSCAN_MEMORYATTRIBUTES = "PointerScanMemoryAttributesViewModel";
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
        DEBUGGER_ALL = DEBUGGER_DISASSEMBLY | DEBUGGER_BREAKPOINTS | DEBUGGER_REGISTERS |
                       DEBUGGER_STACK | DEBUGGER_MEMORY | DEBUGGER_IMPORTS_EXPORTS | DEBUGGER_STATE |
                       DEBUGGER_THREADS | DEBUGGER_WATCHPOINTS
    };

    [[nodiscard]] inline ViewUpdateFlags operator|(ViewUpdateFlags a, ViewUpdateFlags b)
    {
        return static_cast<ViewUpdateFlags>(static_cast<unsigned int>(a) | static_cast<unsigned int>(b));
    }

    [[nodiscard]] inline ViewUpdateFlags operator&(ViewUpdateFlags a, ViewUpdateFlags b)
    {
        return static_cast<ViewUpdateFlags>(static_cast<unsigned int>(a) & static_cast<unsigned int>(b));
    }

    [[nodiscard]] inline bool has_flag(const ViewUpdateFlags flags, const ViewUpdateFlags flag)
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
        };
    }

    namespace FileTypes
    {
#if defined (_WIN32) || defined(_WIN64)
        constexpr auto* PLUGIN_EXTENSION = ".dll";
#endif

        constexpr auto* CONFIGURATION_EXTENSION = ".json";
    }
}
