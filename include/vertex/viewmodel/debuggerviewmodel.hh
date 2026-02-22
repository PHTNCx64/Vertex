//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <memory>
#include <string>
#include <string_view>
#include <functional>
#include <vector>
#include <optional>

#include <vertex/event/eventbus.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/model/debuggermodel.hh>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/debugger/debuggerworker.hh>
#include <vertex/runtime/iregistry.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>
#include <vertex/theme.hh>

namespace Vertex::ViewModel
{
    class DebuggerViewModel final
    {
    public:
        explicit DebuggerViewModel(
            std::unique_ptr<Model::DebuggerModel> model,
            Event::EventBus& eventBus,
            Log::ILog& logService,
            std::string name = ViewModelName::DEBUGGER
        );

        ~DebuggerViewModel();

        void set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> callback);

        void start_worker() const;
        void stop_worker() const;

        void attach_debugger() const;
        void detach_debugger() const;
        [[nodiscard]] bool is_attached() const;
        [[nodiscard]] Debugger::DebuggerState get_state() const;

        void continue_execution() const;
        void pause_execution() const;
        void step_into() const;
        void step_over() const;
        void step_out() const;
        void run_to_cursor(std::uint64_t address) const;

        void navigate_to_address(std::uint64_t address) const;
        void refresh_data() const;

        [[nodiscard]] StatusCode disassemble_at_address(std::uint64_t address) const;
        [[nodiscard]] StatusCode disassemble_extend_up(std::uint64_t fromAddress) const;
        [[nodiscard]] StatusCode disassemble_extend_down(std::uint64_t fromAddress) const;
        [[nodiscard]] StatusCode load_modules_and_disassemble() const;

        void ensure_data_loaded() const;

        [[nodiscard]] StatusCode read_registers() const;

        [[nodiscard]] StatusCode load_threads() const;

        void clear_cached_data() const;

        void toggle_breakpoint(std::uint64_t address) const;
        void add_breakpoint(std::uint64_t address, Debugger::BreakpointType type = Debugger::BreakpointType::Software) const;
        void remove_breakpoint(std::uint32_t id) const;
        void remove_breakpoint_at(std::uint64_t address) const;
        void enable_breakpoint(std::uint32_t id, bool enable) const;

        void set_watchpoint(std::uint64_t address, std::uint32_t size) const;
        void remove_watchpoint(std::uint32_t id) const;
        void enable_watchpoint(std::uint32_t id, bool enable) const;
        [[nodiscard]] const std::vector<Debugger::Watchpoint>& get_watchpoints() const;

        [[nodiscard]] std::uint64_t get_current_address() const;
        [[nodiscard]] std::uint32_t get_current_thread_id() const;
        [[nodiscard]] const Debugger::DisassemblyRange& get_disassembly() const;
        [[nodiscard]] const Debugger::RegisterSet& get_registers() const;
        [[nodiscard]] const Debugger::CallStack& get_call_stack() const;
        [[nodiscard]] const std::vector<Debugger::Breakpoint>& get_breakpoints() const;
        [[nodiscard]] const std::vector<Debugger::ModuleInfo>& get_modules() const;
        [[nodiscard]] const std::vector<Debugger::ThreadInfo>& get_threads() const;
        [[nodiscard]] bool has_breakpoint_at(std::uint64_t address) const;

        [[nodiscard]] bool has_exception() const;
        [[nodiscard]] const Debugger::ExceptionData& get_exception_info() const;

        void select_stack_frame(std::uint32_t frameIndex);
        [[nodiscard]] std::uint32_t get_selected_frame_index() const;

        void select_module(std::string_view moduleName);
        [[nodiscard]] std::string get_selected_module() const;

        [[nodiscard]] StatusCode load_module_imports_exports(std::string_view moduleName) const;
        [[nodiscard]] const std::vector<Debugger::ImportEntry>& get_imports() const;
        [[nodiscard]] const std::vector<Debugger::ExportEntry>& get_exports() const;

        [[nodiscard]] std::vector<Runtime::RegisterCategoryInfo> get_register_categories() const;
        [[nodiscard]] std::vector<Runtime::RegisterInfo> get_register_definitions() const;
        [[nodiscard]] std::vector<Runtime::RegisterInfo> get_registers_by_category(std::string_view categoryId) const;
        [[nodiscard]] std::vector<Runtime::FlagBitInfo> get_flag_bits(std::string_view flagsRegisterName) const;
        [[nodiscard]] std::optional<Runtime::ArchInfo> get_architecture_info() const;
        [[nodiscard]] bool has_registry_data() const;

        [[nodiscard]] Theme get_theme() const;

        [[nodiscard]] std::string get_aui_perspective() const;
        void set_aui_perspective(std::string_view perspective) const;

    private:
        void subscribe_to_events();
        void unsubscribe_from_events() const;
        void notify_view_update(ViewUpdateFlags flags) const;
        void on_process_opened(const Event::ProcessOpenEvent& event);
        void on_process_closed();
        void on_debugger_event(const Debugger::DebuggerEvent& evt) const;

        std::uint32_t m_selectedStackFrame {};
        std::string m_selectedModule {};
        std::string m_viewModelName {};

        std::unique_ptr<Model::DebuggerModel> m_model {};
        std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> m_eventCallback {};

        Event::EventBus& m_eventBus;
        Log::ILog& m_logService;
    };
}
