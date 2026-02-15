//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/debugger/debuggerworker.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/runtime/iregistry.hh>
#include <vertex/log/ilog.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/theme.hh>

#include <sdk/statuscode.h>

#include <functional>
#include <memory>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

namespace Vertex::Model
{
    using DebuggerEventHandler = std::function<void(const Debugger::DebuggerEvent&)>;

    class DebuggerModel final
    {
    public:
        explicit DebuggerModel(
            Configuration::ISettings& settingsService,
            Runtime::ILoader& loaderService,
            Log::ILog& loggerService
        );

        ~DebuggerModel();

        [[nodiscard]] StatusCode start_worker() const;
        [[nodiscard]] StatusCode stop_worker() const;

        void set_event_handler(DebuggerEventHandler handler);

        void attach_debugger() const;
        void detach_debugger() const;
        void continue_execution() const;
        void pause_execution() const;
        void step_into() const;
        void step_over() const;
        void step_out() const;
        void run_to_address(std::uint64_t address) const;
        void navigate_to_address(std::uint64_t address);
        void refresh_data() const;

        void add_breakpoint(std::uint64_t address,
            Debugger::BreakpointType type = Debugger::BreakpointType::Software);
        void remove_breakpoint(std::uint32_t breakpointId);
        void remove_breakpoint_at(std::uint64_t address);
        void toggle_breakpoint(std::uint64_t address);
        void enable_breakpoint(std::uint32_t breakpointId, bool enable);

        [[nodiscard]] StatusCode set_watchpoint(std::uint64_t address, std::uint32_t size, std::uint32_t* outWatchpointId = nullptr);
        [[nodiscard]] StatusCode remove_watchpoint(std::uint32_t watchpointId);
        [[nodiscard]] StatusCode enable_watchpoint(std::uint32_t watchpointId, bool enable);
        void on_watchpoint_hit(std::uint32_t watchpointId, std::uint64_t accessorAddress);
        [[nodiscard]] const std::vector<Debugger::Watchpoint>& get_cached_watchpoints() const;

        [[nodiscard]] bool is_attached() const;
        [[nodiscard]] Debugger::DebuggerState get_debugger_state() const;
        [[nodiscard]] std::uint64_t get_current_address() const;
        [[nodiscard]] std::uint32_t get_current_thread_id() const;

        [[nodiscard]] const Debugger::RegisterSet& get_cached_registers() const;
        [[nodiscard]] const Debugger::DisassemblyRange& get_cached_disassembly() const;
        [[nodiscard]] const Debugger::CallStack& get_cached_call_stack() const;
        [[nodiscard]] const std::vector<Debugger::Breakpoint>& get_cached_breakpoints() const;
        [[nodiscard]] const std::vector<Debugger::ModuleInfo>& get_cached_modules() const;
        [[nodiscard]] const std::vector<Debugger::ThreadInfo>& get_cached_threads() const;
        [[nodiscard]] bool has_breakpoint_at(std::uint64_t address) const;

        [[nodiscard]] std::vector<Runtime::RegisterCategoryInfo> get_register_categories() const;
        [[nodiscard]] std::vector<Runtime::RegisterInfo> get_register_definitions() const;
        [[nodiscard]] std::vector<Runtime::RegisterInfo> get_registers_by_category(std::string_view categoryId) const;
        [[nodiscard]] std::vector<Runtime::FlagBitInfo> get_flag_bits(std::string_view flagsRegisterName) const;
        [[nodiscard]] std::optional<Runtime::ArchInfo> get_architecture_info() const;
        [[nodiscard]] bool has_registry_data() const;

        [[nodiscard]] Theme get_theme() const;

        [[nodiscard]] bool get_ui_state_bool(std::string_view key, bool defaultValue) const;
        void set_ui_state_bool(std::string_view key, bool value) const;
        [[nodiscard]] std::string get_ui_state_string(std::string_view key, std::string_view defaultValue) const;
        void set_ui_state_string(std::string_view key, std::string_view value) const;

        [[nodiscard]] StatusCode disassemble_at_address(std::uint64_t address);
        [[nodiscard]] StatusCode disassemble_extend_up(std::uint64_t fromAddress, std::size_t byteCount = 512);
        [[nodiscard]] StatusCode disassemble_extend_down(std::uint64_t fromAddress, std::size_t byteCount = 512);
        [[nodiscard]] StatusCode load_modules();
        [[nodiscard]] StatusCode load_module_imports_exports(std::string_view moduleName);

        [[nodiscard]] const std::vector<Debugger::ImportEntry>& get_cached_imports() const;
        [[nodiscard]] const std::vector<Debugger::ExportEntry>& get_cached_exports() const;

        [[nodiscard]] StatusCode read_registers();
        [[nodiscard]] StatusCode read_registers_for_thread(std::uint32_t threadId);

        [[nodiscard]] StatusCode load_threads();

        void clear_cached_data();

    private:
        static constexpr auto MODEL_NAME = "DebuggerModel";
        static constexpr std::size_t MAX_DISASSEMBLY_LINES = 1000;
        static constexpr std::size_t TRIM_LINES_COUNT = 200;

        void on_worker_event(const Debugger::DebuggerEvent& evt);

        Configuration::ISettings& m_settingsService;
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;

        std::unique_ptr<Debugger::DebuggerWorker> m_worker;

        DebuggerEventHandler m_eventHandler;

        Debugger::DebuggerSnapshot m_cachedSnapshot{};

        Debugger::RegisterSet m_cachedRegisters{};
        Debugger::DisassemblyRange m_cachedDisassembly{};
        Debugger::CallStack m_cachedCallStack{};
        std::vector<Debugger::Breakpoint> m_cachedBreakpoints{};
        std::vector<Debugger::ModuleInfo> m_cachedModules{};
        std::vector<Debugger::ThreadInfo> m_cachedThreads{};

        std::vector<Debugger::ImportEntry> m_cachedImports{};
        std::vector<Debugger::ExportEntry> m_cachedExports{};

        std::vector<Debugger::Watchpoint> m_cachedWatchpoints{};
    };
}
