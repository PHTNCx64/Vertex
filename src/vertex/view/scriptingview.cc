//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/view/scriptingview.hh>

#include <fmt/format.h>

#include <vertex/event/types/scriptoutputevent.hh>
#include <vertex/event/types/scriptdiagnosticevent.hh>
#include <vertex/utility.hh>
#include <wx/settings.h>

#include <wx/filedlg.h>
#include <wx/msgdlg.h>
#include <wx/app.h>
#include <wx/accel.h>
#include <wx/datetime.h>
#include <wx/menu.h>
#include <wx/numdlg.h>
#include <wx/stattext.h>
#include <wx/textdlg.h>
#include <wx/sizer.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <cstddef>
#include <ranges>
#include <string_view>
#include <unordered_map>
#include <utility>

namespace Vertex::View
{
    namespace
    {
        constexpr std::size_t SCRIPT_BROWSER_REFRESH_INTERVAL_TICKS = 10;
        constexpr std::size_t MAX_RECENT_SCRIPTS = 10;

        struct ScriptBrowserItemData final : wxTreeItemData
        {
            ScriptBrowserItemData(std::filesystem::path itemPath, const bool directory)
                : path(std::move(itemPath)), isDirectory(directory)
            {
            }

            std::filesystem::path path;
            bool isDirectory{};
        };

        [[nodiscard]] ScriptBrowserItemData* script_browser_item_data(const wxTreeCtrl& tree, const wxTreeItemId& item)
        {
            return dynamic_cast<ScriptBrowserItemData*>(tree.GetItemData(item));
        }

        [[nodiscard]] bool paths_equal(const std::filesystem::path& lhs, const std::filesystem::path& rhs)
        {
            std::error_code errorCode{};
            if (std::filesystem::equivalent(lhs, rhs, errorCode) && !errorCode)
            {
                return true;
            }

            return lhs.lexically_normal() == rhs.lexically_normal();
        }

        [[nodiscard]] std::string to_lower_ascii(std::string_view text)
        {
            std::string lowered{text};
            std::ranges::transform(lowered, lowered.begin(),
                [](const unsigned char character)
                {
                    return static_cast<char>(std::tolower(character));
                });
            return lowered;
        }

        [[nodiscard]] bool is_script_file(const std::filesystem::path& path)
        {
            auto extension = to_lower_ascii(path.extension().string());
            return extension == FileTypes::SCRIPTING_EXTENSION;
        }

        [[nodiscard]] wxString recent_script_menu_label(const std::filesystem::path& path)
        {
            const auto fileName = path.filename().string();
            const auto parentPath = path.parent_path().string();

            if (parentPath.empty())
            {
                return wxString::FromUTF8(fileName);
            }

            return wxString::FromUTF8(fmt::format("{} ({})", fileName, parentPath));
        }

        [[nodiscard]] std::vector<int> editor_breakpoint_lines(wxStyledTextCtrl& editor)
        {
            std::vector<int> lines{};
            constexpr int markerMask = 1 << ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER;
            lines.reserve(static_cast<std::size_t>(editor.GetLineCount()));

            for (const auto line : std::views::iota(0, editor.GetLineCount()))
            {
                if ((editor.MarkerGet(line) & markerMask) != 0)
                {
                    lines.push_back(line + 1);
                }
            }

            return lines;
        }

        [[nodiscard]] std::string trim_ascii_copy(std::string_view text)
        {
            std::size_t start{};
            while (start < text.size() &&
                std::isspace(static_cast<unsigned char>(text[start])) != 0)
            {
                ++start;
            }

            std::size_t end = text.size();
            while (end > start &&
                std::isspace(static_cast<unsigned char>(text[end - 1])) != 0)
            {
                --end;
            }

            return std::string{text.substr(start, end - start)};
        }

        [[nodiscard]] bool has_path_separator(std::string_view text)
        {
            return text.find('/') != std::string_view::npos ||
                   text.find('\\') != std::string_view::npos;
        }

        [[nodiscard]] bool path_is_within(const std::filesystem::path& candidate,
                                          const std::filesystem::path& directory)
        {
            const auto normalizedCandidate = candidate.lexically_normal();
            const auto normalizedDirectory = directory.lexically_normal();

            auto candidateIt = normalizedCandidate.begin();
            auto directoryIt = normalizedDirectory.begin();
            for (; directoryIt != normalizedDirectory.end(); ++directoryIt, ++candidateIt)
            {
                if (candidateIt == normalizedCandidate.end())
                {
                    return false;
                }

                if (*candidateIt != *directoryIt)
                {
                    return false;
                }
            }

            return true;
        }

        void collect_expanded_script_browser_paths(wxTreeCtrl& tree,
                                                   const wxTreeItemId& parent,
                                                   std::vector<std::filesystem::path>& expandedPaths)
        {
            if (!parent.IsOk())
            {
                return;
            }

            if (tree.IsExpanded(parent))
            {
                if (auto* data = script_browser_item_data(tree, parent); data && data->isDirectory)
                {
                    expandedPaths.push_back(data->path);
                }
            }

            wxTreeItemIdValue cookie{};
            for (auto child = tree.GetFirstChild(parent, cookie);
                 child.IsOk();
                 child = tree.GetNextChild(parent, cookie))
            {
                collect_expanded_script_browser_paths(tree, child, expandedPaths);
            }
        }

        [[nodiscard]] wxTreeItemId find_script_browser_item(wxTreeCtrl& tree,
                                                            const wxTreeItemId& parent,
                                                            const std::filesystem::path& targetPath)
        {
            if (!parent.IsOk())
            {
                return {};
            }

            if (auto* data = script_browser_item_data(tree, parent);
                data && !data->path.empty() && paths_equal(data->path, targetPath))
            {
                return parent;
            }

            wxTreeItemIdValue cookie{};
            for (auto child = tree.GetFirstChild(parent, cookie);
                 child.IsOk();
                 child = tree.GetNextChild(parent, cookie))
            {
                if (const auto found = find_script_browser_item(tree, child, targetPath); found.IsOk())
                {
                    return found;
                }
            }

            return {};
        }

        void expand_script_browser_item(wxTreeCtrl& tree, const wxTreeItemId& item)
        {
            if (!item.IsOk())
            {
                return;
            }

            if (tree.HasFlag(wxTR_HIDE_ROOT) && item == tree.GetRootItem())
            {
                return;
            }

            tree.Expand(item);
        }

        [[nodiscard]] bool is_identifier_char(const int character)
        {
            if (character < 0 || character > 255)
            {
                return false;
            }

            const auto byte = static_cast<unsigned char>(character);
            return std::isalnum(byte) != 0 || character == '_';
        }

        [[nodiscard]] bool starts_with_case_insensitive(std::string_view text, std::string_view prefix)
        {
            if (prefix.size() > text.size())
            {
                return false;
            }

            for (std::size_t i{}; i < prefix.size(); ++i)
            {
                const auto textChar = static_cast<unsigned char>(text[i]);
                const auto prefixChar = static_cast<unsigned char>(prefix[i]);
                if (std::tolower(textChar) != std::tolower(prefixChar))
                {
                    return false;
                }
            }

            return true;
        }

        void normalize_completion_words(std::vector<std::string>& words)
        {
            std::ranges::sort(words);
            words.erase(std::ranges::unique(words).begin(), words.end());
        }

        template <std::size_t N>
        void append_completion_words(std::vector<std::string>& destination, const std::array<const char*, N>& source)
        {
            destination.reserve(destination.size() + source.size());
            for (const auto* word : source)
            {
                destination.emplace_back(word);
            }
        }

        [[nodiscard]] std::string build_autocomplete_payload(const std::vector<std::string>& words, std::string_view prefix)
        {
            std::string payload{};
            for (const auto& word : words)
            {
                if (!prefix.empty() && !starts_with_case_insensitive(word, prefix))
                {
                    continue;
                }

                if (!payload.empty())
                {
                    payload.push_back(' ');
                }

                payload.append(word);
            }

            return payload;
        }

        [[nodiscard]] std::string current_word_prefix(wxStyledTextCtrl& editor)
        {
            const int currentPosition = editor.GetCurrentPos();
            const int start = editor.WordStartPosition(currentPosition, true);
            if (start == wxSTC_INVALID_POSITION || start >= currentPosition)
            {
                return {};
            }

            return editor.GetTextRange(start, currentPosition).ToStdString();
        }

        [[nodiscard]] std::string symbol_before_dot(wxStyledTextCtrl& editor)
        {
            const int currentPosition = editor.GetCurrentPos();
            const int dotPosition = currentPosition - 1;
            if (dotPosition <= 0 || editor.GetCharAt(dotPosition) != '.')
            {
                return {};
            }

            int start = dotPosition;
            while (start > 0 && is_identifier_char(editor.GetCharAt(start - 1)))
            {
                --start;
            }

            if (start == dotPosition)
            {
                return {};
            }

            return editor.GetTextRange(start, dotPosition).ToStdString();
        }

        [[nodiscard]] std::string function_name_before_paren(wxStyledTextCtrl& editor)
        {
            const int currentPosition = editor.GetCurrentPos();
            const int parenPosition = currentPosition - 1;
            if (parenPosition < 0 || editor.GetCharAt(parenPosition) != '(')
            {
                return {};
            }

            int end = parenPosition - 1;
            while (end >= 0)
            {
                const int character = editor.GetCharAt(end);
                if (character < 0 || character > 255 ||
                    std::isspace(static_cast<unsigned char>(character)) == 0)
                {
                    break;
                }
                --end;
            }

            if (end < 0)
            {
                return {};
            }

            int start = end;
            while (start >= 0 && is_identifier_char(editor.GetCharAt(start)))
            {
                --start;
            }
            ++start;

            if (start > end)
            {
                return {};
            }

            return editor.GetTextRange(start, end + 1).ToStdString();
        }

        void apply_newline_indentation(wxStyledTextCtrl& editor)
        {
            const int currentLine = editor.GetCurrentLine();
            if (currentLine <= 0)
            {
                return;
            }

            const int previousLine = currentLine - 1;
            int targetIndentation = editor.GetLineIndentation(previousLine);

            auto previousText = editor.GetLine(previousLine).ToStdString();
            while (!previousText.empty() &&
                std::isspace(static_cast<unsigned char>(previousText.back())) != 0)
            {
                previousText.pop_back();
            }

            if (!previousText.empty() && previousText.back() == '{')
            {
                targetIndentation += editor.GetIndent();
            }

            editor.SetLineIndentation(currentLine, targetIndentation);
            const int caretPosition = editor.GetLineIndentPosition(currentLine);
            editor.SetCurrentPos(caretPosition);
            editor.SetAnchor(caretPosition);
        }

        void apply_closing_brace_outdent(wxStyledTextCtrl& editor)
        {
            const int currentLine = editor.GetCurrentLine();
            const int firstNonWhitespace = editor.GetLineIndentPosition(currentLine);
            if (editor.GetCharAt(firstNonWhitespace) != '}')
            {
                return;
            }

            const int currentIndentation = editor.GetLineIndentation(currentLine);
            const int indentStep = editor.GetIndent();
            if (indentStep <= 0 || currentIndentation <= 0)
            {
                return;
            }

            const int previousCurrentPos = editor.GetCurrentPos();
            const int newIndentation = std::max(0, currentIndentation - indentStep);
            editor.SetLineIndentation(currentLine, newIndentation);

            const int positionDelta = newIndentation - currentIndentation;
            const int lineStart = editor.PositionFromLine(currentLine);
            const int adjustedPosition = std::max(lineStart, previousCurrentPos + positionDelta);
            editor.SetCurrentPos(adjustedPosition);
            editor.SetAnchor(adjustedPosition);
        }

