//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/utility.hh>
#include <vertex/model/debuggermodel.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>
#include <sdk/disassembler.h>

#include <algorithm>
#include <vector>
#include <ranges>

namespace Vertex::Model
{
    DebuggerModel::DebuggerModel(Configuration::ISettings& settingsService,
                                   Runtime::ILoader& loaderService,
                                   Log::ILog& loggerService,
                                   Thread::IThreadDispatcher& dispatcher)
        : m_settingsService{settingsService},
          m_loaderService{loaderService},
          m_loggerService{loggerService},
          m_worker(std::make_unique<Debugger::DebuggerWorker>(loaderService, dispatcher))
    {
        m_worker->set_event_callback([this](const Debugger::DebuggerEvent& evt)
        {
            on_worker_event(evt);
        });
    }

    DebuggerModel::~DebuggerModel()
    {
        std::ignore = stop_worker();
    }

    StatusCode DebuggerModel::start_worker() const
    {
        m_loggerService.log_info(fmt::format("{}: Starting debugger worker thread", MODEL_NAME));
        return m_worker->start();
    }

    StatusCode DebuggerModel::stop_worker() const
    {
        m_loggerService.log_info(fmt::format("{}: Stopping debugger worker thread", MODEL_NAME));
        return m_worker->stop();
    }

    void DebuggerModel::set_event_handler(DebuggerEventHandler handler)
    {
        m_eventHandler = std::move(handler);
    }

    void DebuggerModel::on_worker_event(const Debugger::DebuggerEvent& evt)
    {
        std::visit([this]<class T0>(T0&& arg)
        {
            using T = std::decay_t<T0>;

            if constexpr (std::is_same_v<T, Debugger::EvtStateChanged>)
            {
                m_cachedSnapshot.state = arg.snapshot.state;
                m_cachedSnapshot.currentAddress = arg.snapshot.currentAddress;
                m_cachedSnapshot.currentThreadId = arg.snapshot.currentThreadId;

                m_loggerService.log_info(fmt::format("{}: State changed to {}",
                    MODEL_NAME, static_cast<int>(m_cachedSnapshot.state)));
            }
            else if constexpr (std::is_same_v<T, Debugger::EvtError>)
            {
                m_loggerService.log_error(fmt::format("{}: Error: {}", MODEL_NAME, arg.message));
            }
            else if constexpr (std::is_same_v<T, Debugger::EvtLog>)
            {
                m_loggerService.log_info(fmt::format("{}: {}", MODEL_NAME, arg.message));
            }
            else if constexpr (std::is_same_v<T, Debugger::EvtBreakpointHit>)
            {
                m_cachedSnapshot.currentThreadId = arg.threadId;
                m_cachedSnapshot.currentAddress = arg.address;

                if (const auto it = std::ranges::find_if(m_cachedBreakpoints,
                    [id = arg.breakpointId](const auto& bp) { return bp.id == id; });
                    it != m_cachedBreakpoints.end())
                {
                    ++it->hitCount;
                }

                m_loggerService.log_info(fmt::format("{}: Breakpoint {} hit at 0x{:X}",
                    MODEL_NAME, arg.breakpointId, arg.address));
            }
            else if constexpr (std::is_same_v<T, Debugger::EvtWatchpointHit>)
            {
                m_cachedSnapshot.currentThreadId = arg.threadId;
                m_cachedSnapshot.currentAddress = arg.accessorAddress;
                on_watchpoint_hit(arg.watchpointId, arg.accessorAddress);
            }
        }, evt);

        if (m_eventHandler)
        {
            m_eventHandler(evt);
        }
    }

