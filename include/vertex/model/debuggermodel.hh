//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/debuggertypes.hh>
#include <vertex/debugger/debuggerengine.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/runtime/iregistry.hh>
#include <vertex/log/ilog.hh>
#include <vertex/configuration/isettings.hh>
#include <vertex/theme.hh>

#include <sdk/statuscode.h>
#include <sdk/disassembler.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <span>
#include <unordered_map>
#include <vector>
#include <string>
#include <string_view>
#include <optional>

namespace Vertex::Runtime { class Plugin; }

namespace Vertex::Model
{
    using DebuggerEventHandler = std::move_only_function<void(Debugger::DirtyFlags, const Debugger::EngineSnapshot&)>;
    using ExtensionResultHandler = std::move_only_function<void(bool isTop, Debugger::ExtensionResult result)>;
    using XrefResultCallback = std::function<void(std::vector<Debugger::XrefEntry>)>;

    class DebuggerModel final
    {
    public:
        explicit DebuggerModel(
            Configuration::ISettings& settingsService,
            Runtime::ILoader& loaderService,
            Log::ILog& loggerService,
            Thread::IThreadDispatcher& dispatcher
        );

        ~DebuggerModel();

        [[nodiscard]] StatusCode start_engine() const;
        [[nodiscard]] StatusCode stop_engine() const;

        [[nodiscard]] Debugger::DebuggerEngine& engine() noexcept { return *m_engine; }
        [[nodiscard]] const Debugger::DebuggerEngine& engine() const noexcept { return *m_engine; }

        void set_event_handler(DebuggerEventHandler handler);
        void set_extension_result_handler(ExtensionResultHandler handler);

        void attach_debugger() const;
        void detach_debugger() const;
        void continue_execution() const;
        void pause_execution() const;
        void step_into() const;
        void step_over() const;
        void step_out() const;
        void run_to_address(std::uint64_t address) const;
        void navigate_to_address(std::uint64_t address);
        void refresh_data();

        void add_breakpoint(std::uint64_t address,
            Debugger::BreakpointType type = Debugger::BreakpointType::Software);
        void remove_breakpoint(std::uint32_t breakpointId);
        StatusCode retry_breakpoint(std::uint32_t breakpointId);
        void remove_breakpoint_at(std::uint64_t address);
        void toggle_breakpoint(std::uint64_t address);
        void enable_breakpoint(std::uint32_t breakpointId, bool enable);

        void set_breakpoint_condition(std::uint32_t breakpointId,
                                      ::BreakpointConditionType conditionType,
                                      std::string_view expression,
                                      std::uint32_t hitCountTarget);
        void clear_breakpoint_condition(std::uint32_t breakpointId);

        [[nodiscard]] StatusCode set_watchpoint(std::uint64_t address, std::uint32_t size, std::uint32_t* outWatchpointId = nullptr);
        [[nodiscard]] StatusCode remove_watchpoint(std::uint32_t watchpointId);
        [[nodiscard]] StatusCode enable_watchpoint(std::uint32_t watchpointId, bool enable);
        void on_watchpoint_hit(std::uint32_t watchpointId, std::uint64_t accessorAddress);
        [[nodiscard]] const std::vector<Debugger::Watchpoint>& get_cached_watchpoints() const;
        [[nodiscard]] const std::vector<Debugger::WatchVariable>& get_cached_watch_variables() const;
        [[nodiscard]] const std::vector<Debugger::LocalVariable>& get_cached_local_variables() const;
        StatusCode add_watch_variable(std::string_view expression);
        StatusCode remove_watch_variable(std::uint32_t watchId);
        StatusCode modify_watch_variable(std::uint32_t watchId, std::string_view newValue);
        StatusCode set_watch_expanded(std::uint32_t watchId, bool expanded);
        void request_watch_data();

        [[nodiscard]] bool is_attached() const;
        [[nodiscard]] Debugger::DebuggerState get_debugger_state() const;
        [[nodiscard]] std::uint64_t get_current_address() const;
        [[nodiscard]] std::uint32_t get_current_thread_id() const;
        [[nodiscard]] const Debugger::ExceptionData& get_cached_exception() const;

        [[nodiscard]] const Debugger::RegisterSet& get_cached_registers() const;
        [[nodiscard]] const Debugger::DisassemblyRange& get_cached_disassembly() const;
        [[nodiscard]] const Debugger::CallStack& get_cached_call_stack() const;
        [[nodiscard]] const std::vector<Debugger::Breakpoint>& get_cached_breakpoints() const;
        [[nodiscard]] const std::vector<Debugger::ModuleInfo>& get_cached_modules() const;
        [[nodiscard]] const std::vector<Debugger::ThreadInfo>& get_cached_threads() const;
        [[nodiscard]] const std::vector<Debugger::LogEntry>& get_cached_logs() const;
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

        void request_disassembly(std::uint64_t address);
        void request_disassembly_extend_up(std::uint64_t fromAddress, std::size_t byteCount = 512);
        void request_disassembly_extend_down(std::uint64_t fromAddress, std::size_t byteCount = 512);

        void query_xrefs_to(std::uint64_t address, XrefResultCallback callback) const;
        void query_xrefs_from(std::uint64_t address, XrefResultCallback callback);
        void request_modules();
        void request_module_imports_exports(std::string_view moduleName);