        [[nodiscard]] auto build_autocomplete_data()
            -> std::pair<std::vector<std::string>, std::unordered_map<std::string, std::vector<std::string>>>
        {
            constexpr std::array languageKeywordCompletions{
                "void", "int", "uint", "float", "double", "bool", "string",
                "int8", "int16", "int32", "int64", "uint8", "uint16", "uint32", "uint64",
                "if", "else", "for", "while", "do", "switch", "case", "default", "break", "continue", "return",
                "class", "interface", "enum", "namespace", "import", "from",
                "cast", "private", "protected", "const", "override", "final",
                "null", "true", "false", "this", "super",
                "funcdef", "typedef", "mixin", "shared", "external", "abstract",
                "try", "catch", "auto", "in", "out", "inout",
                "get", "set", "is", "not", "and", "or", "xor"
            };

            constexpr std::array builtinFunctionCompletions{
                "yield", "sleep", "wait_until", "wait_while", "wait_ticks"
            };

            constexpr std::array stdlibFunctionCompletions{
                "log_info", "log_warn", "log_error",
                "is_process_open", "get_process_name", "get_process_id", "close_process", "kill_process",
                "open_process", "refresh_process_list", "get_process_count", "get_process_at",
                "refresh_modules_list", "get_module_count", "get_module_at",
                "read_memory", "write_memory", "bulk_read", "bulk_write", "allocate_memory", "free_memory",
                "get_pointer_size", "get_min_address", "get_max_address",
                "refresh_memory_regions", "get_region_count", "get_region_at",
                "get_current_plugin", "get_host_os", "get_version", "get_vendor", "get_name",
                "ui_create_frame", "ui_create_dialog", "ui_create_box_sizer", "ui_message_box"
            };

            constexpr std::array typeCompletions{
                "StatusCode", "CoroutineCondition",
                "ProcessInfo", "ModuleInfo", "MemoryRegion",
                "BulkReadEntry", "BulkReadResult", "BulkWriteEntry", "BulkWriteResult",
                "Frame", "Dialog", "Panel", "Button", "TextInput", "CheckBox", "ComboBox", "Label", "BoxSizer", "GroupBox"
            };

            constexpr std::array uiConstantCompletions{
                "UI_VERTICAL", "UI_HORIZONTAL", "UI_EXPAND", "UI_ALL",
                "UI_LEFT", "UI_RIGHT", "UI_TOP", "UI_BOTTOM", "UI_CENTER",
                "UI_OK", "UI_OK_CANCEL", "UI_YES_NO",
                "UI_ICON_INFO", "UI_ICON_WARNING", "UI_ICON_ERROR", "UI_ICON_QUESTION"
            };

            constexpr std::array statusCodeCompletions{
                "STATUS_OK", "STATUS_ERROR_GENERAL", "STATUS_ERROR_INVALID_PARAMETER",
                "STATUS_ERROR_NOT_IMPLEMENTED", "STATUS_ERROR_FUNCTION_NOT_FOUND",
                "STATUS_ERROR_PLUGIN_NOT_ACTIVE", "STATUS_ERROR_PLUGIN_NOT_LOADED",
                "STATUS_ERROR_PLUGIN_FUNCTION_NOT_IMPLEMENTED", "STATUS_ERROR_PROCESS_ACCESS_DENIED",
                "STATUS_ERROR_PROCESS_INVALID", "STATUS_ERROR_PROCESS_NOT_FOUND",
                "STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL", "STATUS_ERROR_MEMORY_OPERATION_ABORTED",
                "STATUS_ERROR_MEMORY_READ", "STATUS_ERROR_MEMORY_WRITE",
                "STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED"
            };

            std::vector<std::string> completions{};
            append_completion_words(completions, languageKeywordCompletions);
            append_completion_words(completions, builtinFunctionCompletions);
            append_completion_words(completions, stdlibFunctionCompletions);
            append_completion_words(completions, typeCompletions);
            append_completion_words(completions, uiConstantCompletions);
            append_completion_words(completions, statusCodeCompletions);
            normalize_completion_words(completions);

            std::unordered_map<std::string, std::vector<std::string>> memberCompletions{
                {"ProcessInfo", {"name", "owner", "id", "parentId"}},
                {"ModuleInfo", {"name", "path", "baseAddress", "size"}},
                {"MemoryRegion", {"moduleName", "baseAddress", "regionSize"}},
                {"BulkReadEntry", {"address", "size"}},
                {"BulkReadResult", {"status", "data"}},
                {"BulkWriteEntry", {"address", "data"}},
                {"BulkWriteResult", {"status"}},
                {"Frame", {"show", "hide", "close", "set_title", "set_size", "set_sizer", "create_panel", "from_dip", "is_open"}},
                {"Dialog", {"show", "hide", "show_modal", "end_modal", "close", "set_title", "set_size", "set_sizer", "create_panel", "from_dip", "is_open"}},
                {"Panel", {"add_button", "add_text_input", "add_checkbox", "add_combobox", "add_label", "add_panel", "create_group_box", "set_sizer", "from_dip"}},
                {"Button", {"set_label", "get_label", "enable", "is_enabled"}},
                {"TextInput", {"set_value", "get_value", "set_read_only", "enable", "is_enabled"}},
                {"CheckBox", {"set_checked", "is_checked", "set_label", "get_label", "enable", "is_enabled"}},
                {"ComboBox", {"add_item", "clear_items", "get_selection", "set_selection", "get_value", "enable", "is_enabled"}},
                {"Label", {"set_text", "get_text"}},
                {"BoxSizer", {"add", "add_spacer", "add_stretch"}},
                {"GroupBox", {"add", "add_spacer", "add_stretch"}}
            };

            for (auto& words : memberCompletions | std::views::values)
            {
                normalize_completion_words(words);
            }

            std::vector<std::pair<std::string, std::vector<std::string>>> lowercaseAliases{};
            lowercaseAliases.reserve(memberCompletions.size());
            for (const auto& [symbol, words] : memberCompletions)
            {
                auto lowered = to_lower_ascii(symbol);
                if (lowered == symbol)
                {
                    continue;
                }

                lowercaseAliases.emplace_back(std::move(lowered), words);
            }

            for (auto& [symbol, words] : lowercaseAliases)
            {
                memberCompletions.emplace(std::move(symbol), std::move(words));
            }

            return {std::move(completions), std::move(memberCompletions)};
        }

        [[nodiscard]] std::unordered_map<std::string, std::string> build_call_tip_data()
        {
            std::unordered_map<std::string, std::string> callTips{
                {"yield", "yield() -> void"},
                {"sleep", "sleep(uint milliseconds) -> void"},
                {"wait_until", "wait_until(CoroutineCondition@ condition) -> void"},
                {"wait_while", "wait_while(CoroutineCondition@ condition) -> void"},
                {"wait_ticks", "wait_ticks(uint ticks) -> void"},

                {"log_info", "log_info(const string &in message) -> void"},
                {"log_warn", "log_warn(const string &in message) -> void"},
                {"log_error", "log_error(const string &in message) -> void"},

                {"is_process_open", "is_process_open() -> bool"},
                {"get_process_name", "get_process_name() -> string"},
                {"get_process_id", "get_process_id() -> uint"},
                {"close_process", "close_process() -> int"},
                {"kill_process", "kill_process() -> int"},
                {"open_process", "open_process(uint processId) -> int"},
                {"refresh_process_list", "refresh_process_list() -> int"},
                {"get_process_count", "get_process_count() -> uint"},
                {"get_process_at", "get_process_at(uint index) -> ProcessInfo"},
                {"refresh_modules_list", "refresh_modules_list() -> int"},
                {"get_module_count", "get_module_count() -> uint"},
                {"get_module_at", "get_module_at(uint index) -> ModuleInfo"},

                {"read_memory", "read_memory(uint64 address, uint size, string &out data) -> int"},
                {"write_memory", "write_memory(uint64 address, const string &in data) -> int"},
                {"bulk_read", "bulk_read(array<BulkReadEntry>@ entries, array<BulkReadResult>@ &out results) -> int"},
                {"bulk_write", "bulk_write(array<BulkWriteEntry>@ entries, array<BulkWriteResult>@ &out results) -> int"},
                {"allocate_memory", "allocate_memory(uint64 address, uint64 size, uint64 &out resultAddress) -> int"},
                {"free_memory", "free_memory(uint64 address, uint64 size) -> int"},
                {"get_pointer_size", "get_pointer_size(uint64 &out pointerSize) -> int"},
                {"get_min_address", "get_min_address(uint64 &out minAddress) -> int"},
                {"get_max_address", "get_max_address(uint64 &out maxAddress) -> int"},
                {"refresh_memory_regions", "refresh_memory_regions() -> int"},
                {"get_region_count", "get_region_count() -> uint"},
                {"get_region_at", "get_region_at(uint index) -> MemoryRegion"},

                {"get_current_plugin", "get_current_plugin() -> string"},
                {"get_host_os", "get_host_os() -> string"},
                {"get_version", "get_version() -> string"},
                {"get_vendor", "get_vendor() -> string"},
                {"get_name", "get_name() -> string"},

                {"ui_create_frame", "ui_create_frame(const string &in title, int width, int height) -> Frame@"},
                {"ui_create_dialog", "ui_create_dialog(const string &in title, int width, int height) -> Dialog@"},
                {"ui_create_box_sizer", "ui_create_box_sizer(int orientation) -> BoxSizer@"},
                {"ui_message_box", "ui_message_box(const string &in message, const string &in caption, int flags) -> void"},

                {"show", "show() -> void"},
                {"hide", "hide() -> void"},
                {"close", "close() -> void"},
                {"set_title", "set_title(const string &in title) -> void"},
                {"set_size", "set_size(int width, int height) -> void"},
                {"set_sizer", "set_sizer(BoxSizer@ sizer) -> void"},
                {"create_panel", "create_panel() -> Panel@"},
                {"from_dip", "from_dip(int value) -> int"},
                {"is_open", "is_open() -> bool"},
                {"show_modal", "show_modal() -> int"},
                {"end_modal", "end_modal(int returnCode) -> void"},

                {"add_button", "add_button(const string &in label) -> Button@"},
                {"add_text_input", "add_text_input(const string &in value) -> TextInput@"},
                {"add_checkbox", "add_checkbox(const string &in label) -> CheckBox@"},
                {"add_combobox", "add_combobox() -> ComboBox@"},
                {"add_label", "add_label(const string &in text) -> Label@"},
                {"add_panel", "add_panel() -> Panel@"},
                {"create_group_box", "create_group_box(const string &in title, int orientation) -> GroupBox@"},

                {"set_label", "set_label(const string &in label) -> void"},
                {"get_label", "get_label() -> string"},
                {"enable", "enable(bool enabled) -> void"},
                {"is_enabled", "is_enabled() -> bool"},
                {"set_value", "set_value(const string &in value) -> void"},
                {"get_value", "get_value() -> string"},
                {"set_read_only", "set_read_only(bool readOnly) -> void"},
                {"set_checked", "set_checked(bool checked) -> void"},
                {"is_checked", "is_checked() -> bool"},
                {"add_item", "add_item(const string &in item) -> void"},
                {"clear_items", "clear_items() -> void"},
                {"get_selection", "get_selection() -> int"},
                {"set_selection", "set_selection(int index) -> void"},
                {"set_text", "set_text(const string &in text) -> void"},
                {"get_text", "get_text() -> string"},

                {"add", "add(widget, int proportion, int flags, int border) -> void"},
                {"add_spacer", "add_spacer(int size) -> void"},
                {"add_stretch", "add_stretch(int proportion) -> void"}
            };

            std::vector<std::pair<std::string, std::string>> lowercaseAliases{};
            lowercaseAliases.reserve(callTips.size());
            for (const auto& [name, signature] : callTips)
            {
                auto lowered = to_lower_ascii(name);
                if (lowered == name || callTips.contains(lowered))
                {
                    continue;
                }

                lowercaseAliases.emplace_back(std::move(lowered), signature);
            }

            for (auto& [name, signature] : lowercaseAliases)
            {
                callTips.emplace(std::move(name), std::move(signature));
            }

            return callTips;
        }

        [[nodiscard]] int output_style_from_severity(const Event::ScriptOutputSeverity severity)
        {
            switch (severity)
            {
                case Event::ScriptOutputSeverity::Info:
                    return ScriptingViewValues::OUTPUT_STYLE_INFO;
                case Event::ScriptOutputSeverity::Warning:
                    return ScriptingViewValues::OUTPUT_STYLE_WARNING;
                case Event::ScriptOutputSeverity::Error:
                    return ScriptingViewValues::OUTPUT_STYLE_ERROR;
            }

            std::unreachable();
        }

        [[nodiscard]] int diagnostic_marker_from_severity(const Event::ScriptDiagnosticSeverity severity)
        {
            switch (severity)
            {
                case Event::ScriptDiagnosticSeverity::Info:
                    return -1;
                case Event::ScriptDiagnosticSeverity::Warning:
                    return ScriptingViewValues::DIAGNOSTIC_MARKER_WARNING;
                case Event::ScriptDiagnosticSeverity::Error:
                    return ScriptingViewValues::DIAGNOSTIC_MARKER_ERROR;
            }

            std::unreachable();
        }
    }

    enum ScriptingCommandIds
    {
        ID_SCRIPT_OPEN = 20000,
        ID_SCRIPT_NEW_TAB,
        ID_SCRIPT_CLOSE_TAB,
        ID_SCRIPT_SAVE,
        ID_SCRIPT_SAVE_AS,
        ID_SCRIPT_FIND,
        ID_SCRIPT_REPLACE,
        ID_SCRIPT_GOTO_LINE,
        ID_SCRIPT_TEMPLATE_BASIC,
        ID_SCRIPT_TEMPLATE_PROCESS_MONITOR,
        ID_SCRIPT_TEMPLATE_MEMORY_PATCHER,
        ID_SCRIPT_TEMPLATE_UI_TOOL,
        ID_SCRIPT_TEMPLATE_BULK_MEMORY_READ,
        ID_SCRIPT_TEMPLATE_PROCESS_MODULES_SNAPSHOT,
        ID_SCRIPT_STOP_CONTEXT,
        ID_SCRIPT_SUSPEND_CONTEXT,
        ID_SCRIPT_RESUME_CONTEXT,
        ID_SCRIPT_EXECUTE,
        ID_SCRIPT_CLEAR_OUTPUT,
        ID_SCRIPT_CLEAR
    };

    struct ScriptTemplateDefinition final
    {
        int commandId{};
        std::string_view translationKey{};
        std::string_view fallbackLabel{};
        std::string_view fileName{};
    };