    void DebuggerModel::attach_debugger() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting attach", MODEL_NAME));
        m_worker->send_command(Debugger::CmdAttach{});
    }

    void DebuggerModel::detach_debugger() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting detach", MODEL_NAME));
        m_worker->send_command(Debugger::CmdDetach{});
    }

    void DebuggerModel::continue_execution() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting continue execution", MODEL_NAME));
        m_worker->send_command(Debugger::CmdContinue{});
    }

    void DebuggerModel::pause_execution() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting pause execution", MODEL_NAME));
        m_worker->send_command(Debugger::CmdPause{});
    }

    void DebuggerModel::step_into() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step into", MODEL_NAME));
        m_worker->send_command(Debugger::CmdStepInto{});
    }

    void DebuggerModel::step_over() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step over", MODEL_NAME));
        m_worker->send_command(Debugger::CmdStepOver{});
    }

    void DebuggerModel::step_out() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step out", MODEL_NAME));
        m_worker->send_command(Debugger::CmdStepOut{});
    }

    void DebuggerModel::run_to_address(const std::uint64_t address) const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting run to address 0x{:X}", MODEL_NAME, address));
        m_worker->send_command(Debugger::CmdRunToAddress{address});
    }

    void DebuggerModel::navigate_to_address(std::uint64_t address)
    {
        m_loggerService.log_info(fmt::format("{}: Navigate to 0x{:X}", MODEL_NAME, address));
        m_cachedSnapshot.currentAddress = address;
    }

    void DebuggerModel::refresh_data() const
    {
        m_loggerService.log_info(fmt::format("{}: Refresh data requested", MODEL_NAME));
    }

    void DebuggerModel::add_breakpoint(std::uint64_t address, const Debugger::BreakpointType type)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint", MODEL_NAME));
            return;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return;
        }

        auto& plugin = pluginOpt.value().get();

        std::uint32_t breakpointId{};
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_set_breakpoint, address, VERTEX_BP_EXECUTE, &breakpointId);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to set breakpoint at 0x{:X}: {}", MODEL_NAME, address, static_cast<int>(status)));
            return;
        }

        m_loggerService.log_info(fmt::format("{}: Breakpoint set at 0x{:X} (ID: {})", MODEL_NAME, address, breakpointId));

        Debugger::Breakpoint bp{};
        bp.id = breakpointId;
        bp.address = address;
        bp.type = type;
        bp.state = Debugger::BreakpointState::Enabled;
        m_cachedBreakpoints.push_back(bp);
    }

    void DebuggerModel::remove_breakpoint(std::uint32_t breakpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint removal", MODEL_NAME));
            return;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return;
        }

        auto& plugin = pluginOpt.value().get();
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_remove_breakpoint, breakpointId);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to remove breakpoint {}: {}", MODEL_NAME, breakpointId, static_cast<int>(status)));
            return;
        }

        std::erase_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
        {
            return bp.id == breakpointId;
        });
        m_loggerService.log_info(fmt::format("{}: Breakpoint {} removed", MODEL_NAME, breakpointId));
    }

    void DebuggerModel::remove_breakpoint_at(std::uint64_t address)
    {
        const auto it = std::ranges::find_if(m_cachedBreakpoints, [address](const Debugger::Breakpoint& bp)
        {
            return bp.address == address;
        });

        if (it != m_cachedBreakpoints.end())
        {
            remove_breakpoint(it->id);
        }
        else
        {
            m_loggerService.log_warn(fmt::format("{}: No breakpoint found at address 0x{:X}", MODEL_NAME, address));
        }
    }

    void DebuggerModel::toggle_breakpoint(const std::uint64_t address)
    {
        const auto it = std::ranges::find_if(m_cachedBreakpoints, [address](const Debugger::Breakpoint& bp)
        {
            return bp.address == address;
        });

        if (it != m_cachedBreakpoints.end())
        {
            remove_breakpoint(it->id);
        }
        else
        {
            add_breakpoint(address, Debugger::BreakpointType::Software);
        }
    }

    void DebuggerModel::enable_breakpoint(std::uint32_t breakpointId, bool enable)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint enable/disable", MODEL_NAME));
            return;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return;
        }

        auto& plugin = pluginOpt.value().get();
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_enable_breakpoint, breakpointId, enable ? 1 : 0);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to {} breakpoint {}: {}", MODEL_NAME, enable ? "enable" : "disable", breakpointId, static_cast<int>(status)));
            return;
        }

        const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
        {
            return bp.id == breakpointId;
        });
        if (it != m_cachedBreakpoints.end())
        {
            it->state = enable ? Debugger::BreakpointState::Enabled : Debugger::BreakpointState::Disabled;
        }
        m_loggerService.log_info(fmt::format("{}: Breakpoint {} {}", MODEL_NAME, breakpointId, enable ? "enabled" : "disabled"));
    }

    StatusCode DebuggerModel::set_watchpoint(std::uint64_t address, std::uint32_t size, std::uint32_t* outWatchpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for watchpoint", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto& plugin = pluginOpt.value().get();
        Watchpoint wp{};
        wp.type = VERTEX_WP_READWRITE;
        wp.address = address;
        wp.size = size;
        wp.active = true;

        std::uint32_t watchpointId{};
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_set_watchpoint, &wp, &watchpointId);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to set watchpoint at 0x{:X}: {}", MODEL_NAME, address, static_cast<int>(status)));
            return status;
        }

        m_loggerService.log_info(fmt::format("{}: Watchpoint set at 0x{:X} (size: {}, ID: {})", MODEL_NAME, address, size, watchpointId));

        Debugger::Watchpoint cachedWp{};
        cachedWp.id = watchpointId;
        cachedWp.address = address;
        cachedWp.size = size;
        cachedWp.type = Debugger::WatchpointType::ReadWrite;
        cachedWp.enabled = true;
        m_cachedWatchpoints.push_back(cachedWp);

        if (outWatchpointId != nullptr)
        {
            *outWatchpointId = watchpointId;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::remove_watchpoint(std::uint32_t watchpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for watchpoint removal", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_remove_watchpoint, watchpointId);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to remove watchpoint {}: {}", MODEL_NAME, watchpointId, static_cast<int>(status)));
            return status;
        }

        std::erase_if(m_cachedWatchpoints, [watchpointId](const Debugger::Watchpoint& wp)
        {
            return wp.id == watchpointId;
        });
        m_loggerService.log_info(fmt::format("{}: Watchpoint {} removed", MODEL_NAME, watchpointId));

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::enable_watchpoint(std::uint32_t watchpointId, bool enable)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for watchpoint enable/disable", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();
        const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_enable_watchpoint, watchpointId, enable ? 1 : 0);
        const auto status = Runtime::get_status(result);

        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to {} watchpoint {}: {}", MODEL_NAME, enable ? "enable" : "disable", watchpointId, static_cast<int>(status)));
            return status;
        }

        const auto it = std::ranges::find_if(m_cachedWatchpoints, [watchpointId](const Debugger::Watchpoint& wp)
        {
            return wp.id == watchpointId;
        });
        if (it != m_cachedWatchpoints.end())
        {
            it->enabled = enable;
        }
        m_loggerService.log_info(fmt::format("{}: Watchpoint {} {}", MODEL_NAME, watchpointId, enable ? "enabled" : "disabled"));

        return StatusCode::STATUS_OK;
    }

    void DebuggerModel::on_watchpoint_hit(const std::uint32_t watchpointId, const std::uint64_t accessorAddress)
    {
        if (const auto it = std::ranges::find_if(m_cachedWatchpoints,
            [watchpointId](const auto& wp) { return wp.id == watchpointId; });
            it != m_cachedWatchpoints.end())
        {
            ++it->hitCount;
            it->lastAccessorAddress = accessorAddress;
            m_loggerService.log_info(fmt::format("{}: Watchpoint {} hit (count: {}, accessor: 0x{:X})",
                MODEL_NAME, watchpointId, it->hitCount, accessorAddress));
        }
        else
        {
            m_loggerService.log_warn(fmt::format("{}: Watchpoint hit for unknown ID {}", MODEL_NAME, watchpointId));
        }
    }

    const std::vector<Debugger::Watchpoint>& DebuggerModel::get_cached_watchpoints() const
    {
        return m_cachedWatchpoints;
    }

    bool DebuggerModel::is_attached() const
    {
        return m_cachedSnapshot.state != Debugger::DebuggerState::Detached;
    }

    Debugger::DebuggerState DebuggerModel::get_debugger_state() const
    {
        return m_cachedSnapshot.state;
    }

    std::uint64_t DebuggerModel::get_current_address() const
    {
        return m_cachedSnapshot.currentAddress;
    }

    std::uint32_t DebuggerModel::get_current_thread_id() const
    {
        return m_cachedSnapshot.currentThreadId;
    }

    const Debugger::RegisterSet& DebuggerModel::get_cached_registers() const
    {
        return m_cachedRegisters;
    }

    const Debugger::DisassemblyRange& DebuggerModel::get_cached_disassembly() const
    {
        return m_cachedDisassembly;
    }

    const Debugger::CallStack& DebuggerModel::get_cached_call_stack() const
    {
        return m_cachedCallStack;
    }

    const std::vector<Debugger::Breakpoint>& DebuggerModel::get_cached_breakpoints() const
    {
        return m_cachedBreakpoints;
    }

    const std::vector<Debugger::ModuleInfo>& DebuggerModel::get_cached_modules() const
    {
        return m_cachedModules;
    }

    const std::vector<Debugger::ThreadInfo>& DebuggerModel::get_cached_threads() const
    {
        return m_cachedThreads;
    }

    bool DebuggerModel::has_breakpoint_at(std::uint64_t address) const
    {
        return std::ranges::any_of(m_cachedBreakpoints, [address](const Debugger::Breakpoint& bp)
        {
            return bp.address == address;
        });
    }

    std::vector<Runtime::RegisterCategoryInfo> DebuggerModel::get_register_categories() const
    {
        return m_loaderService.get_registry().get_categories();
    }

    std::vector<Runtime::RegisterInfo> DebuggerModel::get_register_definitions() const
    {
        return m_loaderService.get_registry().get_registers();
    }

    std::vector<Runtime::RegisterInfo> DebuggerModel::get_registers_by_category(const std::string_view categoryId) const
    {
        return m_loaderService.get_registry().get_registers_by_category(categoryId);
    }

    std::vector<Runtime::FlagBitInfo> DebuggerModel::get_flag_bits(const std::string_view flagsRegisterName) const
    {
        return m_loaderService.get_registry().get_flag_bits(flagsRegisterName);
    }

    std::optional<Runtime::ArchInfo> DebuggerModel::get_architecture_info() const
    {
        return m_loaderService.get_registry().get_architecture();
    }

    bool DebuggerModel::has_registry_data() const
    {
        return !m_loaderService.get_registry().get_registers().empty();
    }

    Theme DebuggerModel::get_theme() const
    {
        return static_cast<Theme>(m_settingsService.get_int("general.theme"));
    }

    StatusCode DebuggerModel::load_modules()
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();

        std::uint32_t count{};
        const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, nullptr, &count);
        const auto countStatus = Runtime::get_status(countResult);
        if (countStatus == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(countResult))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to get modules count", MODEL_NAME));
            return countStatus;
        }

        if (count == 0)
        {
            m_cachedModules.clear();
            return StatusCode::STATUS_OK;
        }

        std::vector<::ModuleInformation> modules(count);
        auto* modulesPtr = modules.data();
        const auto result = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, &modulesPtr, &count);
        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Failed to get modules list", MODEL_NAME));
            return Runtime::get_status(result);
        }

        m_cachedModules = modules
            | std::views::take(count)
            | std::views::transform([](const auto& m) {
                return Debugger::ModuleInfo{
                    .name = m.moduleName,
                    .path = m.modulePath,
                    .baseAddress = m.baseAddress,
                    .size = m.size
                };
            })
            | std::ranges::to<std::vector>();

        m_loggerService.log_info(fmt::format("{}: Loaded {} modules", MODEL_NAME, count));
        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::disassemble_at_address(std::uint64_t address)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();
        m_loggerService.log_info(fmt::format("{}: Disassembling at 0x{:X}", MODEL_NAME, address));

        constexpr std::size_t MAX_INSTRUCTIONS = 500;
        constexpr std::size_t DISASM_BYTES = 4096;

        std::vector<::DisassemblerResult> resultBuffer(MAX_INSTRUCTIONS);
        ::DisassemblerResults results{};
        results.results = resultBuffer.data();
        results.count = 0;
        results.capacity = static_cast<std::uint32_t>(MAX_INSTRUCTIONS);
        results.startAddress = address;

        const auto result = Runtime::safe_call(
            plugin.internal_vertex_process_disassemble_range,
            address,
            static_cast<std::uint32_t>(DISASM_BYTES),
            &results
        );
        const auto status = Runtime::get_status(result);

        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(result))
        {
            m_loggerService.log_error(fmt::format("{}: Disassembly failed with status {}",
                MODEL_NAME, static_cast<int>(status)));
            return status;
        }

        if (results.count == 0)
        {
            m_loggerService.log_warn(fmt::format("{}: Disassembly returned 0 instructions", MODEL_NAME));
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        m_cachedDisassembly.lines.clear();
        m_cachedDisassembly.startAddress = address;
        m_cachedSnapshot.currentAddress = address;

        for (const auto& [i, instr] : std::span{results.results, results.count} | std::views::enumerate)
        {
            Debugger::DisassemblyLine line{};
            line.address = instr.address;
            line.mnemonic = instr.mnemonic;
            line.operands = instr.operands;
            line.isCurrentInstruction = (i == 0);

            std::ranges::copy(
                std::span{instr.rawBytes, std::min<std::size_t>(instr.size, VERTEX_MAX_BYTES_LENGTH)},
                std::back_inserter(line.bytes));

            switch (instr.branchType)
            {
                case VERTEX_BRANCH_UNCONDITIONAL:
                case VERTEX_BRANCH_INDIRECT_JUMP:
                    line.branchType = Debugger::BranchType::UnconditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CONDITIONAL:
                    line.branchType = Debugger::BranchType::ConditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CALL:
                case VERTEX_BRANCH_INDIRECT_CALL:
                    line.branchType = Debugger::BranchType::Call;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_RETURN:
                    line.branchType = Debugger::BranchType::Return;
                    break;
                case VERTEX_BRANCH_LOOP:
                    line.branchType = Debugger::BranchType::Loop;
                    line.branchTarget = instr.targetAddress;
                    break;
                default:
                    line.branchType = Debugger::BranchType::None;
                    break;
            }

            m_cachedDisassembly.lines.push_back(std::move(line));
            m_cachedDisassembly.endAddress = instr.address + instr.size;
        }

        m_loggerService.log_info(fmt::format("{}: Disassembled {} instructions", MODEL_NAME, results.count));
        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::disassemble_extend_up(std::uint64_t fromAddress, std::size_t byteCount)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();
        const std::uint64_t startAddress = (fromAddress > byteCount) ? (fromAddress - byteCount) : 0;
        if (startAddress == 0 || startAddress >= fromAddress)
        {
            return StatusCode::STATUS_OK;
        }

        constexpr std::size_t MAX_INSTRUCTIONS = 200;
        std::vector<::DisassemblerResult> resultBuffer(MAX_INSTRUCTIONS);
        ::DisassemblerResults results{};
        results.results = resultBuffer.data();
        results.count = 0;
        results.capacity = static_cast<std::uint32_t>(MAX_INSTRUCTIONS);
        results.startAddress = startAddress;

        const auto result = Runtime::safe_call(
            plugin.internal_vertex_process_disassemble_range,
            startAddress,
            static_cast<std::uint32_t>(fromAddress - startAddress),
            &results
        );
        const auto status = Runtime::get_status(result);

        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(result) || results.count == 0)
        {
            return status;
        }

        std::vector<Debugger::DisassemblyLine> newLines;
        newLines.reserve(results.count);

        for (const auto& instr : std::span{results.results, results.count}
            | std::views::take_while([fromAddress](const auto& r) { return r.address < fromAddress; }))
        {
            Debugger::DisassemblyLine line{};
            line.address = instr.address;
            line.mnemonic = instr.mnemonic;
            line.operands = instr.operands;
            line.isCurrentInstruction = false;

            std::ranges::copy(
                std::span{instr.rawBytes, std::min<std::size_t>(instr.size, VERTEX_MAX_BYTES_LENGTH)},
                std::back_inserter(line.bytes));

            switch (instr.branchType)
            {
                case VERTEX_BRANCH_UNCONDITIONAL:
                case VERTEX_BRANCH_INDIRECT_JUMP:
                    line.branchType = Debugger::BranchType::UnconditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CONDITIONAL:
                    line.branchType = Debugger::BranchType::ConditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CALL:
                case VERTEX_BRANCH_INDIRECT_CALL:
                    line.branchType = Debugger::BranchType::Call;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_RETURN:
                    line.branchType = Debugger::BranchType::Return;
                    break;
                case VERTEX_BRANCH_LOOP:
                    line.branchType = Debugger::BranchType::Loop;
                    line.branchTarget = instr.targetAddress;
                    break;
                default:
                    line.branchType = Debugger::BranchType::None;
                    break;
            }

            newLines.push_back(std::move(line));
        }

        if (!newLines.empty())
        {
            m_cachedDisassembly.lines.insert(
                m_cachedDisassembly.lines.begin(),
                newLines.begin(),
                newLines.end()
            );
            m_cachedDisassembly.startAddress = newLines.front().address;

            if (m_cachedDisassembly.lines.size() > MAX_DISASSEMBLY_LINES)
            {
                const auto linesToRemove = std::min(
                    TRIM_LINES_COUNT,
                    m_cachedDisassembly.lines.size() - MAX_DISASSEMBLY_LINES
                );
                m_cachedDisassembly.lines.erase(
                    m_cachedDisassembly.lines.end() - static_cast<std::ptrdiff_t>(linesToRemove),
                    m_cachedDisassembly.lines.end()
                );
                if (!m_cachedDisassembly.lines.empty())
                {
                    const auto& lastLine = m_cachedDisassembly.lines.back();
                    m_cachedDisassembly.endAddress = lastLine.address + lastLine.bytes.size();
                }
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::disassemble_extend_down(std::uint64_t fromAddress, std::size_t byteCount)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();
        constexpr std::size_t MAX_INSTRUCTIONS = 200;

        std::vector<::DisassemblerResult> resultBuffer(MAX_INSTRUCTIONS);
        ::DisassemblerResults results{};
        results.results = resultBuffer.data();
        results.count = 0;
        results.capacity = static_cast<std::uint32_t>(MAX_INSTRUCTIONS);
        results.startAddress = fromAddress;

        const auto result = Runtime::safe_call(
            plugin.internal_vertex_process_disassemble_range,
            fromAddress,
            static_cast<std::uint32_t>(byteCount),
            &results
        );
        const auto status = Runtime::get_status(result);

        if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
        {
            return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
        }
        if (!Runtime::status_ok(result) || results.count == 0)
        {
            return status;
        }

        std::size_t addedCount{};
        for (const auto& instr : std::span{results.results, results.count}
            | std::views::filter([fromAddress](const auto& r) { return r.address >= fromAddress; }))
        {
            Debugger::DisassemblyLine line{};
            line.address = instr.address;
            line.mnemonic = instr.mnemonic;
            line.operands = instr.operands;
            line.isCurrentInstruction = false;

            std::ranges::copy(
                std::span{instr.rawBytes, std::min<std::size_t>(instr.size, VERTEX_MAX_BYTES_LENGTH)},
                std::back_inserter(line.bytes));

            switch (instr.branchType)
            {
                case VERTEX_BRANCH_UNCONDITIONAL:
                case VERTEX_BRANCH_INDIRECT_JUMP:
                    line.branchType = Debugger::BranchType::UnconditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CONDITIONAL:
                    line.branchType = Debugger::BranchType::ConditionalJump;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_CALL:
                case VERTEX_BRANCH_INDIRECT_CALL:
                    line.branchType = Debugger::BranchType::Call;
                    line.branchTarget = instr.targetAddress;
                    break;
                case VERTEX_BRANCH_RETURN:
                    line.branchType = Debugger::BranchType::Return;
                    break;
                case VERTEX_BRANCH_LOOP:
                    line.branchType = Debugger::BranchType::Loop;
                    line.branchTarget = instr.targetAddress;
                    break;
                default:
                    line.branchType = Debugger::BranchType::None;
                    break;
            }

            m_cachedDisassembly.lines.push_back(std::move(line));
            m_cachedDisassembly.endAddress = instr.address + instr.size;
            ++addedCount;
        }

        if (addedCount > 0 && m_cachedDisassembly.lines.size() > MAX_DISASSEMBLY_LINES)
        {
            const auto linesToRemove = std::min(
                TRIM_LINES_COUNT,
                m_cachedDisassembly.lines.size() - MAX_DISASSEMBLY_LINES
            );
            m_cachedDisassembly.lines.erase(
                m_cachedDisassembly.lines.begin(),
                m_cachedDisassembly.lines.begin() + static_cast<std::ptrdiff_t>(linesToRemove)
            );
            if (!m_cachedDisassembly.lines.empty())
            {
                m_cachedDisassembly.startAddress = m_cachedDisassembly.lines.front().address;
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::read_registers()
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::uint32_t threadId = m_cachedSnapshot.currentThreadId;
        auto& plugin = pluginOpt.value().get();

        if (threadId == 0)
        {
            const bool isDebuggerActive = m_cachedSnapshot.state != Debugger::DebuggerState::Detached;

            if (isDebuggerActive)
            {
                const auto currentThreadResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_current_thread, &threadId);
                if (!Runtime::status_ok(currentThreadResult))
                {
                    threadId = 0;
                }
            }

            if (threadId == 0)
            {
                ::ThreadList threadList{};
                const auto debuggerThreadsResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_threads, &threadList);
                if (Runtime::status_ok(debuggerThreadsResult))
                {
                    threadId = threadList.currentThreadId;
                }
            }

            if (threadId == 0)
            {
                ::ThreadList threadList{};
                const auto processThreadsResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_threads, &threadList);
                const auto status = Runtime::get_status(processThreadsResult);
                if (!Runtime::status_ok(processThreadsResult) || threadList.threadCount == 0)
                {
                    return Runtime::status_ok(processThreadsResult) ? StatusCode::STATUS_ERROR_GENERAL : status;
                }

                if (threadList.currentThreadId != 0)
                {
                    threadId = threadList.currentThreadId;
                }
                else
                {
                    threadId = threadList.threads[0].id;
                }
            }

            if (threadId == 0)
            {
                return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
            }
        }

        return read_registers_for_thread(threadId);
    }

    StatusCode DebuggerModel::read_registers_for_thread(std::uint32_t threadId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto& plugin = pluginOpt.value().get();

        ::RegisterSet sdkRegs{};
        StatusCode status{};

        const bool isPausedState =
            m_cachedSnapshot.state == Debugger::DebuggerState::Paused ||
            m_cachedSnapshot.state == Debugger::DebuggerState::BreakpointHit ||
            m_cachedSnapshot.state == Debugger::DebuggerState::Stepping ||
            m_cachedSnapshot.state == Debugger::DebuggerState::Exception;

        if (isPausedState)
        {
            const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
            status = Runtime::get_status(result);
        }
        else
        {
            const auto suspendResult = Runtime::safe_call(plugin.internal_vertex_debugger_suspend_thread, threadId);
            const auto suspendStatus = Runtime::get_status(suspendResult);
            if (suspendStatus != StatusCode::STATUS_OK)
            {
                m_loggerService.log_error(fmt::format("{}: Failed to suspend thread {} for register read: {}",
                    MODEL_NAME, threadId, static_cast<int>(suspendStatus)));
                return suspendStatus;
            }

            const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
            status = Runtime::get_status(result);

            const auto resumeResult = Runtime::safe_call(plugin.internal_vertex_debugger_resume_thread, threadId);
            const auto resumeStatus = Runtime::get_status(resumeResult);
            if (resumeStatus != StatusCode::STATUS_OK)
            {
                m_loggerService.log_warn(fmt::format("{}: Failed to resume thread {} after register read: {}",
                    MODEL_NAME, threadId, static_cast<int>(resumeStatus)));
            }
        }

        if (status != StatusCode::STATUS_OK)
        {
            m_loggerService.log_error(fmt::format("{}: Failed to read registers for thread {}: {}",
                MODEL_NAME, threadId, static_cast<int>(status)));
            return status;
        }

        m_cachedRegisters = Debugger::RegisterSet{};
        m_cachedRegisters.instructionPointer = sdkRegs.instructionPointer;
        m_cachedRegisters.stackPointer = sdkRegs.stackPointer;
        m_cachedRegisters.basePointer = sdkRegs.basePointer;
        m_cachedSnapshot.currentThreadId = threadId;

        if (sdkRegs.instructionPointer != 0)
        {
            m_cachedSnapshot.currentAddress = sdkRegs.instructionPointer;
        }

        for (const auto& [name, category, value, previousValue, bitWidth, modified]
            : std::span{sdkRegs.registers, std::min<std::uint32_t>(sdkRegs.registerCount, VERTEX_MAX_REGISTERS)})
        {
            Debugger::Register reg{};
            reg.name = name;
            reg.value = value;
            reg.previousValue = previousValue;
            reg.bitWidth = bitWidth;
            reg.modified = modified != 0;

            switch (category)
            {
                case VERTEX_REG_SEGMENT:
                    reg.category = Debugger::RegisterCategory::Segment;
                    m_cachedRegisters.segment.push_back(reg);
                    break;
                case VERTEX_REG_FLAGS:
                    reg.category = Debugger::RegisterCategory::Flags;
                    m_cachedRegisters.flags.push_back(reg);
                    break;
                case VERTEX_REG_FLOATING_POINT:
                    reg.category = Debugger::RegisterCategory::FloatingPoint;
                    m_cachedRegisters.floatingPoint.push_back(reg);
                    break;
                case VERTEX_REG_VECTOR:
                    reg.category = Debugger::RegisterCategory::Vector;
                    m_cachedRegisters.vector.push_back(reg);
                    break;
                case VERTEX_REG_GENERAL:
                default:
                    reg.category = Debugger::RegisterCategory::General;
                    m_cachedRegisters.generalPurpose.push_back(reg);
                    break;
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::load_threads()
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto& plugin = pluginOpt.value().get();

        ::ThreadList threadList{};
        const auto threadResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_threads, &threadList);
        const auto status = Runtime::get_status(threadResult);

        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        m_cachedThreads.clear();
        for (const auto& sdkThread
            : std::span{threadList.threads, std::min<std::uint32_t>(threadList.threadCount, VERTEX_MAX_THREADS)})
        {
            Debugger::ThreadInfo thread{};
            thread.id = sdkThread.id;
            thread.name = sdkThread.name;
            thread.instructionPointer = sdkThread.instructionPointer;
            thread.stackPointer = sdkThread.stackPointer;
            thread.entryPoint = sdkThread.entryPoint;
            thread.priority = sdkThread.priority;
            thread.isCurrent = (sdkThread.isCurrent != 0) || (sdkThread.id == threadList.currentThreadId);

            char* priorityStr = nullptr;
            const auto priorityResult = Runtime::safe_call(plugin.internal_vertex_debugger_thread_priority_value_to_string, sdkThread.priority, &priorityStr, nullptr);
            if (Runtime::status_ok(priorityResult) && priorityStr)
            {
                thread.priorityString = priorityStr;
            }

            switch (sdkThread.state)
            {
                case VERTEX_THREAD_RUNNING:
                    thread.state = Debugger::ThreadState::Running;
                    break;
                case VERTEX_THREAD_SUSPENDED:
                    thread.state = Debugger::ThreadState::Suspended;
                    break;
                case VERTEX_THREAD_WAITING:
                    thread.state = Debugger::ThreadState::Waiting;
                    break;
                case VERTEX_THREAD_TERMINATED:
                    thread.state = Debugger::ThreadState::Terminated;
                    break;
                default:
                    thread.state = Debugger::ThreadState::Running;
                    break;
            }

            m_cachedThreads.push_back(std::move(thread));
        }

        if (threadList.currentThreadId != 0 && m_cachedSnapshot.currentThreadId == 0)
        {
            m_cachedSnapshot.currentThreadId = threadList.currentThreadId;
        }

        return StatusCode::STATUS_OK;
    }

    void DebuggerModel::clear_cached_data()
    {
        m_cachedSnapshot = {};
        m_cachedRegisters = {};
        m_cachedDisassembly = {};
        m_cachedCallStack = {};
        m_cachedBreakpoints.clear();
        m_cachedModules.clear();
        m_cachedThreads.clear();
        m_cachedImports.clear();
        m_cachedExports.clear();
        m_cachedWatchpoints.clear();
    }

    StatusCode DebuggerModel::load_module_imports_exports(const std::string_view moduleName)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto pluginOpt = m_loaderService.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto& plugin = pluginOpt.value().get();

        const auto it = std::ranges::find_if(m_cachedModules, [moduleName](const auto& mod) {
            return mod.name == moduleName;
        });

        if (it == m_cachedModules.end())
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        const auto& targetModule = *it;

        m_cachedImports.clear();
        m_cachedExports.clear();

        ModuleInformation moduleInfo{};
        const auto nameLen = std::min(targetModule.name.size(), static_cast<std::size_t>(VERTEX_MAX_NAME_LENGTH - 1));
        std::copy_n(targetModule.name.data(), nameLen, moduleInfo.moduleName);
        moduleInfo.moduleName[nameLen] = '\0';

        const auto pathLen = std::min(targetModule.path.size(), static_cast<std::size_t>(VERTEX_MAX_PATH_LENGTH - 1));
        std::copy_n(targetModule.path.data(), pathLen, moduleInfo.modulePath);
        moduleInfo.modulePath[pathLen] = '\0';

        moduleInfo.baseAddress = targetModule.baseAddress;
        moduleInfo.size = targetModule.size;

        ModuleImport* imports = nullptr;
        std::uint32_t importCount = 0;

        const auto importsResult = Runtime::safe_call(plugin.internal_vertex_process_get_module_imports, &moduleInfo, &imports, &importCount);
        if (Runtime::status_ok(importsResult) && imports && importCount > 0)
        {
            m_cachedImports = std::span{imports, importCount}
                | std::views::transform([](const auto& imp) {
                    return Debugger::ImportEntry{
                        .moduleName = imp.libraryName ? imp.libraryName : EMPTY_STRING,
                        .functionName = imp.entry.name ? imp.entry.name : fmt::format("Ordinal #{}", imp.entry.ordinal),
                        .address = reinterpret_cast<std::uint64_t>(imp.importAddress),
                        .hint = static_cast<std::uint64_t>(imp.hint),
                        .bound = false
                    };
                })
                | std::ranges::to<std::vector>();
        }

        ModuleExport* exports = nullptr;
        std::uint32_t exportCount = 0;

        const auto exportsResult = Runtime::safe_call(plugin.internal_vertex_process_get_module_exports, &moduleInfo, &exports, &exportCount);
        if (Runtime::status_ok(exportsResult) && exports && exportCount > 0)
        {
            m_cachedExports = std::span{exports, exportCount}
                | std::views::transform([](const auto& exp) {
                    return Debugger::ExportEntry{
                        .functionName = exp.entry.name ? exp.entry.name : fmt::format("Ordinal #{}", exp.entry.ordinal),
                        .address = reinterpret_cast<std::uint64_t>(exp.entry.address),
                        .ordinal = static_cast<std::uint32_t>(exp.entry.ordinal),
                        .forwarded = exp.entry.isForwarder != 0,
                        .forwardTarget = exp.entry.forwarderName ? exp.entry.forwarderName : EMPTY_STRING
                    };
                })
                | std::ranges::to<std::vector>();
        }

        return StatusCode::STATUS_OK;
    }

    const std::vector<Debugger::ImportEntry>& DebuggerModel::get_cached_imports() const
    {
        return m_cachedImports;
    }

    const std::vector<Debugger::ExportEntry>& DebuggerModel::get_cached_exports() const
    {
        return m_cachedExports;
    }

    bool DebuggerModel::get_ui_state_bool(const std::string_view key, const bool defaultValue) const
    {
        const std::string keyStr{key};
        if (!m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            return defaultValue;
        }
        return m_settingsService.get_bool(keyStr, defaultValue);
    }

    void DebuggerModel::set_ui_state_bool(const std::string_view key, const bool value) const
    {
        const std::string keyStr{key};
        if (m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            m_settingsService.set_value(keyStr, value);
        }
    }

    std::string DebuggerModel::get_ui_state_string(const std::string_view key, const std::string_view defaultValue) const
    {
        const std::string keyStr{key};
        const std::string defaultStr{defaultValue};
        if (!m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            return defaultStr;
        }
        return m_settingsService.get_string(keyStr, defaultStr);
    }

    void DebuggerModel::set_ui_state_string(const std::string_view key, const std::string_view value) const
    {
        const std::string keyStr{key};
        if (m_settingsService.get_bool("general.guiSavingEnabled", true))
        {
            m_settingsService.set_value(keyStr, std::string{value});
        }
    }

}