        [[nodiscard]] const std::vector<Debugger::ImportEntry>& get_cached_imports() const;
        [[nodiscard]] const std::vector<Debugger::ExportEntry>& get_cached_exports() const;

        void request_registers();
        void request_registers_for_thread(std::uint32_t threadId);

        void request_call_stack();
        void request_call_stack_for_thread(std::uint32_t threadId);

        StatusCode select_thread(std::uint32_t threadId);
        StatusCode suspend_thread(std::uint32_t threadId);
        StatusCode resume_thread(std::uint32_t threadId);
        StatusCode write_register(std::string_view registerName, std::uint64_t value);

        void request_threads();
        void request_memory(std::uint64_t address, std::size_t size);
        [[nodiscard]] StatusCode write_memory(std::uint64_t address, std::span<const std::uint8_t> data);
        [[nodiscard]] const Debugger::MemoryBlock& get_cached_memory() const;

        void clear_cached_data();

        [[nodiscard]] static Debugger::DisassemblyLine convert_disasm_result(const ::DisassemblerResult& instr, std::uint64_t currentAddress);
        static void resolve_disassembly_symbols(Debugger::DisassemblyRange& range,
                                                 std::span<const Debugger::ModuleInfo> modules,
                                                 const std::unordered_map<std::uint64_t, std::string>& symbolTable);

    private:
        static constexpr std::string_view MODEL_NAME{"DebuggerModel"};
        static constexpr std::size_t MAX_DISASSEMBLY_LINES = 1000;
        static constexpr std::size_t TRIM_LINES_COUNT = 200;
        static constexpr std::size_t MAX_MEMORY_VIEW_BYTES = 1024 * 1024;
        static constexpr std::size_t XREF_SCAN_CHUNK_BYTES = 4096;
        static constexpr std::size_t XREF_SCAN_CHUNK_INSTRUCTIONS = 500;
        static constexpr std::size_t XREF_BACKWARD_PROBE_BYTES = 4096;

        enum class QueryFamily : std::uint8_t
        {
            Registers,
            Threads,
            CallStack,
            Disassembly,
            Modules,
            Memory
        };

        struct QueryTracker final
        {
            std::atomic<bool> inflight{false};
            std::atomic<bool> pendingRedispatch{false};
            std::uint64_t dispatchGeneration{};
        };

        struct PendingBreakpointAdd final
        {
            std::uint64_t address{};
            Debugger::BreakpointType type{Debugger::BreakpointType::Software};
        };

        void on_engine_event(Debugger::DirtyFlags flags, const Debugger::EngineSnapshot& snapshot);
        void resync_breakpoints_from_plugin();
        void resync_watchpoints_from_plugin();
        void request_symbol_table();

        Configuration::ISettings& m_settingsService;
        Runtime::ILoader& m_loaderService;
        Log::ILog& m_loggerService;
        Thread::IThreadDispatcher& m_dispatcher;

        std::unique_ptr<Debugger::DebuggerEngine> m_engine;

        DebuggerEventHandler m_eventHandler;
        ExtensionResultHandler m_extensionResultHandler;

        mutable std::mutex m_cacheMutex{};

        Debugger::EngineSnapshot m_cachedSnapshot{};
        std::uint64_t m_navigationAddress{};
        std::uint64_t m_generation{};

        Debugger::RegisterSet m_cachedRegisters{};
        Debugger::DisassemblyRange m_cachedDisassembly{};
        Debugger::CallStack m_cachedCallStack{};
        std::vector<Debugger::Breakpoint> m_cachedBreakpoints{};
        std::unordered_map<std::uint32_t, PendingBreakpointAdd> m_pendingBreakpointAdds{};
        std::vector<Debugger::ModuleInfo> m_cachedModules{};
        std::vector<Debugger::ThreadInfo> m_cachedThreads{};
        std::vector<Debugger::LogEntry> m_cachedLogs{};
        Debugger::MemoryBlock m_cachedMemoryBlock{};

        std::vector<Debugger::ImportEntry> m_cachedImports{};
        std::vector<Debugger::ExportEntry> m_cachedExports{};

        std::vector<Debugger::Watchpoint> m_cachedWatchpoints{};
        std::vector<Debugger::WatchVariable> m_cachedWatchVariables{};
        std::vector<Debugger::LocalVariable> m_cachedLocalVariables{};
        Debugger::ExceptionData m_cachedException{};

        std::unordered_map<std::uint64_t, std::string> m_symbolTable{};

        QueryTracker m_queryRegisters{};
        QueryTracker m_queryThreads{};
        QueryTracker m_queryCallStack{};
        QueryTracker m_queryDisassembly{};
        QueryTracker m_queryModules{};
        QueryTracker m_queryMemory{};

        Debugger::EngineState m_lastEngineState{Debugger::EngineState::Idle};
        std::uint64_t m_lastWatchpointHitGeneration {};
        std::uint64_t m_memoryWriteGeneration {};

        std::atomic<std::uint64_t> m_pendingDisasmAddress{};
        std::atomic<std::uint64_t> m_pendingMemoryAddress{};
        std::atomic<std::uint64_t> m_pendingMemorySize{4096};
        std::atomic<bool> m_hasMemoryRequest{};
    };
}
