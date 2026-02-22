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
          [this](const Debugger::DebuggerEvent& evt)
          {
              on_debugger_event(evt);
          });
    }

    DebuggerViewModel::~DebuggerViewModel()
    {
        stop_worker();
        unsubscribe_from_events();
    }

    void DebuggerViewModel::start_worker() const
    {
        if (const auto status = m_model->start_worker(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to start worker (status={})", static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::stop_worker() const
    {
        if (const auto status = m_model->stop_worker(); status != StatusCode::STATUS_OK) [[unlikely]]
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to stop worker (status={})", static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::on_debugger_event(const Debugger::DebuggerEvent& evt) const
    {
        std::visit(
          [this]<class T0>([[maybe_unused]] T0&& arg)
          {
              using T = std::decay_t<T0>;

              if constexpr (std::is_same_v<T, Debugger::EvtStateChanged>)
              {
                  notify_view_update(ViewUpdateFlags::DEBUGGER_ALL);
              }
              else if constexpr (std::is_same_v<T, Debugger::EvtAttachFailed>)
              {
                  notify_view_update(ViewUpdateFlags::DEBUGGER_STATE);
              }
              else if constexpr (std::is_same_v<T, Debugger::EvtError>)
              {
                  notify_view_update(ViewUpdateFlags::DEBUGGER_STATE);
              }
              else if constexpr (std::is_same_v<T, Debugger::EvtWatchpointHit>)
              {
                   notify_view_update(ViewUpdateFlags::DEBUGGER_WATCHPOINTS);
              }
              else if constexpr (std::is_same_v<T, Debugger::EvtLog>)
              {
              }
          },
          evt);
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
                                                   stop_worker();
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

        start_worker();

        if (const auto status = load_modules_and_disassemble(); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to load modules and disassemble on process open (status={})", static_cast<int>(status)));
        }

        if (const auto status = read_registers(); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to read registers on process open (status={})", static_cast<int>(status)));
        }

        if (const auto status = load_threads(); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to load threads on process open (status={})", static_cast<int>(status)));
        }
    }

    void DebuggerViewModel::on_process_closed()
    {
        detach_debugger();
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

    StatusCode DebuggerViewModel::disassemble_at_address(const std::uint64_t address) const
    {
        const StatusCode status = m_model->disassemble_at_address(address);
        if (status == StatusCode::STATUS_OK) [[likely]]
        {
            notify_view_update(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        }
        return status;
    }

    StatusCode DebuggerViewModel::disassemble_extend_up(const std::uint64_t fromAddress) const
    {
        const StatusCode status = m_model->disassemble_extend_up(fromAddress);
        notify_view_update(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        return status;
    }

    StatusCode DebuggerViewModel::disassemble_extend_down(const std::uint64_t fromAddress) const
    {
        const StatusCode status = m_model->disassemble_extend_down(fromAddress);
        notify_view_update(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        return status;
    }

    StatusCode DebuggerViewModel::load_modules_and_disassemble() const
    {
        StatusCode status = m_model->load_modules();
        if (status != StatusCode::STATUS_OK) [[unlikely]]
        {
            return status;
        }

        notify_view_update(ViewUpdateFlags::DEBUGGER_IMPORTS_EXPORTS);

        const auto& modules = m_model->get_cached_modules();
        if (modules.empty()) [[unlikely]]
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        const std::uint64_t entryPoint = modules[0].baseAddress;

        status = m_model->disassemble_at_address(entryPoint);
        if (status == StatusCode::STATUS_OK) [[likely]]
        {
            notify_view_update(ViewUpdateFlags::DEBUGGER_DISASSEMBLY);
        }

        return status;
    }

    StatusCode DebuggerViewModel::read_registers() const
    {
        const StatusCode status = m_model->read_registers();
        if (status == StatusCode::STATUS_OK) [[likely]]
        {
            notify_view_update(ViewUpdateFlags::DEBUGGER_REGISTERS);
        }
        return status;
    }

    StatusCode DebuggerViewModel::load_threads() const
    {
        const StatusCode status = m_model->load_threads();
        if (status == StatusCode::STATUS_OK) [[likely]]
        {
            notify_view_update(ViewUpdateFlags::DEBUGGER_THREADS);
        }
        return status;
    }

    void DebuggerViewModel::ensure_data_loaded() const
    {
        if (get_modules().empty())
        {
            if (const auto status = load_modules_and_disassemble(); status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("DebuggerViewModel: failed to load modules and disassemble (status={})", static_cast<int>(status)));
            }
        }
        else if (get_disassembly().lines.empty() && !get_modules().empty())
        {
            const auto& modules = get_modules();
            if (const auto status = disassemble_at_address(modules[0].baseAddress); status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("DebuggerViewModel: failed to disassemble at base address (status={})", static_cast<int>(status)));
            }
        }

        if (get_registers().generalPurpose.empty())
        {
            if (const auto status = read_registers(); status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("DebuggerViewModel: failed to read registers (status={})", static_cast<int>(status)));
            }
        }

        if (get_threads().empty())
        {
            if (const auto status = load_threads(); status != StatusCode::STATUS_OK)
            {
                m_logService.log_error(fmt::format("DebuggerViewModel: failed to load threads (status={})", static_cast<int>(status)));
            }
        }
    }

    void DebuggerViewModel::clear_cached_data() const
    {
        m_model->clear_cached_data();
    }

    void DebuggerViewModel::toggle_breakpoint(const std::uint64_t address) const { m_model->toggle_breakpoint(address); }

    void DebuggerViewModel::add_breakpoint(const std::uint64_t address, const Debugger::BreakpointType type) const { m_model->add_breakpoint(address, type); }

    void DebuggerViewModel::remove_breakpoint(const std::uint32_t id) const { m_model->remove_breakpoint(id); }

    void DebuggerViewModel::remove_breakpoint_at(const std::uint64_t address) const { m_model->remove_breakpoint_at(address); }

    void DebuggerViewModel::enable_breakpoint(const std::uint32_t id, const bool enable) const { m_model->enable_breakpoint(id, enable); }

    void DebuggerViewModel::set_watchpoint(const std::uint64_t address, const std::uint32_t size) const
    {
        if (const auto status = m_model->set_watchpoint(address, size); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to set watchpoint at 0x{:X} (status={})", address, static_cast<int>(status)));
        }
        notify_view_update(ViewUpdateFlags::DEBUGGER_WATCHPOINTS);
    }

    void DebuggerViewModel::remove_watchpoint(const std::uint32_t id) const
    {
        if (const auto status = m_model->remove_watchpoint(id); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to remove watchpoint {} (status={})", id, static_cast<int>(status)));
        }
        notify_view_update(ViewUpdateFlags::DEBUGGER_WATCHPOINTS);
    }

    void DebuggerViewModel::enable_watchpoint(const std::uint32_t id, const bool enable) const
    {
        if (const auto status = m_model->enable_watchpoint(id, enable); status != StatusCode::STATUS_OK)
        {
            m_logService.log_error(fmt::format("DebuggerViewModel: failed to {} watchpoint {} (status={})", enable ? "enable" : "disable", id, static_cast<int>(status)));
        }
        notify_view_update(ViewUpdateFlags::DEBUGGER_WATCHPOINTS);
    }

    const std::vector<Debugger::Watchpoint>& DebuggerViewModel::get_watchpoints() const { return m_model->get_cached_watchpoints(); }

    std::uint64_t DebuggerViewModel::get_current_address() const { return m_model->get_current_address(); }

    std::uint32_t DebuggerViewModel::get_current_thread_id() const { return m_model->get_current_thread_id(); }

    const Debugger::DisassemblyRange& DebuggerViewModel::get_disassembly() const { return m_model->get_cached_disassembly(); }

    const Debugger::RegisterSet& DebuggerViewModel::get_registers() const { return m_model->get_cached_registers(); }

    const Debugger::CallStack& DebuggerViewModel::get_call_stack() const { return m_model->get_cached_call_stack(); }

    const std::vector<Debugger::Breakpoint>& DebuggerViewModel::get_breakpoints() const { return m_model->get_cached_breakpoints(); }

    const std::vector<Debugger::ModuleInfo>& DebuggerViewModel::get_modules() const { return m_model->get_cached_modules(); }

    const std::vector<Debugger::ThreadInfo>& DebuggerViewModel::get_threads() const { return m_model->get_cached_threads(); }

    bool DebuggerViewModel::has_breakpoint_at(const std::uint64_t address) const { return m_model->has_breakpoint_at(address); }

    bool DebuggerViewModel::has_exception() const { return get_state() == Debugger::DebuggerState::Exception; }

    const Debugger::ExceptionData& DebuggerViewModel::get_exception_info() const
    {
        static Debugger::ExceptionData empty{};
        return empty;
    }

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
            if (const auto status = disassemble_at_address(it->baseAddress); status != StatusCode::STATUS_OK) [[unlikely]]
            {
                m_logService.log_error(fmt::format("DebuggerViewModel: failed to disassemble at module base 0x{:X} (status={})", it->baseAddress, static_cast<int>(status)));
            }
        }
    }

    std::string DebuggerViewModel::get_selected_module() const { return m_selectedModule; }

    StatusCode DebuggerViewModel::load_module_imports_exports(const std::string_view moduleName) const
    {
        return m_model->load_module_imports_exports(moduleName);
    }

    const std::vector<Debugger::ImportEntry>& DebuggerViewModel::get_imports() const
    {
        return m_model->get_cached_imports();
    }

    const std::vector<Debugger::ExportEntry>& DebuggerViewModel::get_exports() const
    {
        return m_model->get_cached_exports();
    }

    std::vector<Runtime::RegisterCategoryInfo> DebuggerViewModel::get_register_categories() const { return m_model->get_register_categories(); }

    std::vector<Runtime::RegisterInfo> DebuggerViewModel::get_register_definitions() const { return m_model->get_register_definitions(); }

    std::vector<Runtime::RegisterInfo> DebuggerViewModel::get_registers_by_category(const std::string_view categoryId) const { return m_model->get_registers_by_category(categoryId); }

    std::vector<Runtime::FlagBitInfo> DebuggerViewModel::get_flag_bits(const std::string_view flagsRegisterName) const { return m_model->get_flag_bits(flagsRegisterName); }

    std::optional<Runtime::ArchInfo> DebuggerViewModel::get_architecture_info() const { return m_model->get_architecture_info(); }

    bool DebuggerViewModel::has_registry_data() const { return m_model->has_registry_data(); }

    Theme DebuggerViewModel::get_theme() const { return m_model->get_theme(); }

    std::string DebuggerViewModel::get_aui_perspective() const
    {
        return m_model->get_ui_state_string("uiState.debuggerView.auiPerspective", EMPTY_STRING);
    }

    void DebuggerViewModel::set_aui_perspective(const std::string_view perspective) const
    {
        m_model->set_ui_state_string("uiState.debuggerView.auiPerspective", perspective);
    }

} // namespace Vertex::ViewModel
