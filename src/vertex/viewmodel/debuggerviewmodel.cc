//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/debuggerviewmodel.hh>

#include <algorithm>
#include <utility>

#include <fmt/format.h>

#include <vertex/event/eventid.hh>
#include <vertex/event/types/processcloseevent.hh>
#include <vertex/event/types/viewevent.hh>
#include <vertex/event/types/viewupdateevent.hh>

namespace Vertex::ViewModel
{
    DebuggerViewModel::DebuggerViewModel(std::unique_ptr<Model::DebuggerModel> model, Event::EventBus& eventBus, Log::ILog& logService, std::string name)
        : m_viewModelName{std::move(name)},
          m_model{std::move(model)},
          m_eventBus{eventBus},
          m_logService{logService}
    {
        subscribe_to_events();

        m_model->set_event_handler(
          [this](const Debugger::DirtyFlags flags, const Debugger::EngineSnapshot& snapshot)
          {
              on_engine_event(flags, snapshot);
          });
    }

    DebuggerViewModel::~DebuggerViewModel()
    {
        m_model->set_event_handler({});
        m_model->set_extension_result_handler({});
        if (m_initThread.joinable())
        {
            m_initThread.join();
        }
        stop_engine();
        unsubscribe_from_events();
    }

    void DebuggerViewModel::start_engine() const
    {
        if (const auto status = m_model->start_engine(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to start engine (status={})", static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::stop_engine() const
    {
        if (const auto status = m_model->stop_engine(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to stop engine (status={})", static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::on_engine_event(const Debugger::DirtyFlags flags, [[maybe_unused]] const Debugger::EngineSnapshot& snapshot) const
    {
        ViewUpdateFlags viewFlags{};

        if ((flags & Debugger::DirtyFlags::State) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_STATE;
        }
        if ((flags & Debugger::DirtyFlags::Registers) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_REGISTERS;
        }
        if ((flags & Debugger::DirtyFlags::Threads) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_THREADS;
        }
        if ((flags & Debugger::DirtyFlags::CallStack) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_STACK;
        }
        if ((flags & Debugger::DirtyFlags::Modules) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_MODULES;
        }
        if ((flags & Debugger::DirtyFlags::ImportsExports) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_IMPORTS_EXPORTS;
        }
        if ((flags & Debugger::DirtyFlags::Disassembly) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_DISASSEMBLY;
        }
        if ((flags & Debugger::DirtyFlags::Breakpoints) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_BREAKPOINTS | ViewUpdateFlags::DEBUGGER_DISASSEMBLY;
        }
        if ((flags & Debugger::DirtyFlags::Watchpoints) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_WATCHPOINTS;
        }
        if ((flags & Debugger::DirtyFlags::Memory) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_MEMORY;
        }
        if ((flags & Debugger::DirtyFlags::Watches) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_WATCH;
        }
        if ((flags & Debugger::DirtyFlags::Logs) != Debugger::DirtyFlags::None)
        {
            viewFlags = viewFlags | ViewUpdateFlags::DEBUGGER_CONSOLE;
        }

        if (viewFlags != ViewUpdateFlags{})
        {
            notify_view_update(viewFlags);
        }
    }

    void DebuggerViewModel::subscribe_to_events()
    {
        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::VIEW_EVENT,
                                               [this](const Event::ViewEvent& event)
                                               {
                                                   if (m_eventCallback)
                                                   {
                                                       m_eventCallback(Event::VIEW_EVENT, event);
                                                   }
                                               });

        m_eventBus.subscribe<Event::ViewEvent>(m_viewModelName, Event::APPLICATION_SHUTDOWN_EVENT,
                                               [this](const Event::ViewEvent&)
                                               {
                                                   stop_engine();
                                               });

        m_eventBus.subscribe<Event::ProcessOpenEvent>(m_viewModelName, Event::PROCESS_OPEN_EVENT,
                                                      [this](const Event::ProcessOpenEvent& evt)
                                                      {
                                                          on_process_opened(evt);
                                                      });

        m_eventBus.subscribe<Event::ProcessCloseEvent>(m_viewModelName, Event::PROCESS_CLOSED_EVENT,
                                                       [this](const Event::ProcessCloseEvent&)
                                                       {
                                                           on_process_closed();
                                                       });
    }

    void DebuggerViewModel::unsubscribe_from_events() const { m_eventBus.unsubscribe_all(m_viewModelName); }

    void DebuggerViewModel::set_event_callback(std::move_only_function<void(Event::EventId, const Event::VertexEvent&) const> callback) { m_eventCallback = std::move(callback); }

    void DebuggerViewModel::notify_view_update(const ViewUpdateFlags flags) const
    {
        if (m_eventCallback)
        {
            const Event::ViewUpdateEvent event(flags);
            m_eventCallback(Event::VIEW_UPDATE_EVENT, event);
        }
    }

    void DebuggerViewModel::on_process_opened([[maybe_unused]] const Event::ProcessOpenEvent& event)
    {
        m_selectedModule.clear();
        m_selectedStackFrame = 0;

        start_engine();

        std::jthread prevThread{};
        std::swap(prevThread, m_initThread);

        m_initThread = std::jthread(
          [this, prev = std::move(prevThread)]() mutable
          {
              if (prev.joinable())
              {
                  prev.join();
              }

              load_modules_and_disassemble();
          });
    }

    void DebuggerViewModel::on_process_closed()
    {
        detach_debugger();
        stop_engine();
        clear_cached_data();
        m_selectedModule.clear();
        m_selectedStackFrame = 0;
        notify_view_update(ViewUpdateFlags::DEBUGGER_ALL);
    }

    void DebuggerViewModel::attach_debugger() const { m_model->attach_debugger(); }

    void DebuggerViewModel::detach_debugger() const { m_model->detach_debugger(); }

    bool DebuggerViewModel::is_attached() const { return m_model->is_attached(); }

    Debugger::DebuggerState DebuggerViewModel::get_state() const { return m_model->get_debugger_state(); }

    void DebuggerViewModel::continue_execution() const { m_model->continue_execution(); }

    void DebuggerViewModel::pause_execution() const { m_model->pause_execution(); }

    void DebuggerViewModel::step_into() const { m_model->step_into(); }

    void DebuggerViewModel::step_over() const { m_model->step_over(); }

    void DebuggerViewModel::step_out() const { m_model->step_out(); }

    void DebuggerViewModel::run_to_cursor(const std::uint64_t address) const { m_model->run_to_address(address); }

    void DebuggerViewModel::navigate_to_address(const std::uint64_t address) const { m_model->navigate_to_address(address); }

    void DebuggerViewModel::refresh_data() const { m_model->refresh_data(); }

    void DebuggerViewModel::request_disassembly(const std::uint64_t address) const { m_model->request_disassembly(address); }

    void DebuggerViewModel::request_disassembly_extend_up(const std::uint64_t fromAddress) const { m_model->request_disassembly_extend_up(fromAddress); }

    void DebuggerViewModel::request_disassembly_extend_down(const std::uint64_t fromAddress) const { m_model->request_disassembly_extend_down(fromAddress); }

    void DebuggerViewModel::set_extension_result_callback(std::function<void(bool, Debugger::ExtensionResult)> callback) const { m_model->set_extension_result_handler(std::move(callback)); }

    void DebuggerViewModel::query_xrefs_to(const std::uint64_t address, Model::XrefResultCallback callback) const { m_model->query_xrefs_to(address, std::move(callback)); }

    void DebuggerViewModel::query_xrefs_from(const std::uint64_t address, Model::XrefResultCallback callback) const { m_model->query_xrefs_from(address, std::move(callback)); }

    void DebuggerViewModel::load_modules_and_disassemble() const { m_model->request_modules(); }

    void DebuggerViewModel::request_registers() const { m_model->request_registers(); }

    void DebuggerViewModel::request_call_stack() const { m_model->request_call_stack(); }

    void DebuggerViewModel::request_threads() const { m_model->request_threads(); }

    void DebuggerViewModel::select_thread(const std::uint32_t threadId) const
    {
        if (const auto status = m_model->select_thread(threadId); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to select thread {} (status={})", threadId, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::suspend_thread(const std::uint32_t threadId) const
    {
        if (const auto status = m_model->suspend_thread(threadId); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to suspend thread {} (status={})", threadId, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::resume_thread(const std::uint32_t threadId) const
    {
        if (const auto status = m_model->resume_thread(threadId); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to resume thread {} (status={})", threadId, static_cast<int>(status)));
        }
    }

    StatusCode DebuggerViewModel::write_register(const std::string_view registerName, const std::uint64_t value) const
    {
        return m_model->write_register(registerName, value);
    }

    void DebuggerViewModel::request_memory(const std::uint64_t address, const std::size_t size) const { m_model->request_memory(address, size); }

    StatusCode DebuggerViewModel::write_memory(const std::uint64_t address, const std::span<const std::uint8_t> data) const { return m_model->write_memory(address, data); }

    void DebuggerViewModel::ensure_data_loaded() const
    {
        if (get_modules().empty())
        {
            load_modules_and_disassemble();
        }
        else if (get_disassembly().lines.empty() && !get_modules().empty())
        {
            const auto& modules = get_modules();
            request_disassembly(modules[0].baseAddress);
        }

        if (get_registers().generalPurpose.empty())
        {
            request_registers();
        }

        if (get_threads().empty())
        {
            request_threads();
        }

        if (get_call_stack().frames.empty() && is_attached())
        {
            request_call_stack();
        }

        if (is_attached() && get_state() == Debugger::DebuggerState::Paused)
        {
            request_watch_data();
        }
    }

    void DebuggerViewModel::clear_cached_data() const { m_model->clear_cached_data(); }

    void DebuggerViewModel::toggle_breakpoint(const std::uint64_t address) const { m_model->toggle_breakpoint(address); }

    void DebuggerViewModel::add_breakpoint(const std::uint64_t address, const Debugger::BreakpointType type) const { m_model->add_breakpoint(address, type); }

    void DebuggerViewModel::remove_breakpoint(const std::uint32_t id) const { m_model->remove_breakpoint(id); }

    void DebuggerViewModel::retry_breakpoint(const std::uint32_t id) const
    {
        if (const auto status = m_model->retry_breakpoint(id); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to retry breakpoint {} (status={})", id, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::remove_breakpoint_at(const std::uint64_t address) const { m_model->remove_breakpoint_at(address); }

    void DebuggerViewModel::enable_breakpoint(const std::uint32_t id, const bool enable) const { m_model->enable_breakpoint(id, enable); }

    void DebuggerViewModel::set_breakpoint_condition(const std::uint32_t breakpointId, const ::BreakpointConditionType conditionType, const std::string_view expression, const std::uint32_t hitCountTarget) const
    {
        m_model->set_breakpoint_condition(breakpointId, conditionType, expression, hitCountTarget);
    }

    void DebuggerViewModel::clear_breakpoint_condition(const std::uint32_t breakpointId) const { m_model->clear_breakpoint_condition(breakpointId); }

    void DebuggerViewModel::set_watchpoint(const std::uint64_t address, const std::uint32_t size) const
    {
        if (const auto status = m_model->set_watchpoint(address, size); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to set watchpoint at 0x{:X} (status={})", address, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::remove_watchpoint(const std::uint32_t id) const
    {
        if (const auto status = m_model->remove_watchpoint(id); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to remove watchpoint {} (status={})", id, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::enable_watchpoint(const std::uint32_t id, const bool enable) const
    {
        if (const auto status = m_model->enable_watchpoint(id, enable); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to {} watchpoint {} (status={})", enable ? "enable" : "disable", id, static_cast<int>(status)));
        }
    }

    const std::vector<Debugger::Watchpoint>& DebuggerViewModel::get_watchpoints() const { return m_model->get_cached_watchpoints(); }

    void DebuggerViewModel::add_watch_variable(const std::string_view expression) const
    {
        if (const auto status = m_model->add_watch_variable(expression); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to add watch '{}' (status={})", expression, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::remove_watch_variable(const std::uint32_t watchId) const
    {
        if (const auto status = m_model->remove_watch_variable(watchId); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to remove watch {} (status={})", watchId, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::modify_watch_variable(const std::uint32_t watchId, const std::string_view newValue) const
    {
        if (const auto status = m_model->modify_watch_variable(watchId, newValue); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to modify watch {} (status={})", watchId, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::expand_watch_variable(const std::uint32_t watchId, const bool expanded) const
    {
        if (const auto status = m_model->set_watch_expanded(watchId, expanded); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to {} watch {} (status={})", expanded ? "expand" : "collapse", watchId, static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::request_watch_data() const
    {
        m_model->request_watch_data();
    }

    const std::vector<Debugger::WatchVariable>& DebuggerViewModel::get_watch_variables() const { return m_model->get_cached_watch_variables(); }

    const std::vector<Debugger::LocalVariable>& DebuggerViewModel::get_local_variables() const { return m_model->get_cached_local_variables(); }

    std::uint64_t DebuggerViewModel::get_current_address() const { return m_model->get_current_address(); }

    std::uint32_t DebuggerViewModel::get_current_thread_id() const { return m_model->get_current_thread_id(); }

    const Debugger::DisassemblyRange& DebuggerViewModel::get_disassembly() const { return m_model->get_cached_disassembly(); }

    const Debugger::RegisterSet& DebuggerViewModel::get_registers() const { return m_model->get_cached_registers(); }

    const Debugger::CallStack& DebuggerViewModel::get_call_stack() const { return m_model->get_cached_call_stack(); }

    const std::vector<Debugger::Breakpoint>& DebuggerViewModel::get_breakpoints() const { return m_model->get_cached_breakpoints(); }

    const std::vector<Debugger::ModuleInfo>& DebuggerViewModel::get_modules() const { return m_model->get_cached_modules(); }

    const std::vector<Debugger::ThreadInfo>& DebuggerViewModel::get_threads() const { return m_model->get_cached_threads(); }

    const std::vector<Debugger::LogEntry>& DebuggerViewModel::get_logs() const { return m_model->get_cached_logs(); }

    const Debugger::MemoryBlock& DebuggerViewModel::get_cached_memory() const { return m_model->get_cached_memory(); }

    bool DebuggerViewModel::has_breakpoint_at(const std::uint64_t address) const { return m_model->has_breakpoint_at(address); }

    bool DebuggerViewModel::has_exception() const { return get_state() == Debugger::DebuggerState::Exception; }

    const Debugger::ExceptionData& DebuggerViewModel::get_exception_info() const { return m_model->get_cached_exception(); }

    void DebuggerViewModel::select_stack_frame(const std::uint32_t frameIndex)
    {
        m_selectedStackFrame = frameIndex;

        const auto& callStack = get_call_stack();
        if (frameIndex < callStack.frames.size())
        {
            navigate_to_address(callStack.frames[frameIndex].returnAddress);
        }
    }

    std::uint32_t DebuggerViewModel::get_selected_frame_index() const { return m_selectedStackFrame; }

    void DebuggerViewModel::select_module(const std::string_view moduleName)
    {
        m_selectedModule = moduleName;

        const auto& modules = get_modules();
        const auto it = std::ranges::find_if(modules,
                                             [&moduleName](const Debugger::ModuleInfo& m)
                                             {
                                                 return m.name == moduleName;
                                             });

        if (it != modules.end()) [[likely]]
        {
            navigate_to_address(it->baseAddress);
        }
    }

    std::string DebuggerViewModel::get_selected_module() const { return m_selectedModule; }

    void DebuggerViewModel::request_module_imports_exports(const std::string_view moduleName) const { m_model->request_module_imports_exports(moduleName); }

    const std::vector<Debugger::ImportEntry>& DebuggerViewModel::get_imports() const { return m_model->get_cached_imports(); }

    const std::vector<Debugger::ExportEntry>& DebuggerViewModel::get_exports() const { return m_model->get_cached_exports(); }

    std::vector<Runtime::RegisterCategoryInfo> DebuggerViewModel::get_register_categories() const { return m_model->get_register_categories(); }

    std::vector<Runtime::RegisterInfo> DebuggerViewModel::get_register_definitions() const { return m_model->get_register_definitions(); }

    std::vector<Runtime::RegisterInfo> DebuggerViewModel::get_registers_by_category(const std::string_view categoryId) const { return m_model->get_registers_by_category(categoryId); }

    std::vector<Runtime::FlagBitInfo> DebuggerViewModel::get_flag_bits(const std::string_view flagsRegisterName) const { return m_model->get_flag_bits(flagsRegisterName); }

    std::optional<Runtime::ArchInfo> DebuggerViewModel::get_architecture_info() const { return m_model->get_architecture_info(); }

    bool DebuggerViewModel::has_registry_data() const { return m_model->has_registry_data(); }

    Theme DebuggerViewModel::get_theme() const { return m_model->get_theme(); }

    std::string DebuggerViewModel::get_aui_perspective() const { return m_model->get_ui_state_string("uiState.debuggerView.auiPerspective", EMPTY_STRING); }

    void DebuggerViewModel::set_aui_perspective(const std::string_view perspective) const { m_model->set_ui_state_string("uiState.debuggerView.auiPerspective", perspective); }

} // namespace Vertex::ViewModel