    constexpr std::array SCRIPT_TEMPLATES{
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_BASIC,
            .translationKey = "scriptingView.ui.templateBasic",
            .fallbackLabel = "Basic",
            .fileName = "basic.vscr"},
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_PROCESS_MONITOR,
            .translationKey = "scriptingView.ui.templateProcessMonitor",
            .fallbackLabel = "Process Monitor",
            .fileName = "process_monitor.vscr"},
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_MEMORY_PATCHER,
            .translationKey = "scriptingView.ui.templateMemoryPatcher",
            .fallbackLabel = "Memory API",
            .fileName = "memory_patcher.vscr"},
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_UI_TOOL,
            .translationKey = "scriptingView.ui.templateUITool",
            .fallbackLabel = "UI Tool",
            .fileName = "ui_tool.vscr"},
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_BULK_MEMORY_READ,
            .translationKey = "scriptingView.ui.templateBulkMemoryRead",
            .fallbackLabel = "Bulk Memory Read",
            .fileName = "bulk_memory_read.vscr"},
        ScriptTemplateDefinition{
            .commandId = ID_SCRIPT_TEMPLATE_PROCESS_MODULES_SNAPSHOT,
            .translationKey = "scriptingView.ui.templateProcessModulesSnapshot",
            .fallbackLabel = "Module Snapshot",
            .fileName = "process_modules_snapshot.vscr"}};

    [[nodiscard]] const ScriptTemplateDefinition* script_template_definition(const int commandId)
    {
        const auto it = std::ranges::find_if(SCRIPT_TEMPLATES,
            [commandId](const ScriptTemplateDefinition& definition)
            {
                return definition.commandId == commandId;
            });
        return it != SCRIPT_TEMPLATES.end() ? &(*it) : nullptr;
    }

    [[nodiscard]] std::string script_template_label(
        Language::ILanguage& languageService,
        const ScriptTemplateDefinition& templateDefinition)
    {
        auto translated = languageService.fetch_translation(templateDefinition.translationKey);
        if (!translated.empty() && translated != "[NO_TRANSLATION_PROVIDED]")
        {
            return translated;
        }

        return std::string{templateDefinition.fallbackLabel};
    }

    ScriptingView::ScriptingView(Language::ILanguage& languageService,
                                  std::unique_ptr<ViewModel::ScriptingViewModel> viewModel,
                                  Gui::IIconManager& iconManager)
        : wxDialog(wxTheApp->GetTopWindow(), wxID_ANY,
                    wxString::FromUTF8(languageService.fetch_translation("scriptingView.ui.title")),
                    wxDefaultPosition, wxSize(ScriptingViewValues::EDITOR_WIDTH, ScriptingViewValues::EDITOR_HEIGHT),
                    wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMAXIMIZE_BOX | wxMINIMIZE_BOX),
          m_refreshTimer{this},
          m_viewModel{std::move(viewModel)},
          m_languageService{languageService},
          m_iconManager{iconManager}
    {
        m_viewModel->set_event_callback(
            [this](const Event::EventId eventId, const Event::VertexEvent& event)
            {
                vertex_event_callback(eventId, event);
            });

        create_controls();
        create_editor_tab({}, {}, true);
        initialize_autocomplete();
        initialize_call_tips();
        configure_editor();
        layout_controls();
        bind_events();
        m_refreshTimer.Start(StandardWidgetValues::TIMER_INTERVAL_MS);
    }

    void ScriptingView::vertex_event_callback(const Event::EventId eventId, const Event::VertexEvent& event)
    {
        if (eventId == Event::VIEW_EVENT)
        {
            std::ignore = toggle_view();
            return;
        }

        if (eventId != Event::SCRIPT_OUTPUT_EVENT)
        {
            if (eventId != Event::SCRIPT_DIAGNOSTIC_EVENT)
            {
                return;
            }

            const auto& diagnosticEvent = static_cast<const Event::ScriptDiagnosticEvent&>(event);
            const auto marker = diagnostic_marker_from_severity(diagnosticEvent.get_severity());
            const int row = diagnosticEvent.get_row();
            const auto message = diagnosticEvent.get_message();

            wxTheApp->CallAfter([this, marker, row, message]()
            {
                if (!m_codeEditor)
                {
                    return;
                }

                if (row <= 0)
                {
                    return;
                }

                const auto line = row - 1;
                if (line >= m_codeEditor->GetLineCount())
                {
                    return;
                }

                if (marker >= 0)
                {
                    m_codeEditor->MarkerAdd(line, marker);
                }

                wxString annotationText = wxString::FromUTF8(message);
                const auto existing = m_codeEditor->AnnotationGetText(line);
                if (!existing.empty())
                {
                    annotationText = existing + "\n" + annotationText;
                }

                m_codeEditor->AnnotationSetText(line, annotationText);
            });
            return;
        }

        const auto& outputEvent = static_cast<const Event::ScriptOutputEvent&>(event);
        const auto style = output_style_from_severity(outputEvent.get_severity());
        const auto message = outputEvent.get_message();

        wxTheApp->CallAfter([this, style, message]()
        {
            append_output_line(style, message);
        });
    }

    bool ScriptingView::toggle_view()
    {
        if (IsShown())
        {
            Hide();
            return false;
        }

        refresh_context_list();
        refresh_script_browser();
        m_scriptBrowserRefreshTicks = 0;

        Show();
        Raise();
        return true;
    }

    void ScriptingView::create_controls()
    {
        m_horizontalSplitter = new wxSplitterWindow(
            this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxSP_LIVE_UPDATE | wxSP_3D);

        m_scriptBrowserPanel = new wxPanel(m_horizontalSplitter, wxID_ANY);
        m_editorPanel = new wxPanel(m_horizontalSplitter, wxID_ANY);

        m_scriptBrowser = new wxTreeCtrl(
            m_scriptBrowserPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxTR_HAS_BUTTONS | wxTR_LINES_AT_ROOT | wxTR_HIDE_ROOT | wxTR_SINGLE);

        const auto theme = m_iconManager.get_current_theme();
        const auto iconSize = FromDIP(StandardWidgetValues::ICON_SIZE);

        m_toolbar = new wxToolBar(
            m_editorPanel,
            wxID_ANY,
            wxDefaultPosition,
            wxDefaultSize,
            wxTB_HORIZONTAL | wxTB_FLAT | wxTB_NODIVIDER);
        m_toolbar->SetToolBitmapSize(wxSize(iconSize, iconSize));
        m_toolbar->AddTool(
            ID_SCRIPT_NEW_TAB,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.newTab")),
            m_iconManager.get_icon("new_window", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_OPEN,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.open")),
            m_iconManager.get_icon("search", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_SAVE,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.save")),
            m_iconManager.get_icon("code", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_CLOSE_TAB,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.closeTab")),
            m_iconManager.get_icon("close", iconSize, theme));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(
            ID_SCRIPT_FIND,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.findPlaceholder")),
            m_iconManager.get_icon("search", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_REPLACE,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.replacePlaceholder")),
            m_iconManager.get_icon("restart", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_GOTO_LINE,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.goToLine")),
            m_iconManager.get_icon("arrow_down", iconSize, theme));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(
            ID_SCRIPT_EXECUTE,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.execute")),
            m_iconManager.get_icon("play", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_STOP_CONTEXT,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuRemove")),
            m_iconManager.get_icon("stop", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_SUSPEND_CONTEXT,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuSuspend")),
            m_iconManager.get_icon("pause", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_RESUME_CONTEXT,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuResume")),
            m_iconManager.get_icon("play", iconSize, theme));
        m_toolbar->AddSeparator();
        m_toolbar->AddTool(
            ID_SCRIPT_CLEAR,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.clear")),
            m_iconManager.get_icon("close", iconSize, theme));
        m_toolbar->AddTool(
            ID_SCRIPT_CLEAR_OUTPUT,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.clearOutput")),
            m_iconManager.get_icon("terminal", iconSize, theme));
        m_toolbar->Realize();

        m_editorNotebook = new wxNotebook(m_editorPanel, wxID_ANY);
        m_minimap = new wxStyledTextCtrl(
            m_editorPanel, wxID_ANY, wxDefaultPosition,
            wxSize(FromDIP(ScriptingViewValues::MINIMAP_WIDTH), -1));
        m_codeEditor = nullptr;

        m_contextList = new wxListCtrl(m_editorPanel, wxID_ANY, wxDefaultPosition,
            wxSize(-1, FromDIP(ScriptingViewValues::CONTEXT_LIST_HEIGHT)),
            wxLC_REPORT | wxLC_SINGLE_SEL | wxLC_NO_SORT_HEADER);

        m_contextList->InsertColumn(0,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.columnName")),
            wxLIST_FORMAT_LEFT, FromDIP(ScriptingViewValues::COLUMN_WIDTH_NAME));

        m_contextList->InsertColumn(1,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.columnState")),
            wxLIST_FORMAT_LEFT, FromDIP(ScriptingViewValues::COLUMN_WIDTH_STATE));

        m_variableInspector = new wxDataViewListCtrl(
            m_editorPanel, wxID_ANY, wxDefaultPosition,
            wxSize(-1, FromDIP(ScriptingViewValues::VARIABLE_INSPECTOR_HEIGHT)),
            wxDV_ROW_LINES | wxDV_VERT_RULES | wxDV_HORIZ_RULES | wxDV_SINGLE);
        m_variableInspector->AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.variableColumnName")),
            wxDATAVIEW_CELL_INERT,
            FromDIP(ScriptingViewValues::COLUMN_WIDTH_VARIABLE_NAME));
        m_variableInspector->AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.variableColumnType")),
            wxDATAVIEW_CELL_INERT,
            FromDIP(ScriptingViewValues::COLUMN_WIDTH_VARIABLE_TYPE));
        m_variableInspector->AppendTextColumn(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.variableColumnValue")),
            wxDATAVIEW_CELL_INERT,
            FromDIP(ScriptingViewValues::COLUMN_WIDTH_VARIABLE_VALUE));

        m_outputPanel = new wxStyledTextCtrl(m_editorPanel, wxID_ANY, wxDefaultPosition,
            wxSize(-1, FromDIP(ScriptingViewValues::OUTPUT_PANEL_HEIGHT)));

        load_recent_scripts();
        refresh_script_browser();
    }

    void ScriptingView::configure_editor() const
    {
        if (!m_codeEditor || !m_outputPanel || !m_minimap)
        {
            return;
        }

        m_codeEditor->SetLexer(wxSTC_LEX_CPP);

        m_codeEditor->SetKeyWords(0,
            "void int uint float double bool string "
            "int8 int16 int32 int64 uint8 uint16 uint32 uint64 "
            "if else for while do switch case default break continue return "
            "class interface enum namespace import from "
            "cast private protected const override final "
            "null true false this super "
            "funcdef typedef mixin shared external abstract "
            "try catch "
            "auto in out inout "
            "get set is not and or xor");

        m_codeEditor->SetKeyWords(1,
            "yield sleep "
            "wait_until wait_while wait_ticks "
            "is_process_open get_process_name "
            "log_info log_warn log_error "
            "print");

        const wxFont monoFont{StandardWidgetValues::TELETYPE_FONT_SIZE,
                              wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL};

        for (const auto i : std::views::iota(0, wxSTC_STYLE_LASTPREDEFINED + 1))
        {
            m_codeEditor->StyleSetFont(i, monoFont);
        }

        m_codeEditor->SetMarginType(ScriptingViewValues::LINE_NUMBER_MARGIN, wxSTC_MARGIN_NUMBER);
        m_codeEditor->SetMarginWidth(ScriptingViewValues::LINE_NUMBER_MARGIN, ScriptingViewValues::LINE_NUMBER_MARGIN_WIDTH);
        m_codeEditor->SetMarginSensitive(ScriptingViewValues::LINE_NUMBER_MARGIN, true);
        m_codeEditor->SetMarginType(ScriptingViewValues::FOLD_MARGIN, wxSTC_MARGIN_SYMBOL);
        m_codeEditor->SetMarginMask(ScriptingViewValues::FOLD_MARGIN, wxSTC_MASK_FOLDERS);
        m_codeEditor->SetMarginSensitive(ScriptingViewValues::FOLD_MARGIN, true);
        m_codeEditor->SetMarginWidth(ScriptingViewValues::FOLD_MARGIN, ScriptingViewValues::FOLD_MARGIN_WIDTH);
        m_codeEditor->SetMarginType(ScriptingViewValues::DIAGNOSTIC_MARGIN, wxSTC_MARGIN_SYMBOL);
        m_codeEditor->SetMarginMask(
            ScriptingViewValues::DIAGNOSTIC_MARGIN,
            (1 << ScriptingViewValues::DIAGNOSTIC_MARKER_ERROR) |
            (1 << ScriptingViewValues::DIAGNOSTIC_MARKER_WARNING) |
            (1 << ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER));
        m_codeEditor->SetMarginSensitive(ScriptingViewValues::DIAGNOSTIC_MARGIN, false);
        m_codeEditor->SetMarginWidth(ScriptingViewValues::DIAGNOSTIC_MARGIN, ScriptingViewValues::DIAGNOSTIC_MARGIN_WIDTH);

        m_codeEditor->SetTabWidth(ScriptingViewValues::TAB_WIDTH);
        m_codeEditor->SetUseTabs(false);
        m_codeEditor->SetIndent(ScriptingViewValues::TAB_WIDTH);
        m_codeEditor->SetTabIndents(true);
        m_codeEditor->SetBackSpaceUnIndents(true);

        m_codeEditor->SetCaretWidth(ScriptingViewValues::CARET_WIDTH);
        m_codeEditor->SetEdgeMode(wxSTC_EDGE_LINE);
        m_codeEditor->SetEdgeColumn(ScriptingViewValues::EDGE_COLUMN);
        m_codeEditor->SetCaretLineVisible(true);

        m_codeEditor->SetViewWhiteSpace(wxSTC_WS_INVISIBLE);
        m_codeEditor->SetViewEOL(false);
        m_codeEditor->SetIndentationGuides(wxSTC_IV_LOOKBOTH);
        m_codeEditor->AutoCompSetSeparator(' ');
        m_codeEditor->AutoCompSetIgnoreCase(true);
        m_codeEditor->AutoCompSetAutoHide(true);
        m_codeEditor->AutoCompSetDropRestOfWord(false);

        m_codeEditor->SetProperty("fold", "1");
        m_codeEditor->SetProperty("fold.compact", "0");
        m_codeEditor->SetProperty("fold.comment", "1");
        m_codeEditor->SetFoldFlags(
            wxSTC_FOLDFLAG_LINEBEFORE_CONTRACTED |
            wxSTC_FOLDFLAG_LINEAFTER_CONTRACTED);

        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDEROPEN, wxSTC_MARK_BOXMINUS);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDER, wxSTC_MARK_BOXPLUS);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDERSUB, wxSTC_MARK_VLINE);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDERTAIL, wxSTC_MARK_LCORNER);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDEREND, wxSTC_MARK_BOXPLUSCONNECTED);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDEROPENMID, wxSTC_MARK_BOXMINUSCONNECTED);
        m_codeEditor->MarkerDefine(wxSTC_MARKNUM_FOLDERMIDTAIL, wxSTC_MARK_TCORNER);
        m_codeEditor->MarkerDefine(ScriptingViewValues::DIAGNOSTIC_MARKER_ERROR, wxSTC_MARK_CIRCLE);
        m_codeEditor->MarkerDefine(ScriptingViewValues::DIAGNOSTIC_MARKER_WARNING, wxSTC_MARK_CIRCLE);
        m_codeEditor->MarkerDefine(ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER, wxSTC_MARK_CIRCLE);
        m_codeEditor->AnnotationSetVisible(wxSTC_ANNOTATION_BOXED);

        m_outputPanel->SetLexer(wxSTC_LEX_NULL);
        m_outputPanel->StyleSetFont(wxSTC_STYLE_DEFAULT, monoFont);
        m_outputPanel->StyleSetFont(ScriptingViewValues::OUTPUT_STYLE_INFO, monoFont);
        m_outputPanel->StyleSetFont(ScriptingViewValues::OUTPUT_STYLE_WARNING, monoFont);
        m_outputPanel->StyleSetFont(ScriptingViewValues::OUTPUT_STYLE_ERROR, monoFont);
        m_outputPanel->SetMarginWidth(0, 0);
        m_outputPanel->SetMarginWidth(1, 0);
        m_outputPanel->SetWrapMode(wxSTC_WRAP_WORD);
        m_outputPanel->SetReadOnly(true);

        const wxFont minimapFont{
            std::max(6, StandardWidgetValues::TELETYPE_FONT_SIZE - 4),
            wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL};
        for (const auto i : std::views::iota(0, wxSTC_STYLE_LASTPREDEFINED + 1))
        {
            m_minimap->StyleSetFont(i, minimapFont);
        }
        m_minimap->SetLexer(wxSTC_LEX_CPP);
        m_minimap->SetMarginWidth(0, 0);
        m_minimap->SetMarginWidth(1, 0);
        m_minimap->SetMarginWidth(2, 0);
        m_minimap->SetUseHorizontalScrollBar(false);
        m_minimap->SetUseVerticalScrollBar(false);
        m_minimap->SetReadOnly(true);
        m_minimap->SetCaretWidth(0);
        m_minimap->SetZoom(-6);
    }

    void ScriptingView::apply_theme() const
    {
    }

    void ScriptingView::layout_controls()
    {
        auto* scriptBrowserSizer = new wxBoxSizer(wxVERTICAL);
        auto* scriptBrowserLabel = new wxStaticText(
            m_scriptBrowserPanel, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowser")));
        scriptBrowserSizer->Add(scriptBrowserLabel,
            StandardWidgetValues::NO_PROPORTION,
            wxLEFT | wxRIGHT | wxTOP,
            StandardWidgetValues::STANDARD_BORDER);
        scriptBrowserSizer->Add(m_scriptBrowser,
            StandardWidgetValues::STANDARD_PROPORTION,
            wxEXPAND | wxALL,
            StandardWidgetValues::STANDARD_BORDER);
        m_scriptBrowserPanel->SetSizer(scriptBrowserSizer);

        auto* editorSizer = new wxBoxSizer(wxVERTICAL);
        auto* editorContentSizer = new wxBoxSizer(wxHORIZONTAL);
        auto* variableInspectorLabel = new wxStaticText(
            m_editorPanel, wxID_ANY,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.variableInspector")));
        editorContentSizer->Add(m_editorNotebook, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);
        editorContentSizer->Add(m_minimap, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(m_toolbar, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxALL, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(editorContentSizer, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(m_contextList, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(variableInspectorLabel, StandardWidgetValues::NO_PROPORTION, wxLEFT | wxRIGHT, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(m_variableInspector, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        editorSizer->Add(m_outputPanel, StandardWidgetValues::NO_PROPORTION, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, StandardWidgetValues::STANDARD_BORDER);
        m_editorPanel->SetSizer(editorSizer);

        m_horizontalSplitter->SetMinimumPaneSize(FromDIP(140));
        m_horizontalSplitter->SetSashGravity(0.0);
        m_horizontalSplitter->SplitVertically(m_scriptBrowserPanel, m_editorPanel, FromDIP(240));

        m_mainSizer = new wxBoxSizer(wxVERTICAL);
        m_mainSizer->Add(m_horizontalSplitter, StandardWidgetValues::STANDARD_PROPORTION, wxEXPAND);

        SetSizer(m_mainSizer);
        Layout();
    }

    void ScriptingView::bind_events()
    {
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_new_tab_clicked, this, ID_SCRIPT_NEW_TAB);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_open_clicked, this, ID_SCRIPT_OPEN);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_save_clicked, this, ID_SCRIPT_SAVE);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_close_tab_clicked, this, ID_SCRIPT_CLOSE_TAB);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_find_clicked, this, ID_SCRIPT_FIND);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_replace_clicked, this, ID_SCRIPT_REPLACE);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_go_to_line_clicked, this, ID_SCRIPT_GOTO_LINE);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_execute_clicked, this, ID_SCRIPT_EXECUTE);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_stop_context_clicked, this, ID_SCRIPT_STOP_CONTEXT);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_suspend_context_clicked, this, ID_SCRIPT_SUSPEND_CONTEXT);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_resume_context_clicked, this, ID_SCRIPT_RESUME_CONTEXT);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_clear_clicked, this, ID_SCRIPT_CLEAR);
        m_toolbar->Bind(wxEVT_TOOL, &ScriptingView::on_clear_output_clicked, this, ID_SCRIPT_CLEAR_OUTPUT);

        Bind(wxEVT_MENU, &ScriptingView::on_new_tab_clicked, this, ID_SCRIPT_NEW_TAB);
        Bind(wxEVT_MENU, &ScriptingView::on_close_tab_clicked, this, ID_SCRIPT_CLOSE_TAB);
        Bind(wxEVT_MENU, &ScriptingView::on_open_clicked, this, ID_SCRIPT_OPEN);
        Bind(wxEVT_MENU, &ScriptingView::on_save_clicked, this, ID_SCRIPT_SAVE);
        Bind(wxEVT_MENU, &ScriptingView::on_save_as_clicked, this, ID_SCRIPT_SAVE_AS);
        Bind(wxEVT_MENU, &ScriptingView::on_find_clicked, this, ID_SCRIPT_FIND);
        Bind(wxEVT_MENU, &ScriptingView::on_replace_clicked, this, ID_SCRIPT_REPLACE);
        Bind(wxEVT_MENU, &ScriptingView::on_go_to_line_clicked, this, ID_SCRIPT_GOTO_LINE);
        for (const auto& templateDefinition : SCRIPT_TEMPLATES)
        {
            Bind(wxEVT_MENU, &ScriptingView::on_new_from_template, this, templateDefinition.commandId);
        }
        Bind(wxEVT_MENU, &ScriptingView::on_stop_context_clicked, this, ID_SCRIPT_STOP_CONTEXT);
        Bind(wxEVT_MENU, &ScriptingView::on_suspend_context_clicked, this, ID_SCRIPT_SUSPEND_CONTEXT);
        Bind(wxEVT_MENU, &ScriptingView::on_resume_context_clicked, this, ID_SCRIPT_RESUME_CONTEXT);
        Bind(wxEVT_MENU, &ScriptingView::on_execute_clicked, this, ID_SCRIPT_EXECUTE);
        Bind(wxEVT_MENU, &ScriptingView::on_clear_output_clicked, this, ID_SCRIPT_CLEAR_OUTPUT);
        Bind(wxEVT_MENU, &ScriptingView::on_clear_clicked, this, ID_SCRIPT_CLEAR);

        std::array<wxAcceleratorEntry, 14> acceleratorEntries{};
        acceleratorEntries[0].Set(wxACCEL_CTRL, static_cast<int>('N'), ID_SCRIPT_NEW_TAB);
        acceleratorEntries[1].Set(wxACCEL_CTRL, static_cast<int>('W'), ID_SCRIPT_CLOSE_TAB);
        acceleratorEntries[2].Set(wxACCEL_CTRL, static_cast<int>('O'), ID_SCRIPT_OPEN);
        acceleratorEntries[3].Set(wxACCEL_CTRL, static_cast<int>('S'), ID_SCRIPT_SAVE);
        acceleratorEntries[4].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('S'), ID_SCRIPT_SAVE_AS);
        acceleratorEntries[5].Set(wxACCEL_CTRL, static_cast<int>('F'), ID_SCRIPT_FIND);
        acceleratorEntries[6].Set(wxACCEL_CTRL, static_cast<int>('H'), ID_SCRIPT_REPLACE);
        acceleratorEntries[7].Set(wxACCEL_CTRL, static_cast<int>('G'), ID_SCRIPT_GOTO_LINE);
        acceleratorEntries[8].Set(wxACCEL_NORMAL, WXK_F5, ID_SCRIPT_EXECUTE);
        acceleratorEntries[9].Set(wxACCEL_SHIFT, WXK_F5, ID_SCRIPT_STOP_CONTEXT);
        acceleratorEntries[10].Set(wxACCEL_NORMAL, WXK_F6, ID_SCRIPT_SUSPEND_CONTEXT);
        acceleratorEntries[11].Set(wxACCEL_NORMAL, WXK_F7, ID_SCRIPT_RESUME_CONTEXT);
        acceleratorEntries[12].Set(wxACCEL_CTRL, static_cast<int>('L'), ID_SCRIPT_CLEAR);
        acceleratorEntries[13].Set(wxACCEL_CTRL | wxACCEL_SHIFT, static_cast<int>('L'), ID_SCRIPT_CLEAR_OUTPUT);
        SetAcceleratorTable(wxAcceleratorTable(static_cast<int>(acceleratorEntries.size()), acceleratorEntries.data()));

        m_editorNotebook->Bind(wxEVT_NOTEBOOK_PAGE_CHANGED, &ScriptingView::on_editor_page_changed, this);
        m_scriptBrowser->Bind(wxEVT_TREE_ITEM_ACTIVATED, &ScriptingView::on_script_browser_item_activated, this);
        m_scriptBrowser->Bind(wxEVT_TREE_ITEM_RIGHT_CLICK, &ScriptingView::on_script_browser_right_click, this);
        m_contextList->Bind(wxEVT_LIST_ITEM_SELECTED, &ScriptingView::on_context_list_selection_changed, this);
        m_contextList->Bind(wxEVT_LIST_ITEM_DESELECTED, &ScriptingView::on_context_list_selection_changed, this);
        m_contextList->Bind(wxEVT_LIST_ITEM_RIGHT_CLICK, &ScriptingView::on_context_list_right_click, this);
        Bind(wxEVT_TIMER, &ScriptingView::on_refresh_timer, this);

        Bind(wxEVT_CLOSE_WINDOW, [this](wxCloseEvent& event)
        {
            if (has_dirty_tabs())
            {
                wxMessageDialog confirmDialog{
                    this,
                    wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedPrompt")),
                    wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedTitle")),
                    wxYES_NO | wxNO_DEFAULT | wxICON_WARNING};

                if (confirmDialog.ShowModal() != wxID_YES)
                {
                    event.Veto();
                    return;
                }
            }

            Hide();
            event.Veto();
        });

    }

    void ScriptingView::bind_editor_events(wxStyledTextCtrl* const editor)
    {
        if (!editor)
        {
            return;
        }

        editor->Bind(wxEVT_STC_UPDATEUI, &ScriptingView::on_editor_update_ui, this);
        editor->Bind(wxEVT_STC_MODIFIED, &ScriptingView::on_editor_modified, this);
        editor->Bind(wxEVT_STC_CHARADDED, &ScriptingView::on_editor_char_added, this);
        editor->Bind(wxEVT_STC_MARGINCLICK, &ScriptingView::on_editor_margin_click, this);
        editor->Bind(wxEVT_KEY_DOWN, &ScriptingView::on_editor_key_down, this);
    }

    int ScriptingView::active_tab_index() const
    {
        if (!m_editorNotebook)
        {
            return wxNOT_FOUND;
        }

        const auto selectedIndex = m_editorNotebook->GetSelection();
        if (selectedIndex == wxNOT_FOUND)
        {
            return wxNOT_FOUND;
        }

        const auto tabIndex = static_cast<std::size_t>(selectedIndex);
        if (tabIndex >= m_tabs.size())
        {
            return wxNOT_FOUND;
        }

        return selectedIndex;
    }

    bool ScriptingView::has_dirty_tabs() const
    {
        return std::ranges::any_of(m_tabs,
            [](const EditorTab& tab)
            {
                return tab.isDirty;
            });
    }

    int ScriptingView::find_tab_index_by_path(const std::filesystem::path& filePath) const
    {
        if (filePath.empty())
        {
            return wxNOT_FOUND;
        }

        for (std::size_t i{}; i < m_tabs.size(); ++i)
        {
            const auto& openedPath = m_tabs[i].filePath;
            if (openedPath.empty())
            {
                continue;
            }

            std::error_code errorCode{};
            if (std::filesystem::equivalent(openedPath, filePath, errorCode) && !errorCode)
            {
                return static_cast<int>(i);
            }

            if (openedPath.lexically_normal() == filePath.lexically_normal())
            {
                return static_cast<int>(i);
            }
        }

        return wxNOT_FOUND;
    }

    bool ScriptingView::open_script_file(const std::filesystem::path& filePath)
    {
        if (filePath.empty() || !is_script_file(filePath))
        {
            return false;
        }

        if (const auto existingTab = find_tab_index_by_path(filePath);
            existingTab != wxNOT_FOUND)
        {
            m_editorNotebook->SetSelection(existingTab);
            sync_active_editor_from_tab_selection();
            add_recent_script(filePath);
            return true;
        }

        auto content = m_viewModel->load_script(filePath);
        if (!content.has_value())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.loadFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return false;
        }

        create_editor_tab(content.value(), filePath, true);
        m_currentScriptPath = filePath;
        add_recent_script(filePath);
        return true;
    }

    bool ScriptingView::prompt_open_script_dialog()
    {
        std::filesystem::path basePath{};
        const auto tabIndex = active_tab_index();
        if (tabIndex != wxNOT_FOUND)
        {
            basePath = m_tabs[static_cast<std::size_t>(tabIndex)].filePath;
        }

        if (basePath.empty())
        {
            basePath = m_currentScriptPath;
        }

        if (basePath.empty())
        {
            basePath = m_viewModel->get_default_script_directory();
        }

        auto dialogDirectory = basePath;
        std::error_code directoryError{};
        if (!basePath.empty() && !std::filesystem::is_directory(basePath, directoryError))
        {
            dialogDirectory = basePath.parent_path();
        }
        if (dialogDirectory.empty())
        {
            dialogDirectory = m_viewModel->get_default_script_directory();
        }

        const auto scriptingPattern = fmt::format("*{}", FileTypes::SCRIPTING_EXTENSION);
        const wxString filter = wxString::Format("%s (%s)|%s",
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptFiles")),
            wxString::FromUTF8(scriptingPattern),
            wxString::FromUTF8(scriptingPattern));

        wxFileDialog dialog{this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.openTitle")),
            dialogDirectory.string(),
            wxEmptyString,
            filter,
            wxFD_OPEN | wxFD_FILE_MUST_EXIST};

        if (dialog.ShowModal() != wxID_OK)
        {
            return false;
        }

        const std::filesystem::path loadPath{dialog.GetPath().ToStdString()};
        if (!open_script_file(loadPath))
        {
            return false;
        }

        if (!m_scriptBrowser)
        {
            return true;
        }

        const auto rootItem = m_scriptBrowser->GetRootItem();
        if (!rootItem.IsOk())
        {
            return true;
        }

        const auto selectedItem = find_script_browser_item(*m_scriptBrowser, rootItem, loadPath);
        if (!selectedItem.IsOk())
        {
            return true;
        }

        m_scriptBrowser->SelectItem(selectedItem);
        m_scriptBrowser->EnsureVisible(selectedItem);
        return true;
    }

    std::filesystem::path ScriptingView::resolve_template_path(const std::string_view templateFileName) const
    {
        if (templateFileName.empty())
        {
            return {};
        }

        const auto templatePath = std::filesystem::path{std::string{templateFileName}};
        std::error_code errorCode{};

        const auto scriptDirectory = m_viewModel->get_default_script_directory();
        if (!scriptDirectory.empty())
        {
            const std::array scriptCandidates{
                scriptDirectory / templatePath,
                scriptDirectory / "templates" / templatePath};

            for (const auto& candidatePath : scriptCandidates)
            {
                if (std::filesystem::exists(candidatePath, errorCode) && !errorCode)
                {
                    return candidatePath;
                }
                errorCode.clear();
            }
        }

        const auto currentPath = std::filesystem::current_path(errorCode);
        if (errorCode)
        {
            return {};
        }

        auto searchRoot = currentPath;
        for (int depth{}; depth < 8; ++depth)
        {
            const auto candidatePath = searchRoot / "resources" / "templates" / templatePath;
            if (std::filesystem::exists(candidatePath, errorCode) && !errorCode)
            {
                return candidatePath;
            }
            errorCode.clear();

            const auto parentPath = searchRoot.parent_path();
            if (parentPath == searchRoot || parentPath.empty())
            {
                break;
            }
            searchRoot = parentPath;
        }

        return {};
    }

    void ScriptingView::create_tab_from_template(const int commandId)
    {
        const auto* templateDefinition = script_template_definition(commandId);
        if (!templateDefinition)
        {
            return;
        }

        const auto templatePath = resolve_template_path(templateDefinition->fileName);
        if (templatePath.empty())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.loadFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        auto loadedTemplate = m_viewModel->load_script(templatePath);
        if (!loadedTemplate.has_value())
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.loadFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        create_editor_tab(loadedTemplate.value(), {}, true);
    }

    void ScriptingView::load_recent_scripts()
    {
        m_recentScripts.clear();

        auto persistedRecentScripts = m_viewModel->get_recent_scripts();
        for (auto& path : persistedRecentScripts)
        {
            if (path.empty() || !is_script_file(path))
            {
                continue;
            }

            path = path.lexically_normal();
            std::error_code existsError{};
            if (!std::filesystem::exists(path, existsError) || existsError)
            {
                continue;
            }

            const auto duplicate = std::ranges::find_if(m_recentScripts,
                [&path](const std::filesystem::path& existingPath)
                {
                    return paths_equal(existingPath, path);
                });
            if (duplicate != m_recentScripts.end())
            {
                continue;
            }

            m_recentScripts.push_back(path);
            if (m_recentScripts.size() >= MAX_RECENT_SCRIPTS)
            {
                break;
            }
        }

        m_viewModel->set_recent_scripts(m_recentScripts);
    }

    void ScriptingView::add_recent_script(const std::filesystem::path& filePath)
    {
        if (filePath.empty() || !is_script_file(filePath))
        {
            return;
        }

        std::error_code existsError{};
        if (!std::filesystem::exists(filePath, existsError) || existsError)
        {
            remove_recent_script(filePath);
            return;
        }

        const auto normalizedPath = filePath.lexically_normal();
        std::erase_if(m_recentScripts,
            [&normalizedPath](const std::filesystem::path& existingPath)
            {
                return paths_equal(existingPath, normalizedPath);
            });

        m_recentScripts.insert(m_recentScripts.begin(), normalizedPath);
        if (m_recentScripts.size() > MAX_RECENT_SCRIPTS)
        {
            m_recentScripts.resize(MAX_RECENT_SCRIPTS);
        }

        m_viewModel->set_recent_scripts(m_recentScripts);
    }

    void ScriptingView::remove_recent_script(const std::filesystem::path& filePath)
    {
        if (filePath.empty())
        {
            return;
        }

        const auto beforeSize = m_recentScripts.size();
        std::erase_if(m_recentScripts,
            [&filePath](const std::filesystem::path& existingPath)
            {
                return paths_equal(existingPath, filePath);
            });

        if (m_recentScripts.size() == beforeSize)
        {
            return;
        }

        m_viewModel->set_recent_scripts(m_recentScripts);
    }

    void ScriptingView::refresh_script_browser()
    {
        if (!m_scriptBrowser)
        {
            return;
        }

        const auto rootDirectory = m_viewModel->get_default_script_directory();
        if (rootDirectory.empty())
        {
            return;
        }

        std::error_code errorCode{};
        if (!std::filesystem::exists(rootDirectory, errorCode))
        {
            std::filesystem::create_directories(rootDirectory, errorCode);
        }
        if (errorCode)
        {
            return;
        }

        populate_script_browser_tree(rootDirectory);
    }

    void ScriptingView::populate_script_browser_tree(const std::filesystem::path& rootDirectory)
    {
        if (!m_scriptBrowser)
        {
            return;
        }

        std::filesystem::path selectedPath{};
        if (const auto selectedItem = m_scriptBrowser->GetSelection(); selectedItem.IsOk())
        {
            if (auto* data = script_browser_item_data(*m_scriptBrowser, selectedItem))
            {
                selectedPath = data->path;
            }
        }

        std::vector<std::filesystem::path> expandedPaths{};
        if (const auto currentRoot = m_scriptBrowser->GetRootItem(); currentRoot.IsOk())
        {
            collect_expanded_script_browser_paths(*m_scriptBrowser, currentRoot, expandedPaths);
        }

        m_scriptBrowser->Freeze();
        m_scriptBrowser->DeleteAllItems();

        const auto rootItem = m_scriptBrowser->AddRoot(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowser")),
            -1, -1,
            new ScriptBrowserItemData(rootDirectory, true));

        append_script_browser_children(rootItem, rootDirectory);
        expand_script_browser_item(*m_scriptBrowser, rootItem);

        for (const auto& expandedPath : expandedPaths)
        {
            const auto expandedItem = find_script_browser_item(*m_scriptBrowser, rootItem, expandedPath);
            if (expandedItem.IsOk())
            {
                expand_script_browser_item(*m_scriptBrowser, expandedItem);
            }
        }

        if (!selectedPath.empty())
        {
            const auto selectedItem = find_script_browser_item(*m_scriptBrowser, rootItem, selectedPath);
            if (selectedItem.IsOk())
            {
                m_scriptBrowser->SelectItem(selectedItem);
                m_scriptBrowser->EnsureVisible(selectedItem);
            }
        }

        m_scriptBrowser->Thaw();
    }

    void ScriptingView::append_script_browser_children(const wxTreeItemId& parent,
                                                       const std::filesystem::path& directory)
    {
        if (!m_scriptBrowser || !parent.IsOk())
        {
            return;
        }

        std::vector<std::filesystem::path> directories{};
        std::vector<std::filesystem::path> scriptFiles{};

        std::error_code iteratorError{};
        for (std::filesystem::directory_iterator iterator{
                 directory,
                 std::filesystem::directory_options::skip_permission_denied,
                 iteratorError};
             !iteratorError && iterator != std::filesystem::directory_iterator{};
             iterator.increment(iteratorError))
        {
            const auto& entry = *iterator;
            std::error_code typeError{};

            if (entry.is_directory(typeError))
            {
                std::error_code symlinkError{};
                if (!entry.is_symlink(symlinkError) && !symlinkError)
                {
                    directories.push_back(entry.path());
                }
                continue;
            }

            if (typeError)
            {
                continue;
            }

            if (entry.is_regular_file(typeError) && !typeError && is_script_file(entry.path()))
            {
                scriptFiles.push_back(entry.path());
            }
        }

        constexpr auto pathComparator = [](const std::filesystem::path& lhs, const std::filesystem::path& rhs)
        {
            return to_lower_ascii(lhs.filename().string()) < to_lower_ascii(rhs.filename().string());
        };

        std::ranges::sort(directories, pathComparator);
        std::ranges::sort(scriptFiles, pathComparator);

        for (const auto& childDirectory : directories)
        {
            const auto directoryName = childDirectory.filename().string();
            const auto item = m_scriptBrowser->AppendItem(
                parent,
                wxString::FromUTF8(directoryName),
                -1, -1,
                new ScriptBrowserItemData(childDirectory, true));

            append_script_browser_children(item, childDirectory);
        }

        for (const auto& scriptPath : scriptFiles)
        {
            const auto scriptName = scriptPath.filename().string();
            m_scriptBrowser->AppendItem(
                parent,
                wxString::FromUTF8(scriptName),
                -1, -1,
                new ScriptBrowserItemData(scriptPath, false));
        }
    }

    void ScriptingView::create_script_from_browser(const std::filesystem::path& directory)
    {
        auto targetDirectory = directory;
        if (targetDirectory.empty())
        {
            targetDirectory = m_viewModel->get_default_script_directory();
        }

        std::error_code errorCode{};
        if (!std::filesystem::exists(targetDirectory, errorCode))
        {
            std::filesystem::create_directories(targetDirectory, errorCode);
        }
        if (errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserCreateFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        wxTextEntryDialog dialog{
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserNewScriptPrompt")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserNewScriptTitle")),
            wxString::FromUTF8(fmt::format("new_script{}", FileTypes::SCRIPTING_EXTENSION))
        };

        if (dialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const auto requestedName = trim_ascii_copy(dialog.GetValue().ToStdString());
        if (requestedName.empty() || has_path_separator(requestedName))
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserInvalidName")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        auto newScriptPath = targetDirectory / std::filesystem::path{requestedName}.filename();
        if (to_lower_ascii(newScriptPath.extension().string()) != FileTypes::SCRIPTING_EXTENSION)
        {
            newScriptPath.replace_extension(FileTypes::SCRIPTING_EXTENSION);
        }

        if (std::filesystem::exists(newScriptPath, errorCode) && !errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserAlreadyExists")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }
        if (errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserCreateFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        if (const auto status = m_viewModel->save_script(newScriptPath, {});
            status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserCreateFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        refresh_script_browser();
        std::ignore = open_script_file(newScriptPath);
    }

    void ScriptingView::rename_script_from_browser(const wxTreeItemId& itemId)
    {
        if (!m_scriptBrowser || !itemId.IsOk())
        {
            return;
        }

        auto* data = script_browser_item_data(*m_scriptBrowser, itemId);
        if (!data || data->path.empty())
        {
            return;
        }

        const auto sourcePath = data->path;
        const bool isDirectory = data->isDirectory;

        wxTextEntryDialog dialog{
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserRenamePrompt")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserRenameTitle")),
            wxString::FromUTF8(sourcePath.filename().string())
        };

        if (dialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const auto requestedName = trim_ascii_copy(dialog.GetValue().ToStdString());
        if (requestedName.empty() || has_path_separator(requestedName))
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserInvalidName")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        auto targetPath = sourcePath.parent_path() / std::filesystem::path{requestedName}.filename();
        if (!isDirectory && to_lower_ascii(targetPath.extension().string()) != FileTypes::SCRIPTING_EXTENSION)
        {
            targetPath.replace_extension(FileTypes::SCRIPTING_EXTENSION);
        }

        if (paths_equal(sourcePath, targetPath))
        {
            return;
        }

        std::error_code errorCode{};
        if (std::filesystem::exists(targetPath, errorCode) && !errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserAlreadyExists")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }
        if (errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserRenameFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        std::filesystem::rename(sourcePath, targetPath, errorCode);
        if (errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserRenameFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        for (std::size_t i{}; i < m_tabs.size(); ++i)
        {
            auto& tab = m_tabs[i];
            if (tab.filePath.empty())
            {
                continue;
            }

            if (!isDirectory && paths_equal(tab.filePath, sourcePath))
            {
                tab.filePath = targetPath;
                tab.tabTitle = tab.filePath.filename().string();
                update_tab_title(i);
                continue;
            }

            if (!isDirectory || !path_is_within(tab.filePath, sourcePath))
            {
                continue;
            }

            const auto relativePath = tab.filePath.lexically_normal().lexically_relative(sourcePath.lexically_normal());
            tab.filePath = (targetPath / relativePath).lexically_normal();
        }

        if (!m_currentScriptPath.empty())
        {
            if (!isDirectory && paths_equal(m_currentScriptPath, sourcePath))
            {
                m_currentScriptPath = targetPath;
            }
            else if (isDirectory && path_is_within(m_currentScriptPath, sourcePath))
            {
                const auto relativePath =
                    m_currentScriptPath.lexically_normal().lexically_relative(sourcePath.lexically_normal());
                m_currentScriptPath = (targetPath / relativePath).lexically_normal();
            }
        }

        bool recentScriptsChanged{};
        for (auto& recentPath : m_recentScripts)
        {
            if (!isDirectory && paths_equal(recentPath, sourcePath))
            {
                recentPath = targetPath;
                recentScriptsChanged = true;
                continue;
            }

            if (!isDirectory || !path_is_within(recentPath, sourcePath))
            {
                continue;
            }

            const auto relativePath = recentPath.lexically_normal().lexically_relative(sourcePath.lexically_normal());
            recentPath = (targetPath / relativePath).lexically_normal();
            recentScriptsChanged = true;
        }

        if (recentScriptsChanged)
        {
            std::vector<std::filesystem::path> uniqueRecentScripts{};
            for (const auto& recentPath : m_recentScripts)
            {
                if (recentPath.empty() || !is_script_file(recentPath))
                {
                    continue;
                }

                const auto duplicate = std::ranges::find_if(uniqueRecentScripts,
                    [&recentPath](const std::filesystem::path& existingPath)
                    {
                        return paths_equal(existingPath, recentPath);
                    });
                if (duplicate != uniqueRecentScripts.end())
                {
                    continue;
                }

                uniqueRecentScripts.push_back(recentPath.lexically_normal());
                if (uniqueRecentScripts.size() >= MAX_RECENT_SCRIPTS)
                {
                    break;
                }
            }

            m_recentScripts = std::move(uniqueRecentScripts);
            m_viewModel->set_recent_scripts(m_recentScripts);
        }

        refresh_script_browser();

        const auto rootItem = m_scriptBrowser->GetRootItem();
        if (!rootItem.IsOk())
        {
            return;
        }

        const auto renamedItem = find_script_browser_item(*m_scriptBrowser, rootItem, targetPath);
        if (!renamedItem.IsOk())
        {
            return;
        }

        m_scriptBrowser->SelectItem(renamedItem);
        m_scriptBrowser->EnsureVisible(renamedItem);
    }

    void ScriptingView::delete_script_from_browser(const wxTreeItemId& itemId)
    {
        if (!m_scriptBrowser || !itemId.IsOk())
        {
            return;
        }

        auto* data = script_browser_item_data(*m_scriptBrowser, itemId);
        if (!data || data->path.empty())
        {
            return;
        }

        const auto deletePath = data->path;
        wxMessageDialog confirmDialog{
            this,
            wxString::FromUTF8(fmt::format(
                fmt::runtime(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserDeletePrompt")),
                deletePath.filename().string())),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserDeleteTitle")),
            wxYES_NO | wxNO_DEFAULT | wxICON_WARNING};

        if (confirmDialog.ShowModal() != wxID_YES)
        {
            return;
        }

        std::vector<std::size_t> affectedTabs{};
        bool hasDirtyAffectedTabs{};
        for (std::size_t i{}; i < m_tabs.size(); ++i)
        {
            const auto& tab = m_tabs[i];
            if (tab.filePath.empty())
            {
                continue;
            }

            const bool isMatch = paths_equal(tab.filePath, deletePath) ||
                (data->isDirectory && path_is_within(tab.filePath, deletePath));
            if (!isMatch)
            {
                continue;
            }

            affectedTabs.push_back(i);
            hasDirtyAffectedTabs = hasDirtyAffectedTabs || tab.isDirty;
        }

        if (hasDirtyAffectedTabs)
        {
            wxMessageDialog unsavedDialog{
                this,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedPrompt")),
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedTitle")),
                wxYES_NO | wxNO_DEFAULT | wxICON_WARNING};

            if (unsavedDialog.ShowModal() != wxID_YES)
            {
                return;
            }
        }

        std::error_code errorCode{};
        if (data->isDirectory)
        {
            std::filesystem::remove_all(deletePath, errorCode);
        }
        else
        {
            std::filesystem::remove(deletePath, errorCode);
        }

        if (errorCode)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserDeleteFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        std::ranges::sort(affectedTabs,
            [](const std::size_t lhs, const std::size_t rhs)
            {
                return lhs > rhs;
            });

        for (const auto tabIndex : affectedTabs)
        {
            std::ignore = close_editor_tab(tabIndex, false);
        }

        if (!m_currentScriptPath.empty())
        {
            const bool isDeletedPath = paths_equal(m_currentScriptPath, deletePath) ||
                (data->isDirectory && path_is_within(m_currentScriptPath, deletePath));
            if (isDeletedPath)
            {
                m_currentScriptPath.clear();
            }
        }

        const auto previousRecentCount = m_recentScripts.size();
        std::erase_if(m_recentScripts,
            [&deletePath, isDirectory = data->isDirectory](const std::filesystem::path& recentPath)
            {
                return paths_equal(recentPath, deletePath) ||
                    (isDirectory && path_is_within(recentPath, deletePath));
            });
        if (m_recentScripts.size() != previousRecentCount)
        {
            m_viewModel->set_recent_scripts(m_recentScripts);
        }

        refresh_script_browser();
    }

    void ScriptingView::sync_active_editor_from_tab_selection()
    {
        const auto selectedIndex = active_tab_index();
        if (selectedIndex == wxNOT_FOUND)
        {
            m_codeEditor = nullptr;
            if (m_minimap)
            {
                m_minimap->SetReadOnly(false);
                m_minimap->ClearAll();
                m_minimap->SetReadOnly(true);
            }
            return;
        }

        auto& tab = m_tabs[static_cast<std::size_t>(selectedIndex)];
        m_codeEditor = tab.editor;

        if (!tab.filePath.empty())
        {
            m_currentScriptPath = tab.filePath;
        }

        sync_minimap_content();
    }

    void ScriptingView::sync_minimap_content() const
    {
        if (!m_codeEditor || !m_minimap)
        {
            return;
        }

        m_minimap->SetReadOnly(false);
        m_minimap->SetText(m_codeEditor->GetText());
        m_minimap->SetReadOnly(true);
        m_minimap->ScrollToLine(m_codeEditor->GetFirstVisibleLine());
    }

    void ScriptingView::update_tab_title(const std::size_t tabIndex) const
    {
        if (!m_editorNotebook || tabIndex >= m_tabs.size())
        {
            return;
        }

        const auto& tab = m_tabs[tabIndex];
        const auto title = tab.isDirty
            ? fmt::format("*{}", tab.tabTitle)
            : tab.tabTitle;

        m_editorNotebook->SetPageText(static_cast<int>(tabIndex), wxString::FromUTF8(title));
    }

    void ScriptingView::create_editor_tab(const std::string& content,
                                          const std::filesystem::path& filePath,
                                          const bool selectTab)
    {
        if (!m_editorNotebook)
        {
            return;
        }

        auto* editor = new wxStyledTextCtrl(m_editorNotebook, wxID_ANY);
        editor->SetText(wxString::FromUTF8(content));
        editor->SetSavePoint();

        m_codeEditor = editor;
        configure_editor();
        apply_theme();

        std::string title = filePath.filename().string();
        if (title.empty())
        {
            title = fmt::format("{} {}", m_languageService.fetch_translation("scriptingView.ui.untitled"), m_untitledCounter++);
        }

        m_tabs.emplace_back(EditorTab{
            .editor = editor,
            .filePath = filePath,
            .isDirty = false,
            .tabTitle = title
        });

        const auto newTabIndex = static_cast<int>(m_tabs.size() - 1);
        m_editorNotebook->AddPage(editor, wxString::FromUTF8(title), selectTab);
        bind_editor_events(editor);

        if (selectTab)
        {
            m_editorNotebook->SetSelection(newTabIndex);
        }

        sync_active_editor_from_tab_selection();
        update_tab_title(static_cast<std::size_t>(newTabIndex));
        m_isDirty = has_dirty_tabs();
    }

    bool ScriptingView::close_editor_tab(const std::size_t tabIndex, const bool promptUnsavedChanges)
    {
        if (!m_editorNotebook || tabIndex >= m_tabs.size())
        {
            return false;
        }

        if (promptUnsavedChanges && m_tabs[tabIndex].isDirty)
        {
            wxMessageDialog confirmDialog{
                this,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedPrompt")),
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedTitle")),
                wxYES_NO | wxNO_DEFAULT | wxICON_WARNING};

            if (confirmDialog.ShowModal() != wxID_YES)
            {
                return false;
            }
        }

        if (!m_editorNotebook->DeletePage(static_cast<int>(tabIndex)))
        {
            return false;
        }
        m_tabs.erase(m_tabs.begin() + static_cast<std::ptrdiff_t>(tabIndex));

        if (m_tabs.empty())
        {
            create_editor_tab({}, {}, true);
        }

        sync_active_editor_from_tab_selection();
        for (std::size_t i{}; i < m_tabs.size(); ++i)
        {
            update_tab_title(i);
        }

        m_isDirty = has_dirty_tabs();
        return true;
    }

    void ScriptingView::on_editor_page_changed(wxBookCtrlEvent& event)
    {
        sync_active_editor_from_tab_selection();
        if (m_codeEditor)
        {
            configure_editor();
            apply_theme();
        }
        event.Skip();
    }

    void ScriptingView::on_new_tab_clicked([[maybe_unused]] const wxCommandEvent& event)
    {
        if (event.GetEventObject() != m_toolbar || !m_toolbar)
        {
            create_editor_tab({}, {}, true);
            return;
        }

        wxMenu menu{};
        const int newEmptyTabId = wxWindow::NewControlId();
        menu.Append(newEmptyTabId, wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.newTab")));

        auto* templateMenu = new wxMenu();
        for (const auto& templateDefinition : SCRIPT_TEMPLATES)
        {
            templateMenu->Append(
                templateDefinition.commandId,
                wxString::FromUTF8(script_template_label(m_languageService, templateDefinition)));
        }

        menu.AppendSubMenu(
            templateMenu,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.newFromTemplate")));

        const int selectedAction = m_toolbar->GetPopupMenuSelectionFromUser(menu);
        if (selectedAction == wxID_NONE)
        {
            return;
        }

        if (selectedAction == newEmptyTabId)
        {
            create_editor_tab({}, {}, true);
            return;
        }

        create_tab_from_template(selectedAction);
    }

    void ScriptingView::on_new_from_template(const wxCommandEvent& event)
    {
        create_tab_from_template(event.GetId());
    }

    std::optional<Scripting::ContextId> ScriptingView::selected_context_for_toolbar_action() const
    {
        if (m_selectedContextId.has_value())
        {
            return m_selectedContextId;
        }

        if (!m_contextList)
        {
            return std::nullopt;
        }

        const auto selectedIndex = m_contextList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == wxNOT_FOUND)
        {
            return std::nullopt;
        }

        const auto contextIndex = static_cast<std::size_t>(selectedIndex);
        if (contextIndex >= m_contextCache.size())
        {
            return std::nullopt;
        }

        return m_contextCache[contextIndex].id;
    }

    void ScriptingView::on_stop_context_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto contextId = selected_context_for_toolbar_action();
        if (!contextId.has_value())
        {
            return;
        }

        const auto status = m_viewModel->remove_context(*contextId);
        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }

        refresh_context_list();
    }

    void ScriptingView::on_suspend_context_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto contextId = selected_context_for_toolbar_action();
        if (!contextId.has_value())
        {
            return;
        }

        const auto status = m_viewModel->suspend_context(*contextId);
        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }

        refresh_context_list();
    }

    void ScriptingView::on_resume_context_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto contextId = selected_context_for_toolbar_action();
        if (!contextId.has_value())
        {
            return;
        }

        const auto status = m_viewModel->resume_context(*contextId);
        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }

        refresh_context_list();
    }

    void ScriptingView::on_close_tab_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto tabIndex = active_tab_index();
        if (tabIndex == wxNOT_FOUND)
        {
            return;
        }

        std::ignore = close_editor_tab(static_cast<std::size_t>(tabIndex), true);
    }

    void ScriptingView::on_clear_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        const auto tabIndex = active_tab_index();
        if (tabIndex == wxNOT_FOUND || !m_codeEditor)
        {
            return;
        }

        if (m_tabs[static_cast<std::size_t>(tabIndex)].isDirty)
        {
            wxMessageDialog confirmDialog{
                this,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedPrompt")),
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.unsavedTitle")),
                wxYES_NO | wxNO_DEFAULT | wxICON_WARNING};

            if (confirmDialog.ShowModal() != wxID_YES)
            {
                return;
            }
        }

        m_codeEditor->ClearAll();
    }

    void ScriptingView::on_clear_output_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        m_outputPanel->SetReadOnly(false);
        m_outputPanel->ClearAll();
        m_outputPanel->SetReadOnly(true);
    }

    void ScriptingView::on_open_clicked([[maybe_unused]] const wxCommandEvent& event)
    {
        if (event.GetEventObject() != m_toolbar || !m_toolbar)
        {
            std::ignore = prompt_open_script_dialog();
            return;
        }

        load_recent_scripts();

        wxMenu menu{};
        const int openDialogId = wxWindow::NewControlId();
        menu.Append(openDialogId, wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.open")));

        std::unordered_map<int, std::filesystem::path> recentSelectionMap{};
        if (!m_recentScripts.empty())
        {
            auto* recentFilesMenu = new wxMenu();
            for (const auto& recentPath : m_recentScripts)
            {
                const int recentId = wxWindow::NewControlId();
                recentFilesMenu->Append(recentId, recent_script_menu_label(recentPath));
                recentSelectionMap.emplace(recentId, recentPath);
            }

            menu.AppendSubMenu(recentFilesMenu,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.recentFiles")));
        }

        const auto selectedAction = m_toolbar->GetPopupMenuSelectionFromUser(menu);
        if (selectedAction == wxID_NONE)
        {
            return;
        }

        if (selectedAction == openDialogId)
        {
            std::ignore = prompt_open_script_dialog();
            return;
        }

        const auto recentSelection = recentSelectionMap.find(selectedAction);
        if (recentSelection == recentSelectionMap.end())
        {
            return;
        }

        if (open_script_file(recentSelection->second))
        {
            return;
        }

        remove_recent_script(recentSelection->second);
    }

    void ScriptingView::on_save_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        save_script(false);
    }

    void ScriptingView::on_save_as_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        save_script(true);
    }

    void ScriptingView::on_find_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (!m_codeEditor)
        {
            return;
        }

        const auto selectedText = m_codeEditor->GetSelectedText().ToStdString();
        const auto initialValue = selectedText.empty() ? m_lastSearchQuery : selectedText;

        wxTextEntryDialog findDialog{
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.findPlaceholder")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.findPlaceholder")),
            wxString::FromUTF8(initialValue)};

        if (findDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const auto query = findDialog.GetValue().ToStdString();
        if (query.empty())
        {
            return;
        }

        m_lastSearchQuery = query;

        const auto findText = wxString::FromUTF8(query);
        const int textLength = m_codeEditor->GetTextLength();
        int startPosition = m_codeEditor->GetSelectionEnd();
        if (startPosition < 0 || startPosition > textLength)
        {
            startPosition = m_codeEditor->GetCurrentPos();
        }
        if (startPosition < 0 || startPosition > textLength)
        {
            startPosition = 0;
        }

        m_codeEditor->SetSearchFlags(0);
        m_codeEditor->SetTargetStart(startPosition);
        m_codeEditor->SetTargetEnd(textLength);
        int foundPosition = m_codeEditor->SearchInTarget(findText);
        if (foundPosition == wxSTC_INVALID_POSITION && startPosition > 0)
        {
            m_codeEditor->SetTargetStart(0);
            m_codeEditor->SetTargetEnd(startPosition);
            foundPosition = m_codeEditor->SearchInTarget(findText);
        }
        if (foundPosition == wxSTC_INVALID_POSITION)
        {
            return;
        }

        const int endPosition = m_codeEditor->GetTargetEnd();
        m_codeEditor->SetSelection(foundPosition, endPosition);
        m_codeEditor->GotoPos(foundPosition);
        m_codeEditor->EnsureCaretVisible();
    }

    void ScriptingView::on_replace_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (!m_codeEditor)
        {
            return;
        }

        const auto selectedText = m_codeEditor->GetSelectedText().ToStdString();
        const auto defaultFindText = selectedText.empty() ? m_lastSearchQuery : selectedText;

        wxTextEntryDialog findDialog{
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.findPlaceholder")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.findPlaceholder")),
            wxString::FromUTF8(defaultFindText)};

        if (findDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const auto findText = findDialog.GetValue().ToStdString();
        if (findText.empty())
        {
            return;
        }

        wxTextEntryDialog replaceDialog{
            this,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.replacePlaceholder")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.replacePlaceholder"))};

        if (replaceDialog.ShowModal() != wxID_OK)
        {
            return;
        }

        const auto replaceText = replaceDialog.GetValue().ToStdString();
        m_lastSearchQuery = findText;

        const auto wxFindText = wxString::FromUTF8(findText);
        const auto wxReplaceText = wxString::FromUTF8(replaceText);

        m_codeEditor->BeginUndoAction();
        m_codeEditor->SetSearchFlags(0);

        int searchStart{};
        while (searchStart <= m_codeEditor->GetTextLength())
        {
            m_codeEditor->SetTargetStart(searchStart);
            m_codeEditor->SetTargetEnd(m_codeEditor->GetTextLength());

            const int foundPosition = m_codeEditor->SearchInTarget(wxFindText);
            if (foundPosition == wxSTC_INVALID_POSITION)
            {
                break;
            }

            const int replacedLength = m_codeEditor->ReplaceTarget(wxReplaceText);
            searchStart = foundPosition + replacedLength;
        }

        m_codeEditor->EndUndoAction();
    }

    void ScriptingView::on_go_to_line_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (!m_codeEditor)
        {
            return;
        }

        const long lineCount = std::max(1, m_codeEditor->GetLineCount());
        const long currentLine = m_codeEditor->GetCurrentLine() + 1;
        const long requestedLine = wxGetNumberFromUser(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.goToLine")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.goToLine")),
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.goToLine")),
            currentLine,
            1,
            lineCount,
            this);

        if (requestedLine < 1)
        {
            return;
        }

        const int targetLine = static_cast<int>(requestedLine - 1);
        const int targetPosition = m_codeEditor->PositionFromLine(targetLine);
        if (targetPosition == wxSTC_INVALID_POSITION)
        {
            return;
        }

        m_codeEditor->GotoPos(targetPosition);
        m_codeEditor->SetSelection(targetPosition, targetPosition);
        m_codeEditor->EnsureCaretVisible();
    }

    void ScriptingView::save_script(const bool forceSaveAs)
    {
        const auto tabIndex = active_tab_index();
        if (tabIndex == wxNOT_FOUND || !m_codeEditor)
        {
            return;
        }

        auto& activeTab = m_tabs[static_cast<std::size_t>(tabIndex)];
        std::filesystem::path savePath = activeTab.filePath;

        if (forceSaveAs || savePath.empty())
        {
            std::filesystem::path basePath = activeTab.filePath;
            if (basePath.empty())
            {
                basePath = m_currentScriptPath;
            }
            if (basePath.empty())
            {
                basePath = m_viewModel->get_default_script_directory();
            }

            auto dialogDirectory = basePath;
            std::error_code directoryError{};
            if (!basePath.empty() && !std::filesystem::is_directory(basePath, directoryError))
            {
                dialogDirectory = basePath.parent_path();
            }
            if (dialogDirectory.empty())
            {
                dialogDirectory = m_viewModel->get_default_script_directory();
            }
            const auto scriptingPattern = fmt::format("*{}", FileTypes::SCRIPTING_EXTENSION);
            const wxString filter = wxString::Format("%s (%s)|%s",
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptFiles")),
                wxString::FromUTF8(scriptingPattern),
                wxString::FromUTF8(scriptingPattern));

            wxFileDialog dialog{this,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.saveTitle")),
                dialogDirectory.string(),
                (forceSaveAs && !activeTab.filePath.empty())
                    ? wxString::FromUTF8(basePath.filename().string())
                    : wxString{},
                filter,
                wxFD_SAVE | wxFD_OVERWRITE_PROMPT};

            if (dialog.ShowModal() != wxID_OK)
            {
                return;
            }

            savePath = std::filesystem::path{dialog.GetPath().ToStdString()};
            if (savePath.extension().empty())
            {
                savePath += FileTypes::SCRIPTING_EXTENSION;
            }
        }

        const auto status = m_viewModel->save_script(savePath, m_codeEditor->GetText().ToStdString());
        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.saveFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        wxMessageBox(
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.saveSuccess")),
            wxString::FromUTF8(m_languageService.fetch_translation("general.information")),
            wxOK | wxICON_INFORMATION, this);

        m_codeEditor->SetSavePoint();
        activeTab.isDirty = false;
        activeTab.filePath = savePath;
        activeTab.tabTitle = savePath.filename().string();
        if (activeTab.tabTitle.empty())
        {
            activeTab.tabTitle = fmt::format("{} {}", m_languageService.fetch_translation("scriptingView.ui.untitled"), m_untitledCounter++);
        }
        update_tab_title(static_cast<std::size_t>(tabIndex));
        m_isDirty = has_dirty_tabs();
        m_currentScriptPath = savePath;
        add_recent_script(savePath);
        refresh_script_browser();
    }

    void ScriptingView::on_execute_clicked([[maybe_unused]] wxCommandEvent& event)
    {
        if (!m_codeEditor)
        {
            return;
        }

        const auto code = m_codeEditor->GetText().ToStdString();
        if (code.empty())
        {
            return;
        }

        m_codeEditor->MarkerDeleteAll(ScriptingViewValues::DIAGNOSTIC_MARKER_ERROR);
        m_codeEditor->MarkerDeleteAll(ScriptingViewValues::DIAGNOSTIC_MARKER_WARNING);
        m_codeEditor->AnnotationClearAll();

        auto result = m_viewModel->execute_script(code);

        if (!result.has_value())
        {
            const auto status = result.error();
            wxMessageBox(
                wxString::FromUTF8(fmt::format("{}\n{}",
                    m_languageService.fetch_translation("scriptingView.ui.executeFailed"),
                    execution_error_message(status))),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
            return;
        }

        const auto contextId = result.value();
        bool breakpointUpdateFailed{};
        for (const auto line : editor_breakpoint_lines(*m_codeEditor))
        {
            const auto status = m_viewModel->set_context_breakpoint(contextId, line);
            if (status == StatusCode::STATUS_OK || status == StatusCode::STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS)
            {
                continue;
            }

            breakpointUpdateFailed = true;
        }

        if (breakpointUpdateFailed)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }

        refresh_context_list();
    }

    void ScriptingView::on_refresh_timer([[maybe_unused]] wxTimerEvent& event)
    {
        if (!IsShown())
        {
            m_scriptBrowserRefreshTicks = 0;
            return;
        }

        refresh_context_list();

        ++m_scriptBrowserRefreshTicks;
        if (m_scriptBrowserRefreshTicks < SCRIPT_BROWSER_REFRESH_INTERVAL_TICKS)
        {
            return;
        }

        refresh_script_browser();
        m_scriptBrowserRefreshTicks = 0;
    }

    void ScriptingView::on_script_browser_item_activated(const wxTreeEvent& event)
    {
        if (!m_scriptBrowser)
        {
            return;
        }

        const auto item = event.GetItem();
        if (!item.IsOk())
        {
            return;
        }

        auto* data = script_browser_item_data(*m_scriptBrowser, item);
        if (!data || data->isDirectory)
        {
            return;
        }

        std::ignore = open_script_file(data->path);
    }

    void ScriptingView::on_script_browser_right_click(const wxTreeEvent& event)
    {
        if (!m_scriptBrowser)
        {
            return;
        }

        auto selectedItem = event.GetItem();
        if (!selectedItem.IsOk())
        {
            selectedItem = m_scriptBrowser->GetSelection();
        }
        if (selectedItem.IsOk())
        {
            m_scriptBrowser->SelectItem(selectedItem);
        }

        auto* data = selectedItem.IsOk()
            ? script_browser_item_data(*m_scriptBrowser, selectedItem)
            : nullptr;

        const auto rootDirectory = m_viewModel->get_default_script_directory();
        auto targetDirectory = rootDirectory;
        if (data)
        {
            targetDirectory = data->isDirectory ? data->path : data->path.parent_path();
        }

        const bool canRenameOrDelete = data && !paths_equal(data->path, rootDirectory);

        wxMenu menu{};
        const int openId = wxWindow::NewControlId();
        const int newScriptId = wxWindow::NewControlId();
        const int renameId = wxWindow::NewControlId();
        const int deleteId = wxWindow::NewControlId();
        const int refreshId = wxWindow::NewControlId();

        if (data && !data->isDirectory)
        {
            menu.Append(openId,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserMenuOpen")));
            menu.AppendSeparator();
        }

        menu.Append(newScriptId,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserMenuNewScript")));

        if (canRenameOrDelete)
        {
            menu.Append(renameId,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserMenuRename")));
            menu.Append(deleteId,
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserMenuDelete")));
        }

        menu.AppendSeparator();
        menu.Append(refreshId,
            wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.scriptBrowserMenuRefresh")));

        const auto actionId = m_scriptBrowser->GetPopupMenuSelectionFromUser(menu);
        if (actionId == wxID_NONE)
        {
            return;
        }

        if (actionId == openId && data && !data->isDirectory)
        {
            std::ignore = open_script_file(data->path);
            return;
        }

        if (actionId == newScriptId)
        {
            create_script_from_browser(targetDirectory);
            return;
        }

        if (actionId == renameId && canRenameOrDelete)
        {
            rename_script_from_browser(selectedItem);
            return;
        }

        if (actionId == deleteId && canRenameOrDelete)
        {
            delete_script_from_browser(selectedItem);
            return;
        }

        if (actionId == refreshId)
        {
            refresh_script_browser();
        }
    }

    void ScriptingView::on_context_list_right_click(const wxListEvent& event)
    {
        const auto selectedIndex = event.GetIndex();
        if (selectedIndex == wxNOT_FOUND)
        {
            return;
        }

        const auto contextIndex = static_cast<std::size_t>(selectedIndex);
        if (contextIndex >= m_contextCache.size())
        {
            return;
        }

        const auto& context = m_contextCache[contextIndex];
        m_selectedContextId = context.id;
        m_contextList->SetItemState(
            selectedIndex,
            wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
            wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
        refresh_variable_inspector();

        wxMenu menu{};
        const int suspendId = wxWindow::NewControlId();
        const int resumeId = wxWindow::NewControlId();
        const int removeId = wxWindow::NewControlId();

        switch (context.state)
        {
            case Scripting::ScriptState::Running:
            case Scripting::ScriptState::Executing:
            case Scripting::ScriptState::Sleeping:
                menu.Append(suspendId,
                    wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuSuspend")));
                break;
            case Scripting::ScriptState::Suspended:
                menu.Append(resumeId,
                    wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuResume")));
                break;
            case Scripting::ScriptState::Ready:
            case Scripting::ScriptState::Finished:
            case Scripting::ScriptState::Error:
                break;
        }

        menu.Append(removeId, wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextMenuRemove")));

        const auto selection = GetPopupMenuSelectionFromUser(menu, event.GetPoint());
        if (selection == wxID_NONE)
        {
            return;
        }

        StatusCode status = StatusCode::STATUS_OK;
        if (selection == suspendId)
        {
            status = m_viewModel->suspend_context(context.id);
        }
        else if (selection == resumeId)
        {
            status = m_viewModel->resume_context(context.id);
        }
        else if (selection == removeId)
        {
            status = m_viewModel->remove_context(context.id);
        }

        if (status != StatusCode::STATUS_OK)
        {
            wxMessageBox(
                wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                wxOK | wxICON_ERROR, this);
        }

        refresh_context_list();
    }

    void ScriptingView::on_context_list_selection_changed(wxListEvent& event)
    {
        event.Skip();

        if (!m_contextList)
        {
            m_selectedContextId.reset();
            refresh_variable_inspector();
            return;
        }

        const auto selectedIndex = m_contextList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex == wxNOT_FOUND)
        {
            m_selectedContextId.reset();
            refresh_variable_inspector();
            return;
        }

        const auto contextIndex = static_cast<std::size_t>(selectedIndex);
        if (contextIndex >= m_contextCache.size())
        {
            m_selectedContextId.reset();
            refresh_variable_inspector();
            return;
        }

        m_selectedContextId = m_contextCache[contextIndex].id;
        refresh_variable_inspector();
    }

    void ScriptingView::initialize_autocomplete()
    {
        auto [completionWords, memberCompletionWords] = build_autocomplete_data();
        m_completionWords = std::move(completionWords);
        m_memberCompletionWords = std::move(memberCompletionWords);
    }

    void ScriptingView::initialize_call_tips()
    {
        m_callTipSignatures = build_call_tip_data();
    }

    void ScriptingView::show_autocomplete_for_prefix(const std::string& prefix) const
    {
        if (!m_codeEditor)
        {
            return;
        }

        const auto payload = build_autocomplete_payload(m_completionWords, prefix);
        if (payload.empty())
        {
            if (m_codeEditor->AutoCompActive())
            {
                m_codeEditor->AutoCompCancel();
            }
            return;
        }

        if (m_codeEditor->AutoCompActive())
        {
            m_codeEditor->AutoCompCancel();
        }

        m_codeEditor->AutoCompShow(static_cast<int>(prefix.size()), wxString::FromUTF8(payload));
    }

    void ScriptingView::show_member_autocomplete(const std::string& symbol)
    {
        if (!m_codeEditor || symbol.empty())
        {
            return;
        }

        auto memberIt = m_memberCompletionWords.find(symbol);
        if (memberIt == m_memberCompletionWords.end())
        {
            memberIt = m_memberCompletionWords.find(to_lower_ascii(symbol));
            if (memberIt == m_memberCompletionWords.end())
            {
                return;
            }
        }

        const auto payload = build_autocomplete_payload(memberIt->second, {});
        if (payload.empty())
        {
            return;
        }

        if (m_codeEditor->AutoCompActive())
        {
            m_codeEditor->AutoCompCancel();
        }

        m_codeEditor->AutoCompShow(0, wxString::FromUTF8(payload));
    }

    void ScriptingView::show_call_tip_for_word(const std::string& functionName)
    {
        if (!m_codeEditor)
        {
            return;
        }

        auto callTipIt = m_callTipSignatures.find(functionName);
        if (callTipIt == m_callTipSignatures.end())
        {
            callTipIt = m_callTipSignatures.find(to_lower_ascii(functionName));
            if (callTipIt == m_callTipSignatures.end())
            {
                if (m_codeEditor->CallTipActive())
                {
                    m_codeEditor->CallTipCancel();
                }
                return;
            }
        }

        if (m_codeEditor->CallTipActive())
        {
            m_codeEditor->CallTipCancel();
        }

        m_codeEditor->CallTipShow(m_codeEditor->GetCurrentPos(), wxString::FromUTF8(callTipIt->second));
    }

    void ScriptingView::on_editor_char_added(wxStyledTextEvent& event)
    {
        event.Skip();

        if (!m_codeEditor)
        {
            return;
        }

        const int key = event.GetKey();
        if (key == '\n')
        {
            apply_newline_indentation(*m_codeEditor);
        }
        else if (key == '}')
        {
            apply_closing_brace_outdent(*m_codeEditor);
        }

        if (key == '(')
        {
            if (m_codeEditor->AutoCompActive())
            {
                m_codeEditor->AutoCompCancel();
            }

            show_call_tip_for_word(function_name_before_paren(*m_codeEditor));
            return;
        }

        if (key == ')' && m_codeEditor->CallTipActive())
        {
            m_codeEditor->CallTipCancel();
        }

        if (key == '.')
        {
            show_member_autocomplete(symbol_before_dot(*m_codeEditor));
            return;
        }

        if (is_identifier_char(key))
        {
            show_autocomplete_for_prefix(current_word_prefix(*m_codeEditor));
            return;
        }

        if (!m_codeEditor->AutoCompActive())
        {
            return;
        }

        const auto isWhitespace = key >= 0 && key <= 255 && std::isspace(static_cast<unsigned char>(key)) != 0;
        if (isWhitespace || key == ';' || key == ',' || key == '(' || key == ')' ||
            key == '{' || key == '}' || key == '[' || key == ']')
        {
            m_codeEditor->AutoCompCancel();
        }
    }

    void ScriptingView::on_editor_key_down(wxKeyEvent& event)
    {
        if (!m_codeEditor)
        {
            event.Skip();
            return;
        }

        if (event.ControlDown() && !event.AltDown() && event.GetKeyCode() == WXK_SPACE)
        {
            show_autocomplete_for_prefix(current_word_prefix(*m_codeEditor));
            return;
        }

        if (event.GetKeyCode() == WXK_ESCAPE)
        {
            bool handled{};
            if (m_codeEditor->AutoCompActive())
            {
                m_codeEditor->AutoCompCancel();
                handled = true;
            }

            if (m_codeEditor->CallTipActive())
            {
                m_codeEditor->CallTipCancel();
                handled = true;
            }

            if (handled)
            {
                return;
            }
        }

        event.Skip();
    }

    void ScriptingView::on_editor_margin_click(wxStyledTextEvent& event)
    {
        if (!m_codeEditor)
        {
            event.Skip();
            return;
        }

        if (event.GetMargin() == ScriptingViewValues::LINE_NUMBER_MARGIN)
        {
            const auto line = m_codeEditor->LineFromPosition(event.GetPosition());
            if (line < 0 || line >= m_codeEditor->GetLineCount())
            {
                return;
            }

            constexpr int markerMask = 1 << ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER;
            const bool hasBreakpoint = (m_codeEditor->MarkerGet(line) & markerMask) != 0;
            const int lineNumber = line + 1;

            if (hasBreakpoint)
            {
                m_codeEditor->MarkerDelete(line, ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER);
            }
            else
            {
                m_codeEditor->MarkerAdd(line, ScriptingViewValues::SCRIPT_BREAKPOINT_MARKER);
            }

            bool updateFailed{};
            for (const auto& context : m_viewModel->get_context_list())
            {
                const auto status = hasBreakpoint
                    ? m_viewModel->remove_context_breakpoint(context.id, lineNumber)
                    : m_viewModel->set_context_breakpoint(context.id, lineNumber);

                if (status == StatusCode::STATUS_OK ||
                    status == StatusCode::STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS ||
                    status == StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND ||
                    status == StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE)
                {
                    continue;
                }

                updateFailed = true;
            }

            if (updateFailed)
            {
                wxMessageBox(
                    wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.contextActionFailed")),
                    wxString::FromUTF8(m_languageService.fetch_translation("general.error")),
                    wxOK | wxICON_ERROR, this);
            }

            return;
        }

        if (event.GetMargin() != ScriptingViewValues::FOLD_MARGIN)
        {
            event.Skip();
            return;
        }

        const auto line = m_codeEditor->LineFromPosition(event.GetPosition());
        const auto foldLevel = m_codeEditor->GetFoldLevel(line);
        if ((foldLevel & wxSTC_FOLDLEVELHEADERFLAG) != 0)
        {
            m_codeEditor->ToggleFold(line);
        }
    }

    void ScriptingView::on_editor_update_ui(wxStyledTextEvent& event)
    {
        event.Skip();

        if (!m_codeEditor)
        {
            return;
        }

        if (m_minimap)
        {
            const int firstVisibleLine = m_codeEditor->GetFirstVisibleLine();
            if (m_minimap->GetFirstVisibleLine() != firstVisibleLine)
            {
                m_minimap->ScrollToLine(firstVisibleLine);
            }
        }

        constexpr auto is_brace = [](const int ch) -> bool
        {
            return ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '{' || ch == '}';
        };

        const int currentPosition = m_codeEditor->GetCurrentPos();
        int bracePosition = wxSTC_INVALID_POSITION;

        if (currentPosition > 0)
        {
            const auto previousChar = m_codeEditor->GetCharAt(currentPosition - 1);
            if (is_brace(previousChar))
            {
                bracePosition = currentPosition - 1;
            }
        }

        if (bracePosition == wxSTC_INVALID_POSITION)
        {
            const auto currentChar = m_codeEditor->GetCharAt(currentPosition);
            if (is_brace(currentChar))
            {
                bracePosition = currentPosition;
            }
        }

        if (bracePosition == wxSTC_INVALID_POSITION)
        {
            m_codeEditor->BraceHighlight(wxSTC_INVALID_POSITION, wxSTC_INVALID_POSITION);
            return;
        }

        const auto matchingPosition = m_codeEditor->BraceMatch(bracePosition);
        if (matchingPosition == wxSTC_INVALID_POSITION)
        {
            m_codeEditor->BraceBadLight(bracePosition);
            return;
        }

        m_codeEditor->BraceHighlight(bracePosition, matchingPosition);
    }

    std::string ScriptingView::execution_error_message(const StatusCode status) const
    {
        switch (status)
        {
            case StatusCode::STATUS_ERROR_SCRIPT_COMPILE_FAILED:
                return m_languageService.fetch_translation("scriptingView.ui.executeFailedCompile");
            case StatusCode::STATUS_ERROR_SCRIPT_PREPARE_FAILED:
                return m_languageService.fetch_translation("scriptingView.ui.executeFailedPrepare");
            case StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_CREATION_FAILED:
                return m_languageService.fetch_translation("scriptingView.ui.executeFailedContextCreation");
            case StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED:
                return m_languageService.fetch_translation("scriptingView.ui.executeFailedEngine");
            default:
                return fmt::format(
                    fmt::runtime(m_languageService.fetch_translation("scriptingView.ui.executeFailedStatus")),
                    static_cast<int>(status));
        }
    }

    void ScriptingView::append_output_line(const int style, std::string message) const
    {
        if (!m_outputPanel)
        {
            return;
        }

        const auto timestamp = wxDateTime::Now().FormatISOTime().ToStdString();
        const auto line = fmt::format("[{}] {}\n", timestamp, std::move(message));

        m_outputPanel->SetReadOnly(false);
        const auto start = m_outputPanel->GetTextLength();
        m_outputPanel->AppendText(wxString::FromUTF8(line));
        const auto end = m_outputPanel->GetTextLength();
        m_outputPanel->StartStyling(start);
        m_outputPanel->SetStyling(end - start, style);
        m_outputPanel->SetCurrentPos(end);
        m_outputPanel->ScrollToLine(m_outputPanel->GetLineCount());
        m_outputPanel->SetReadOnly(true);
    }

    void ScriptingView::refresh_variable_inspector()
    {
        if (!m_variableInspector)
        {
            return;
        }

        m_variableInspector->DeleteAllItems();

        if (!m_selectedContextId.has_value())
        {
            return;
        }

        const auto contextIt = std::ranges::find_if(m_contextCache,
            [selectedId = *m_selectedContextId](const Scripting::ContextInfo& context)
            {
                return context.id == selectedId;
            });
        if (contextIt == m_contextCache.end())
        {
            m_selectedContextId.reset();
            return;
        }

        if (contextIt->state != Scripting::ScriptState::Suspended)
        {
            return;
        }

        auto variables = m_viewModel->get_context_variables(*m_selectedContextId);
        if (!variables.has_value())
        {
            return;
        }

        for (const auto& variable : variables.value())
        {
            wxVector<wxVariant> row{};
            row.emplace_back(wxString::FromUTF8(variable.name));
            row.emplace_back(wxString::FromUTF8(variable.type));
            row.emplace_back(wxString::FromUTF8(variable.value));
            m_variableInspector->AppendItem(row);
        }
    }

    void ScriptingView::on_editor_modified(wxStyledTextEvent& event)
    {
        event.Skip();

        auto* editor = dynamic_cast<wxStyledTextCtrl*>(event.GetEventObject());
        if (!editor)
        {
            m_isDirty = has_dirty_tabs();
            return;
        }

        for (std::size_t i{}; i < m_tabs.size(); ++i)
        {
            auto& tab = m_tabs[i];
            if (tab.editor != editor)
            {
                continue;
            }

            tab.isDirty = tab.editor->GetModify();
            update_tab_title(i);
            if (editor == m_codeEditor)
            {
                sync_minimap_content();
            }
            break;
        }

        m_isDirty = has_dirty_tabs();
    }

    void ScriptingView::refresh_context_list()
    {
        auto newContexts = m_viewModel->get_context_list();
        const auto newCount = static_cast<long>(newContexts.size());
        const auto oldCount = m_contextList->GetItemCount();

        bool needsUpdate = (newCount != oldCount);
        if (!needsUpdate)
        {
            for (std::size_t i{}; i < newContexts.size(); ++i)
            {
                if (newContexts[i].id != m_contextCache[i].id ||
                    newContexts[i].name != m_contextCache[i].name ||
                    newContexts[i].state != m_contextCache[i].state)
                {
                    needsUpdate = true;
                    break;
                }
            }
        }

        if (!needsUpdate)
        {
            return;
        }

        std::optional<Scripting::ContextId> selectedContextId = m_selectedContextId;
        const auto selectedIndex = m_contextList->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
        if (selectedIndex != wxNOT_FOUND)
        {
            const auto previousContextIndex = static_cast<std::size_t>(selectedIndex);
            if (previousContextIndex < m_contextCache.size())
            {
                selectedContextId = m_contextCache[previousContextIndex].id;
            }
        }

        m_contextCache = std::move(newContexts);

        m_contextList->Freeze();

        while (m_contextList->GetItemCount() > newCount)
        {
            m_contextList->DeleteItem(m_contextList->GetItemCount() - 1);
        }

        long restoredSelection = wxNOT_FOUND;
        for (long i{}; i < newCount; ++i)
        {
            const auto& ctx = m_contextCache[static_cast<std::size_t>(i)];

            if (i >= oldCount)
            {
                m_contextList->InsertItem(i, wxString::FromUTF8(ctx.name));
            }
            else
            {
                m_contextList->SetItem(i, 0, wxString::FromUTF8(ctx.name));
            }

            m_contextList->SetItem(i, 1, state_to_string(ctx.state));
            m_contextList->SetItemTextColour(i, state_to_colour(ctx.state));

            if (selectedContextId.has_value() && ctx.id == *selectedContextId)
            {
                restoredSelection = i;
            }
        }

        if (restoredSelection != wxNOT_FOUND)
        {
            m_contextList->SetItemState(
                restoredSelection,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED,
                wxLIST_STATE_SELECTED | wxLIST_STATE_FOCUSED);
            m_contextList->EnsureVisible(restoredSelection);
            m_selectedContextId = *selectedContextId;
        }
        else
        {
            m_selectedContextId.reset();
        }

        m_contextList->Thaw();
        refresh_variable_inspector();
    }

    wxString ScriptingView::state_to_string(const Scripting::ScriptState state) const
    {
        switch (state)
        {
            case Scripting::ScriptState::Ready:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateReady"));
            case Scripting::ScriptState::Running:
            case Scripting::ScriptState::Executing:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateRunning"));
            case Scripting::ScriptState::Suspended:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateSuspended"));
            case Scripting::ScriptState::Sleeping:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateSleeping"));
            case Scripting::ScriptState::Finished:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateFinished"));
            case Scripting::ScriptState::Error:
                return wxString::FromUTF8(m_languageService.fetch_translation("scriptingView.ui.stateError"));
        }

        std::unreachable();
    }

    wxColour ScriptingView::state_to_colour(const Scripting::ScriptState state) const
    {
        switch (state)
        {
            case Scripting::ScriptState::Running:
            case Scripting::ScriptState::Executing:
                return {34, 139, 34};
            case Scripting::ScriptState::Suspended:
            case Scripting::ScriptState::Sleeping:
                return {200, 170, 0};
            case Scripting::ScriptState::Finished:
            case Scripting::ScriptState::Error:
                return {200, 50, 50};
            case Scripting::ScriptState::Ready:
                return wxSystemSettings::GetColour(wxSYS_COLOUR_WINDOWTEXT);
        }

        std::unreachable();
    }
}
