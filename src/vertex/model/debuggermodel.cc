//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <fmt/format.h>
#include <vertex/utility.hh>
#include <vertex/model/debuggermodel.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>
#include <vertex/thread/threadchannel.hh>
#include <sdk/disassembler.h>

#include <wx/app.h>

#include <algorithm>
#include <span>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <ranges>

namespace
{
    constexpr std::uint32_t PENDING_BREAKPOINT_ID_BASE = 0xFFFF'0000u;
    std::atomic<std::uint32_t> g_pendingIdCounter{0};

    [[nodiscard]] bool is_pending_id(const std::uint32_t id)
    {
        return id >= PENDING_BREAKPOINT_ID_BASE;
    }
}

namespace Vertex::Model
{
    // NOTE:
    //
    // We use a lot of wx stuff to make dispatch to the UI thread easier,
    // but it's not the cleanest approach, we should probably integrate a global vertex function
    // in which CallAfter is hooked up to during initialization of Vertex and use that instead of depending on wx directly in the model.
    // This would also allow us to remove the dependency on wx from the model layer.
    // Might also be compatible with other ui frameworks like qt if we decide to switch in the future for whatever reason.

    DebuggerModel::DebuggerModel(Configuration::ISettings& settingsService,
                                   Runtime::ILoader& loaderService,
                                   Log::ILog& loggerService,
                                   Thread::IThreadDispatcher& dispatcher)
        : m_settingsService{settingsService},
          m_loaderService{loaderService},
          m_loggerService{loggerService},
          m_dispatcher{dispatcher},
          m_engine(std::make_unique<Debugger::DebuggerEngine>(loaderService, dispatcher))
    {
        m_engine->set_event_callback([this](Debugger::DirtyFlags flags, const Debugger::EngineSnapshot& snapshot)
        {
            on_engine_event(flags, snapshot);
        });
    }

    DebuggerModel::~DebuggerModel()
    {
        std::ignore = stop_engine();
    }

    StatusCode DebuggerModel::start_engine() const
    {
        m_loggerService.log_info(fmt::format("{}: Starting debugger engine", MODEL_NAME));
        return m_engine->start();
    }

    StatusCode DebuggerModel::stop_engine() const
    {
        m_loggerService.log_info(fmt::format("{}: Stopping debugger engine", MODEL_NAME));
        return m_engine->stop();
    }

    void DebuggerModel::set_event_handler(DebuggerEventHandler handler)
    {
        m_eventHandler = std::move(handler);
    }

    void DebuggerModel::set_extension_result_handler(ExtensionResultHandler handler)
    {
        m_extensionResultHandler = std::move(handler);
    }

    void DebuggerModel::on_engine_event(Debugger::DirtyFlags flags, const Debugger::EngineSnapshot& snapshot)
    {
        {
            std::scoped_lock lock{m_cacheMutex};
            m_cachedSnapshot = snapshot;
            if (snapshot.currentAddress != 0)
            {
                m_navigationAddress = 0;
            }

            if (snapshot.hasException)
            {
                m_cachedException.code = snapshot.exceptionCode;
                m_cachedException.address = snapshot.currentAddress;
                m_cachedException.threadId = snapshot.currentThreadId;
                m_cachedException.firstChance = snapshot.exceptionFirstChance;
                m_cachedException.continuable = true;
                m_cachedException.description = fmt::format("Exception 0x{:08X} at 0x{:016X}",
                    snapshot.exceptionCode, snapshot.currentAddress);
            }
            else if (snapshot.state != Debugger::EngineState::Paused)
            {
                m_cachedException = {};
            }
        }
        m_generation = snapshot.generation;

        if (snapshot.state == Debugger::EngineState::Exited)
        {
            clear_cached_data();
        }

        const auto previousState = m_lastEngineState;
        m_lastEngineState = snapshot.state;

        const auto enteredInspectable =
            (snapshot.state == Debugger::EngineState::Paused) &&
            (previousState != Debugger::EngineState::Paused);

        if (enteredInspectable)
        {
            flags = flags | Debugger::DirtyFlags::Registers
                          | Debugger::DirtyFlags::CallStack
                          | Debugger::DirtyFlags::Threads;
        }

        const auto isInspectable = snapshot.state == Debugger::EngineState::Paused;

        if (isInspectable && (flags & Debugger::DirtyFlags::Registers) != Debugger::DirtyFlags::None)
        {
            request_registers();
        }
        if (isInspectable && (flags & Debugger::DirtyFlags::Threads) != Debugger::DirtyFlags::None)
        {
            request_threads();
        }
        if (isInspectable && (flags & Debugger::DirtyFlags::CallStack) != Debugger::DirtyFlags::None)
        {
            request_call_stack();
        }
        if ((flags & Debugger::DirtyFlags::Modules) != Debugger::DirtyFlags::None)
        {
            request_modules();
        }
        if ((flags & Debugger::DirtyFlags::Breakpoints) != Debugger::DirtyFlags::None)
        {
            resync_breakpoints_from_plugin();
        }
        if ((flags & Debugger::DirtyFlags::Watchpoints) != Debugger::DirtyFlags::None)
        {
            resync_watchpoints_from_plugin();
        }

        if (snapshot.state == Debugger::EngineState::Paused && snapshot.currentAddress != 0)
        {
            request_disassembly(snapshot.currentAddress);
        }

        static constexpr auto ASYNC_QUERY_FLAGS =
            Debugger::DirtyFlags::Modules
            | Debugger::DirtyFlags::Breakpoints
            | Debugger::DirtyFlags::Watchpoints;

        const auto immediateFlags = static_cast<Debugger::DirtyFlags>(
            std::to_underlying(flags) & ~std::to_underlying(ASYNC_QUERY_FLAGS));

        if (m_eventHandler && immediateFlags != Debugger::DirtyFlags::None)
        {
            m_eventHandler(immediateFlags, snapshot);
        }
    }

    void DebuggerModel::attach_debugger() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting attach", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdAttach{});
    }

    void DebuggerModel::detach_debugger() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting detach", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdDetach{});
    }

    void DebuggerModel::continue_execution() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting continue execution", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdContinue{});
    }

    void DebuggerModel::pause_execution() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting pause execution", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdPause{});
    }

    void DebuggerModel::step_into() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step into", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdStepInto{});
    }

    void DebuggerModel::step_over() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step over", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdStepOver{});
    }

    void DebuggerModel::step_out() const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting step out", MODEL_NAME));
        m_engine->send_command(Debugger::engine::CmdStepOut{});
    }

    void DebuggerModel::run_to_address(const std::uint64_t address) const
    {
        m_loggerService.log_info(fmt::format("{}: Requesting run to address 0x{:X}", MODEL_NAME, address));
        m_engine->send_command(Debugger::engine::CmdRunToAddress{address});
    }

    void DebuggerModel::navigate_to_address(std::uint64_t address)
    {
        m_loggerService.log_info(fmt::format("{}: Navigate to 0x{:X}", MODEL_NAME, address));
        {
            std::scoped_lock lock{m_cacheMutex};
            m_navigationAddress = address;
        }
        request_disassembly(address);
    }

    void DebuggerModel::refresh_data()
    {
        m_loggerService.log_info(fmt::format("{}: Refresh data requested", MODEL_NAME));
        request_threads();
        request_registers();
        request_call_stack();
        request_modules();
        const auto snapshot = m_engine->get_snapshot();
        if (snapshot.currentAddress != 0)
        {
            request_disassembly(snapshot.currentAddress);
        }
    }

    void DebuggerModel::add_breakpoint(std::uint64_t address, const Debugger::BreakpointType type)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint", MODEL_NAME));
            return;
        }

        ::BreakpointType sdkType{};
        switch (type)
        {
            case Debugger::BreakpointType::Hardware: sdkType = VERTEX_BP_READWRITE; break;
            case Debugger::BreakpointType::Memory:   sdkType = VERTEX_BP_WRITE; break;
            case Debugger::BreakpointType::Software:
            default:                                  sdkType = VERTEX_BP_EXECUTE; break;
        }

        const auto pendingId = PENDING_BREAKPOINT_ID_BASE + g_pendingIdCounter.fetch_add(1);

        {
            Debugger::Breakpoint pendingBp{};
            pendingBp.id = pendingId;
            pendingBp.address = address;
            pendingBp.type = type;
            pendingBp.state = Debugger::BreakpointState::Pending;

            std::scoped_lock lock{m_cacheMutex};
            m_pendingBreakpointAdds[pendingId] = PendingBreakpointAdd{
                .address = address,
                .type = type
            };
            m_cachedBreakpoints.push_back(std::move(pendingBp));
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, address, type, sdkType, generation, pendingId]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this, pendingId, generation]()
                    {
                        if (m_engine->get_generation() != generation) { return; }
                        {
                            std::scoped_lock lock{m_cacheMutex};
                            m_pendingBreakpointAdds.erase(pendingId);
                            std::erase_if(m_cachedBreakpoints, [pendingId](const Debugger::Breakpoint& bp)
                            {
                                return bp.id == pendingId;
                            });
                        }
                        if (m_eventHandler)
                        {
                            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                std::uint32_t breakpointId{};
                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_set_breakpoint, address, sdkType, &breakpointId);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to set breakpoint at 0x{:X}: {}", MODEL_NAME, address, static_cast<int>(status)));
                    wxTheApp->CallAfter([this, pendingId, generation]()
                    {
                        if (m_engine->get_generation() != generation) { return; }
                        {
                            std::scoped_lock lock{m_cacheMutex};
                            m_pendingBreakpointAdds.erase(pendingId);
                            std::erase_if(m_cachedBreakpoints, [pendingId](const Debugger::Breakpoint& bp)
                            {
                                return bp.id == pendingId;
                            });
                        }
                        if (m_eventHandler)
                        {
                            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                        }
                    });
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Breakpoint set at 0x{:X} (ID: {})", MODEL_NAME, address, breakpointId));

                wxTheApp->CallAfter([this, pendingId, breakpointId, address, type, generation]()
                {
                    if (m_engine->get_generation() != generation) { return; }

                    bool pendingRequestTracked{};
                    bool pendingEntryFound{};
                    bool hasRealEntry{};
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        pendingRequestTracked = m_pendingBreakpointAdds.contains(pendingId);

                        auto it = std::ranges::find_if(m_cachedBreakpoints, [pendingId](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == pendingId;
                        });
                        pendingEntryFound = it != m_cachedBreakpoints.end();

                        hasRealEntry = std::ranges::any_of(m_cachedBreakpoints, [breakpointId, address](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == breakpointId || (!is_pending_id(bp.id) && bp.address == address);
                        });

                        if (pendingEntryFound)
                        {
                            if (hasRealEntry)
                            {
                                m_cachedBreakpoints.erase(it);
                            }
                            else
                            {
                                it->id = breakpointId;
                                it->address = address;
                                it->type = type;
                                it->state = Debugger::BreakpointState::Enabled;
                            }
                        }
                        else if (pendingRequestTracked && !hasRealEntry)
                        {
                            Debugger::Breakpoint establishedBreakpoint{};
                            establishedBreakpoint.id = breakpointId;
                            establishedBreakpoint.address = address;
                            establishedBreakpoint.type = type;
                            establishedBreakpoint.state = Debugger::BreakpointState::Enabled;
                            m_cachedBreakpoints.push_back(std::move(establishedBreakpoint));
                        }

                        m_pendingBreakpointAdds.erase(pendingId);
                    }

                    if (!pendingRequestTracked)
                    {
                        remove_breakpoint(breakpointId);
                        return;
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
    }

    void DebuggerModel::remove_breakpoint(std::uint32_t breakpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint removal", MODEL_NAME));
            return;
        }

        if (is_pending_id(breakpointId))
        {
            {
                std::scoped_lock lock{m_cacheMutex};
                m_pendingBreakpointAdds.erase(breakpointId);
                std::erase_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
                {
                    return bp.id == breakpointId;
                });
            }
            if (m_eventHandler)
            {
                m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
            }
            return;
        }

        {
            std::scoped_lock lock{m_cacheMutex};
            const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
            {
                return bp.id == breakpointId;
            });
            if (it != m_cachedBreakpoints.end())
            {
                it->state = Debugger::BreakpointState::Pending;
            }
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, breakpointId, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    resync_breakpoints_from_plugin();
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_remove_breakpoint, breakpointId);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to remove breakpoint {}: {}", MODEL_NAME, breakpointId, static_cast<int>(status)));
                    resync_breakpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Breakpoint {} removed", MODEL_NAME, breakpointId));

                wxTheApp->CallAfter([this, breakpointId, generation]()
                {
                    if (m_engine->get_generation() != generation) { return; }
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        std::erase_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == breakpointId;
                        });
                    }
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
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

        if (is_pending_id(breakpointId))
        {
            return;
        }

        {
            std::scoped_lock lock{m_cacheMutex};
            const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
            {
                return bp.id == breakpointId;
            });
            if (it != m_cachedBreakpoints.end())
            {
                it->state = Debugger::BreakpointState::Pending;
            }
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, breakpointId, enable, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    resync_breakpoints_from_plugin();
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_enable_breakpoint, breakpointId, enable ? 1 : 0);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to {} breakpoint {}: {}", MODEL_NAME, enable ? "enable" : "disable", breakpointId, static_cast<int>(status)));
                    resync_breakpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Breakpoint {} {}", MODEL_NAME, breakpointId, enable ? "enabled" : "disabled"));

                wxTheApp->CallAfter([this, breakpointId, enable, generation]()
                {
                    if (m_engine->get_generation() != generation) { return; }
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == breakpointId;
                        });
                        if (it != m_cachedBreakpoints.end())
                        {
                            it->state = enable ? Debugger::BreakpointState::Enabled : Debugger::BreakpointState::Disabled;
                        }
                    }
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
    }

    void DebuggerModel::set_breakpoint_condition(const std::uint32_t breakpointId,
                                                  const ::BreakpointConditionType conditionType,
                                                  const std::string_view expression,
                                                  const std::uint32_t hitCountTarget)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint condition", MODEL_NAME));
            return;
        }

        if (is_pending_id(breakpointId))
        {
            return;
        }

        auto priorState = Debugger::BreakpointState::Enabled;
        {
            std::scoped_lock lock{m_cacheMutex};
            const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
            {
                return bp.id == breakpointId;
            });
            if (it != m_cachedBreakpoints.end())
            {
                priorState = it->state;
                it->state = Debugger::BreakpointState::Pending;
            }
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
        }

        const auto generation = m_engine->get_generation();
        std::string expressionCopy{expression};

        std::packaged_task<StatusCode()> task(
            [this, breakpointId, conditionType, expr = std::move(expressionCopy), hitCountTarget, generation, priorState]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    resync_breakpoints_from_plugin();
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                ::BreakpointCondition condition{};
                condition.type = conditionType;
                condition.hitCountTarget = hitCountTarget;
                condition.enabled = 1;
                std::copy_n(expr.c_str(), std::min(expr.size(), static_cast<std::size_t>(VERTEX_MAX_CONDITION_LENGTH - 1)), condition.expression);

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_set_breakpoint_condition, breakpointId, &condition);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to set condition on breakpoint {}: {}", MODEL_NAME, breakpointId, static_cast<int>(status)));
                    resync_breakpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Condition set on breakpoint {} (type: {})", MODEL_NAME, breakpointId, static_cast<int>(conditionType)));

                wxTheApp->CallAfter([this, breakpointId, conditionType, expr = std::string{expr}, hitCountTarget, generation, priorState]()
                {
                    if (m_engine->get_generation() != generation) { return; }
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == breakpointId;
                        });
                        if (it != m_cachedBreakpoints.end())
                        {
                            it->state = priorState;
                            it->conditionType = conditionType;
                            it->condition = expr;
                            it->hitCountTarget = hitCountTarget;
                        }
                    }
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
    }

    void DebuggerModel::clear_breakpoint_condition(const std::uint32_t breakpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for breakpoint condition clear", MODEL_NAME));
            return;
        }

        if (is_pending_id(breakpointId))
        {
            return;
        }

        auto priorState = Debugger::BreakpointState::Enabled;
        {
            std::scoped_lock lock{m_cacheMutex};
            const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
            {
                return bp.id == breakpointId;
            });
            if (it != m_cachedBreakpoints.end())
            {
                priorState = it->state;
                it->state = Debugger::BreakpointState::Pending;
            }
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, breakpointId, generation, priorState]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    resync_breakpoints_from_plugin();
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_clear_breakpoint_condition, breakpointId);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to clear condition on breakpoint {}: {}", MODEL_NAME, breakpointId, static_cast<int>(status)));
                    resync_breakpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Condition cleared on breakpoint {}", MODEL_NAME, breakpointId));

                wxTheApp->CallAfter([this, breakpointId, generation, priorState]()
                {
                    if (m_engine->get_generation() != generation) { return; }
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        const auto it = std::ranges::find_if(m_cachedBreakpoints, [breakpointId](const Debugger::Breakpoint& bp)
                        {
                            return bp.id == breakpointId;
                        });
                        if (it != m_cachedBreakpoints.end())
                        {
                            it->state = priorState;
                            it->conditionType = VERTEX_BP_COND_NONE;
                            it->condition.clear();
                            it->hitCountTarget = 0;
                        }
                    }
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
    }

    StatusCode DebuggerModel::set_watchpoint(std::uint64_t address, std::uint32_t size, std::uint32_t* outWatchpointId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for watchpoint", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, address, size, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                ::Watchpoint wp{};
                wp.type = VERTEX_WP_READWRITE;
                wp.address = address;
                wp.size = size;
                wp.active = true;

                std::uint32_t watchpointId{};
                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_set_watchpoint, &wp, &watchpointId);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
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

                wxTheApp->CallAfter([this, cachedWp = cachedWp, generation]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }
                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedWatchpoints.push_back(cachedWp);
                    }
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Watchpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));

        if (outWatchpointId != nullptr)
        {
            *outWatchpointId = 0;
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

        {
            std::scoped_lock lock{m_cacheMutex};
            std::erase_if(m_cachedWatchpoints, [watchpointId](const Debugger::Watchpoint& wp)
            {
                return wp.id == watchpointId;
            });
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Watchpoints, m_engine->get_snapshot());
        }

        std::packaged_task<StatusCode()> task(
            [this, watchpointId]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_remove_watchpoint, watchpointId);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to remove watchpoint {}: {}", MODEL_NAME, watchpointId, static_cast<int>(status)));
                    resync_watchpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Watchpoint {} removed", MODEL_NAME, watchpointId));
                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerModel::enable_watchpoint(std::uint32_t watchpointId, bool enable)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            m_loggerService.log_warn(fmt::format("{}: No plugin loaded for watchpoint enable/disable", MODEL_NAME));
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        {
            std::scoped_lock lock{m_cacheMutex};
            const auto it = std::ranges::find_if(m_cachedWatchpoints, [watchpointId](const Debugger::Watchpoint& wp)
            {
                return wp.id == watchpointId;
            });
            if (it != m_cachedWatchpoints.end())
            {
                it->enabled = enable;
            }
        }

        if (m_eventHandler)
        {
            m_eventHandler(Debugger::DirtyFlags::Watchpoints, m_engine->get_snapshot());
        }

        std::packaged_task<StatusCode()> task(
            [this, watchpointId, enable]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_enable_watchpoint, watchpointId, enable ? 1 : 0);
                const auto status = Runtime::get_status(result);

                if (status != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to {} watchpoint {}: {}", MODEL_NAME, enable ? "enable" : "disable", watchpointId, static_cast<int>(status)));
                    resync_watchpoints_from_plugin();
                    return status;
                }

                m_loggerService.log_info(fmt::format("{}: Watchpoint {} {}", MODEL_NAME, watchpointId, enable ? "enabled" : "disabled"));
                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_fire_and_forget(Thread::ThreadChannel::Debugger, std::move(task));
        return StatusCode::STATUS_OK;
    }

    void DebuggerModel::resync_breakpoints_from_plugin()
    {
        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                ::BreakpointInfo* rawBreakpoints{};
                std::uint32_t count{};
                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_get_breakpoints, &rawBreakpoints, &count);
                if (!Runtime::status_ok(result) || (count > 0 && rawBreakpoints == nullptr))
                {
                    m_loggerService.log_warn(fmt::format("{}: Failed to resync breakpoints from plugin", MODEL_NAME));
                    return StatusCode::STATUS_ERROR_GENERAL;
                }

                auto synced = std::span{rawBreakpoints, count}
                    | std::views::transform([plugin](const ::BreakpointInfo& info)
                    {
                        Debugger::Breakpoint bp{};
                        bp.id = info.id;
                        bp.address = info.address;

                        switch (info.type)
                        {
                            case VERTEX_BP_READWRITE: bp.type = Debugger::BreakpointType::Hardware; break;
                            case VERTEX_BP_WRITE:     bp.type = Debugger::BreakpointType::Memory; break;
                            default:                  bp.type = Debugger::BreakpointType::Software; break;
                        }

                        switch (info.state)
                        {
                            case VERTEX_BP_STATE_DISABLED: bp.state = Debugger::BreakpointState::Disabled; break;
                            case VERTEX_BP_STATE_PENDING:  bp.state = Debugger::BreakpointState::Pending; break;
                            case VERTEX_BP_STATE_ERROR:    bp.state = Debugger::BreakpointState::Error; break;
                            default:                       bp.state = Debugger::BreakpointState::Enabled; break;
                        }

                        bp.hitCount = info.hitCount;
                        bp.temporary = info.temporary != 0;

                        ::BreakpointCondition sdkCondition{};
                        const auto condResult = Runtime::safe_call(plugin->internal_vertex_debugger_get_breakpoint_condition, info.id, &sdkCondition);
                        if (Runtime::status_ok(condResult))
                        {
                            bp.conditionType = sdkCondition.type;
                            bp.condition = sdkCondition.expression;
                            bp.hitCountTarget = sdkCondition.hitCountTarget;
                        }

                        return bp;
                    })
                    | std::ranges::to<std::vector>();

                wxTheApp->CallAfter([this, synced = std::move(synced), generation]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }
                    std::scoped_lock lock{m_cacheMutex};
                    m_cachedBreakpoints = std::move(synced);

                    for (const auto& [pendingId, pendingAdd] : m_pendingBreakpointAdds)
                    {
                        const bool alreadyRepresented = std::ranges::any_of(
                            m_cachedBreakpoints,
                            [pendingId, &pendingAdd](const Debugger::Breakpoint& bp)
                            {
                                return bp.id == pendingId
                                    || (!is_pending_id(bp.id) && bp.address == pendingAdd.address);
                            });

                        if (!alreadyRepresented)
                        {
                            Debugger::Breakpoint pendingBp{};
                            pendingBp.id = pendingId;
                            pendingBp.address = pendingAdd.address;
                            pendingBp.type = pendingAdd.type;
                            pendingBp.state = Debugger::BreakpointState::Pending;
                            m_cachedBreakpoints.push_back(std::move(pendingBp));
                        }
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Breakpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
    }

    void DebuggerModel::resync_watchpoints_from_plugin()
    {
        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }
                auto* plugin = &pluginOpt.value().get();

                ::WatchpointInfo* rawWatchpoints{};
                std::uint32_t count{};
                const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_get_watchpoints, &rawWatchpoints, &count);
                if (!Runtime::status_ok(result) || (count > 0 && rawWatchpoints == nullptr))
                {
                    m_loggerService.log_warn(fmt::format("{}: Failed to resync watchpoints from plugin", MODEL_NAME));
                    return StatusCode::STATUS_ERROR_GENERAL;
                }

                auto synced = std::span{rawWatchpoints, count}
                    | std::views::transform([](const ::WatchpointInfo& info)
                    {
                        Debugger::Watchpoint wp{};
                        wp.id = info.id;
                        wp.address = info.address;
                        wp.size = info.size;

                        switch (info.type)
                        {
                            case VERTEX_WP_READ:      wp.type = Debugger::WatchpointType::Read; break;
                            case VERTEX_WP_WRITE:     wp.type = Debugger::WatchpointType::Write; break;
                            case VERTEX_WP_EXECUTE:   wp.type = Debugger::WatchpointType::Execute; break;
                            default:                  wp.type = Debugger::WatchpointType::ReadWrite; break;
                        }

                        wp.enabled = info.enabled != 0;
                        wp.hitCount = info.hitCount;
                        return wp;
                    })
                    | std::ranges::to<std::vector>();

                wxTheApp->CallAfter([this, synced = std::move(synced), generation]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }
                    std::scoped_lock lock{m_cacheMutex};
                    m_cachedWatchpoints = std::move(synced);
                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Watchpoints, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
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

    const Debugger::ExceptionData& DebuggerModel::get_cached_exception() const
    {
        return m_cachedException;
    }

    bool DebuggerModel::is_attached() const
    {
        const auto state = m_cachedSnapshot.state;
        return state != Debugger::EngineState::Idle
            && state != Debugger::EngineState::Detached
            && state != Debugger::EngineState::Stopped;
    }

    Debugger::DebuggerState DebuggerModel::get_debugger_state() const
    {
        switch (m_cachedSnapshot.state)
        {
            case Debugger::EngineState::Idle:
            case Debugger::EngineState::Detached:
            case Debugger::EngineState::Stopped:
                return Debugger::DebuggerState::Detached;
            case Debugger::EngineState::Running:
                return Debugger::DebuggerState::Running;
            case Debugger::EngineState::Paused:
                return m_cachedSnapshot.hasException
                    ? Debugger::DebuggerState::Exception
                    : Debugger::DebuggerState::Paused;
            case Debugger::EngineState::Exited:
                return Debugger::DebuggerState::Detached;
            default:
                return Debugger::DebuggerState::Detached;
        }
    }

    std::uint64_t DebuggerModel::get_current_address() const
    {
        std::scoped_lock lock{m_cacheMutex};
        return m_navigationAddress != 0 ? m_navigationAddress : m_cachedSnapshot.currentAddress;
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

    void DebuggerModel::request_modules()
    {
        auto& tracker = m_queryModules;

        tracker.pendingRedispatch.store(true, std::memory_order_release);

        if (tracker.inflight.load(std::memory_order_acquire))
        {
            return;
        }

        tracker.inflight.store(true, std::memory_order_release);
        tracker.pendingRedispatch.store(false, std::memory_order_release);

        const auto generation = m_engine->get_generation();
        tracker.dispatchGeneration = generation;

        std::packaged_task<StatusCode()> task(
            [this, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryModules.inflight.store(false, std::memory_order_release);
                        if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_modules();
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();

                std::uint32_t count{};
                const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, nullptr, &count);
                const auto countStatus = Runtime::get_status(countResult);
                if (countStatus == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND)
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryModules.inflight.store(false, std::memory_order_release);
                    });
                    return StatusCode::STATUS_ERROR_NOT_IMPLEMENTED;
                }
                if (!Runtime::status_ok(countResult))
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to get modules count", MODEL_NAME));
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryModules.inflight.store(false, std::memory_order_release);
                        if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_modules();
                        }
                    });
                    return countStatus;
                }

                if (count == 0)
                {
                    wxTheApp->CallAfter([this, generation]()
                    {
                        m_queryModules.inflight.store(false, std::memory_order_release);
                        if (m_engine->get_generation() != generation)
                        {
                            if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                            {
                                request_modules();
                            }
                            return;
                        }
                        {
                            std::scoped_lock lock{m_cacheMutex};
                            m_cachedModules.clear();
                        }
                        if (m_eventHandler)
                        {
                            m_eventHandler(Debugger::DirtyFlags::Modules, m_engine->get_snapshot());
                        }
                    });
                    return StatusCode::STATUS_OK;
                }

                std::vector<::ModuleInformation> modules(count);
                auto* modulesPtr = modules.data();
                const auto result = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, &modulesPtr, &count);
                if (!Runtime::status_ok(result))
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to get modules list", MODEL_NAME));
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryModules.inflight.store(false, std::memory_order_release);
                        if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_modules();
                        }
                    });
                    return Runtime::get_status(result);
                }

                modules.resize(count);

                auto converted = modules
                    | std::views::transform([](const auto& m)
                    {
                        return Debugger::ModuleInfo{
                            .name = m.moduleName,
                            .path = m.modulePath,
                            .baseAddress = m.baseAddress,
                            .size = m.size
                        };
                    })
                    | std::ranges::to<std::vector>();

                m_loggerService.log_info(fmt::format("{}: Loaded {} modules", MODEL_NAME, converted.size()));

                wxTheApp->CallAfter([this, generation, modules = std::move(converted)]() mutable
                {
                    m_queryModules.inflight.store(false, std::memory_order_release);

                    if (m_engine->get_generation() != generation)
                    {
                        if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_modules();
                        }
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedModules = std::move(modules);
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Modules, m_engine->get_snapshot());
                    }

                    request_symbol_table();

                    if (m_queryModules.pendingRedispatch.load(std::memory_order_acquire))
                    {
                        request_modules();
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!result.has_value())
        {
            tracker.inflight.store(false, std::memory_order_release);
        }
    }

    void DebuggerModel::request_symbol_table()
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return;
        }

        std::vector<Debugger::ModuleInfo> modulesCopy{};
        {
            std::scoped_lock lock{m_cacheMutex};
            modulesCopy = m_cachedModules;
        }

        if (modulesCopy.empty())
        {
            return;
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation, modules = std::move(modulesCopy)]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                const auto& plugin = pluginOpt.value().get();

                std::unordered_map<std::uint64_t, std::string> table{};
                const bool hasSymbolEnumeration =
                    plugin.internal_vertex_symbol_enumerate_functions &&
                    plugin.internal_vertex_symbol_free_enumeration;

                if (!hasSymbolEnumeration)
                {
                    m_loggerService.log_warn(
                        fmt::format("{}: Symbol enumeration API is unavailable; skipping symbol table build", MODEL_NAME));
                }

                for (const auto& mod : modules)
                {
                    if (!hasSymbolEnumeration || mod.baseAddress == 0)
                    {
                        continue;
                    }

                    SymbolInfo* symbols = nullptr;
                    std::uint32_t symbolCount = 0;

                    const auto enumerateResult = Runtime::safe_call(
                        plugin.internal_vertex_symbol_enumerate_functions,
                        mod.baseAddress,
                        &symbols,
                        &symbolCount
                    );

                    if (Runtime::status_ok(enumerateResult) && symbols && symbolCount > 0)
                    {
                        for (const auto& symbol : std::span{symbols, symbolCount})
                        {
                            if (symbol.address == 0)
                            {
                                continue;
                            }

                            const auto nameBegin = std::begin(symbol.name);
                            const auto nameEnd = std::find(nameBegin, std::end(symbol.name), '\0');
                            if (nameBegin != nameEnd)
                            {
                                table[symbol.address] = std::string{nameBegin, nameEnd};
                            }
                        }
                    }

                    if (symbols)
                    {
                        std::ignore = Runtime::safe_call(
                            plugin.internal_vertex_symbol_free_enumeration,
                            symbols
                        );
                    }
                }

                m_loggerService.log_info(fmt::format("{}: Built symbol table with {} entries", MODEL_NAME, table.size()));

                wxTheApp->CallAfter([this, generation, symbolTable = std::move(table)]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_symbolTable = std::move(symbolTable);
                    }

                    if (!m_cachedDisassembly.lines.empty())
                    {
                        {
                            std::scoped_lock lock{m_cacheMutex};
                            resolve_disassembly_symbols(m_cachedDisassembly, m_cachedModules, m_symbolTable);
                        }

                        if (m_eventHandler)
                        {
                            m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                        }
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
    }

    void DebuggerModel::request_disassembly(std::uint64_t address)
    {
        auto& tracker = m_queryDisassembly;

        m_pendingDisasmAddress.store(address, std::memory_order_release);
        tracker.pendingRedispatch.store(true, std::memory_order_release);

        if (tracker.inflight.load(std::memory_order_acquire))
        {
            return;
        }

        tracker.inflight.store(true, std::memory_order_release);
        tracker.pendingRedispatch.store(false, std::memory_order_release);

        const auto generation = m_engine->get_generation();
        tracker.dispatchGeneration = generation;
        const auto targetAddress = m_pendingDisasmAddress.load(std::memory_order_acquire);

        constexpr std::size_t MAX_INSTRUCTIONS = 500;
        constexpr std::size_t DISASM_BYTES = 4096;

        std::packaged_task<StatusCode()> task(
            [this, generation, targetAddress]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryDisassembly.inflight.store(false, std::memory_order_release);
                        if (m_queryDisassembly.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_disassembly(m_pendingDisasmAddress.load(std::memory_order_acquire));
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();

                std::vector<::DisassemblerResult> resultBuffer(MAX_INSTRUCTIONS);
                ::DisassemblerResults results{};
                results.results = resultBuffer.data();
                results.count = 0;
                results.capacity = static_cast<std::uint32_t>(MAX_INSTRUCTIONS);
                results.startAddress = targetAddress;

                const auto disasmResult = Runtime::safe_call(
                    plugin.internal_vertex_process_disassemble_range,
                    targetAddress,
                    static_cast<std::uint32_t>(DISASM_BYTES),
                    &results
                );
                const auto status = Runtime::get_status(disasmResult);

                if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND || status != StatusCode::STATUS_OK || results.count == 0)
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryDisassembly.inflight.store(false, std::memory_order_release);
                        if (m_queryDisassembly.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_disassembly(m_pendingDisasmAddress.load(std::memory_order_acquire));
                        }
                    });
                    return status;
                }

                Debugger::DisassemblyRange newDisasm{};
                newDisasm.startAddress = targetAddress;

                for (const auto& instr : std::span{results.results, results.count})
                {
                    newDisasm.lines.push_back(convert_disasm_result(instr, targetAddress));
                    newDisasm.endAddress = instr.address + instr.size;
                }

                m_loggerService.log_info(fmt::format("{}: Disassembled {} instructions at 0x{:X}", MODEL_NAME, results.count, targetAddress));

                {
                    std::scoped_lock lock{m_cacheMutex};
                    resolve_disassembly_symbols(newDisasm, m_cachedModules, m_symbolTable);
                }

                wxTheApp->CallAfter([this, generation, disasm = std::move(newDisasm)]() mutable
                {
                    m_queryDisassembly.inflight.store(false, std::memory_order_release);

                    if (m_engine->get_generation() != generation)
                    {
                        if (m_queryDisassembly.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_disassembly(m_pendingDisasmAddress.load(std::memory_order_acquire));
                        }
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedDisassembly = std::move(disasm);
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                    }

                    if (m_queryDisassembly.pendingRedispatch.load(std::memory_order_acquire))
                    {
                        request_disassembly(m_pendingDisasmAddress.load(std::memory_order_acquire));
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!result.has_value())
        {
            tracker.inflight.store(false, std::memory_order_release);
        }
    }

    void DebuggerModel::request_disassembly_extend_up(std::uint64_t fromAddress, std::size_t byteCount)
    {
        auto notifyResult = [this](Debugger::ExtensionResult result)
        {
            wxTheApp->CallAfter([this, result]()
            {
                if (m_eventHandler)
                {
                    m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                }
                if (m_extensionResultHandler)
                {
                    m_extensionResultHandler(true, result);
                }
            });
        };

        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            notifyResult(Debugger::ExtensionResult::Error);
            return;
        }

        const auto generation = m_engine->get_generation();
        const std::uint64_t startAddress = (fromAddress > byteCount) ? (fromAddress - byteCount) : 0;
        if (startAddress >= fromAddress)
        {
            notifyResult(Debugger::ExtensionResult::EndOfRange);
            return;
        }

        std::packaged_task<StatusCode()> task(
            [this, generation, startAddress, fromAddress, notifyResult]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    notifyResult(Debugger::ExtensionResult::Error);
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();

                constexpr std::size_t MAX_INSTRUCTIONS = 200;
                std::vector<::DisassemblerResult> resultBuffer(MAX_INSTRUCTIONS);
                ::DisassemblerResults results{};
                results.results = resultBuffer.data();
                results.count = 0;
                results.capacity = static_cast<std::uint32_t>(MAX_INSTRUCTIONS);
                results.startAddress = startAddress;

                const auto disasmResult = Runtime::safe_call(
                    plugin.internal_vertex_process_disassemble_range,
                    startAddress,
                    static_cast<std::uint32_t>(fromAddress - startAddress),
                    &results
                );
                const auto status = Runtime::get_status(disasmResult);

                if (status != StatusCode::STATUS_OK)
                {
                    notifyResult(Debugger::ExtensionResult::Error);
                    return status;
                }

                if (results.count == 0)
                {
                    notifyResult(Debugger::ExtensionResult::EndOfRange);
                    return status;
                }

                std::vector<Debugger::DisassemblyLine> newLines{};
                newLines.reserve(results.count);

                for (const auto& instr : std::span{results.results, results.count}
                    | std::views::take_while([fromAddress](const auto& r) { return r.address < fromAddress; }))
                {
                    newLines.push_back(convert_disasm_result(instr, 0));
                }

                if (newLines.empty())
                {
                    notifyResult(Debugger::ExtensionResult::EndOfRange);
                    return StatusCode::STATUS_OK;
                }

                {
                    Debugger::DisassemblyRange tempRange{};
                    tempRange.lines = std::move(newLines);
                    std::scoped_lock lock{m_cacheMutex};
                    resolve_disassembly_symbols(tempRange, m_cachedModules, m_symbolTable);
                    newLines = std::move(tempRange.lines);
                }

                wxTheApp->CallAfter([this, generation, newLines = std::move(newLines)]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
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

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                    }
                    if (m_extensionResultHandler)
                    {
                        m_extensionResultHandler(true, Debugger::ExtensionResult::Success);
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto notifyUpError = [this]()
        {
            wxTheApp->CallAfter([this]()
            {
                if (m_extensionResultHandler)
                {
                    m_extensionResultHandler(true, Debugger::ExtensionResult::Error);
                }
            });
        };

        auto dispatchResult = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!dispatchResult.has_value()) [[unlikely]]
        {
            notifyUpError();
        }
    }

    void DebuggerModel::request_disassembly_extend_down(std::uint64_t fromAddress, std::size_t byteCount)
    {
        auto notifyResult = [this](Debugger::ExtensionResult result)
        {
            wxTheApp->CallAfter([this, result]()
            {
                if (m_eventHandler)
                {
                    m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                }
                if (m_extensionResultHandler)
                {
                    m_extensionResultHandler(false, result);
                }
            });
        };

        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            notifyResult(Debugger::ExtensionResult::Error);
            return;
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation, fromAddress, byteCount, notifyResult]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    notifyResult(Debugger::ExtensionResult::Error);
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

                const auto disasmResult = Runtime::safe_call(
                    plugin.internal_vertex_process_disassemble_range,
                    fromAddress,
                    static_cast<std::uint32_t>(byteCount),
                    &results
                );
                const auto status = Runtime::get_status(disasmResult);

                if (status != StatusCode::STATUS_OK)
                {
                    notifyResult(Debugger::ExtensionResult::Error);
                    return status;
                }

                if (results.count == 0)
                {
                    notifyResult(Debugger::ExtensionResult::EndOfRange);
                    return status;
                }

                std::vector<Debugger::DisassemblyLine> newLines{};
                for (const auto& instr : std::span{results.results, results.count}
                    | std::views::filter([fromAddress](const auto& r) { return r.address >= fromAddress; }))
                {
                    newLines.push_back(convert_disasm_result(instr, 0));
                }

                if (newLines.empty())
                {
                    notifyResult(Debugger::ExtensionResult::EndOfRange);
                    return StatusCode::STATUS_OK;
                }

                {
                    Debugger::DisassemblyRange tempRange{};
                    tempRange.lines = std::move(newLines);
                    std::scoped_lock lock{m_cacheMutex};
                    resolve_disassembly_symbols(tempRange, m_cachedModules, m_symbolTable);
                    newLines = std::move(tempRange.lines);
                }

                wxTheApp->CallAfter([this, generation, newLines = std::move(newLines)]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        for (auto& line : newLines)
                        {
                            m_cachedDisassembly.lines.push_back(std::move(line));
                        }
                        const auto& lastLine = m_cachedDisassembly.lines.back();
                        m_cachedDisassembly.endAddress = lastLine.address + lastLine.bytes.size();

                        if (m_cachedDisassembly.lines.size() > MAX_DISASSEMBLY_LINES)
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
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Disassembly, m_engine->get_snapshot());
                    }
                    if (m_extensionResultHandler)
                    {
                        m_extensionResultHandler(false, Debugger::ExtensionResult::Success);
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto notifyDownError = [this]()
        {
            wxTheApp->CallAfter([this]()
            {
                if (m_extensionResultHandler)
                {
                    m_extensionResultHandler(false, Debugger::ExtensionResult::Error);
                }
            });
        };

        auto dispatchResult = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!dispatchResult.has_value()) [[unlikely]]
        {
            notifyDownError();
        }
    }

    void DebuggerModel::request_registers()
    {
        auto& tracker = m_queryRegisters;

        tracker.pendingRedispatch.store(true, std::memory_order_release);

        if (tracker.inflight.load(std::memory_order_acquire))
        {
            return;
        }

        tracker.inflight.store(true, std::memory_order_release);
        tracker.pendingRedispatch.store(false, std::memory_order_release);

        const auto generation = m_engine->get_generation();
        tracker.dispatchGeneration = generation;

        std::uint32_t threadId = m_cachedSnapshot.currentThreadId;

        std::packaged_task<StatusCode()> task(
            [this, generation, threadId]() mutable -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryRegisters.inflight.store(false, std::memory_order_release);
                        if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_registers();
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();

                if (threadId == 0)
                {
                    const auto currentThreadResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_current_thread, &threadId);
                    if (!Runtime::status_ok(currentThreadResult))
                    {
                        threadId = 0;
                    }

                    if (threadId == 0)
                    {
                        ::ThreadList threadList{};
                        const auto threadsResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_threads, &threadList);
                        if (!Runtime::status_ok(threadsResult) || threadList.threadCount == 0)
                        {
                            wxTheApp->CallAfter([this]()
                            {
                                m_queryRegisters.inflight.store(false, std::memory_order_release);
                                if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                                {
                                    request_registers();
                                }
                            });
                            return StatusCode::STATUS_ERROR_THREAD_NOT_FOUND;
                        }

                        threadId = threadList.currentThreadId != 0 ? threadList.currentThreadId : threadList.threads[0].id;
                    }
                }

                ::RegisterSet sdkRegs{};

                const auto snapshot = m_engine->get_snapshot();
                const bool isPausedState = snapshot.state == Debugger::EngineState::Paused;

                StatusCode regStatus{};
                if (isPausedState)
                {
                    const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
                    regStatus = Runtime::get_status(result);
                }
                else
                {
                    const auto suspendResult = Runtime::safe_call(plugin.internal_vertex_debugger_suspend_thread, threadId);
                    const auto suspendStatus = Runtime::get_status(suspendResult);
                    if (suspendStatus != StatusCode::STATUS_OK)
                    {
                        m_loggerService.log_error(fmt::format("{}: Failed to suspend thread {} for register read: {}",
                            MODEL_NAME, threadId, static_cast<int>(suspendStatus)));
                        wxTheApp->CallAfter([this]()
                        {
                            m_queryRegisters.inflight.store(false, std::memory_order_release);
                            if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                            {
                                request_registers();
                            }
                        });
                        return suspendStatus;
                    }

                    const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
                    regStatus = Runtime::get_status(result);

                    const auto resumeResult = Runtime::safe_call(plugin.internal_vertex_debugger_resume_thread, threadId);
                    const auto resumeStatus = Runtime::get_status(resumeResult);
                    if (resumeStatus != StatusCode::STATUS_OK)
                    {
                        m_loggerService.log_warn(fmt::format("{}: Failed to resume thread {} after register read: {}",
                            MODEL_NAME, threadId, static_cast<int>(resumeStatus)));
                    }
                }

                if (regStatus != StatusCode::STATUS_OK)
                {
                    m_loggerService.log_error(fmt::format("{}: Failed to read registers for thread {}: {}",
                        MODEL_NAME, threadId, static_cast<int>(regStatus)));
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryRegisters.inflight.store(false, std::memory_order_release);
                        if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_registers();
                        }
                    });
                    return regStatus;
                }

                Debugger::RegisterSet newRegs{};
                newRegs.instructionPointer = sdkRegs.instructionPointer;
                newRegs.stackPointer = sdkRegs.stackPointer;
                newRegs.basePointer = sdkRegs.basePointer;

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
                            newRegs.segment.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_FLAGS:
                            reg.category = Debugger::RegisterCategory::Flags;
                            newRegs.flags.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_FLOATING_POINT:
                            reg.category = Debugger::RegisterCategory::FloatingPoint;
                            newRegs.floatingPoint.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_VECTOR:
                            reg.category = Debugger::RegisterCategory::Vector;
                            newRegs.vector.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_GENERAL:
                        default:
                            reg.category = Debugger::RegisterCategory::General;
                            newRegs.generalPurpose.push_back(std::move(reg));
                            break;
                    }
                }

                wxTheApp->CallAfter([this, generation, threadId, newRegs = std::move(newRegs),
                    ip = sdkRegs.instructionPointer]() mutable
                {
                    m_queryRegisters.inflight.store(false, std::memory_order_release);

                    if (m_engine->get_generation() != generation)
                    {
                        if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_registers();
                        }
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedRegisters = std::move(newRegs);
                        m_cachedSnapshot.currentThreadId = threadId;
                        if (ip != 0)
                        {
                            m_cachedSnapshot.currentAddress = ip;
                            m_navigationAddress = 0;
                        }
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Registers, m_engine->get_snapshot());
                    }

                    if (m_queryRegisters.pendingRedispatch.load(std::memory_order_acquire))
                    {
                        request_registers();
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!result.has_value())
        {
            tracker.inflight.store(false, std::memory_order_release);
        }
    }

    void DebuggerModel::request_registers_for_thread(std::uint32_t threadId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return;
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation, threadId]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();
                ::RegisterSet sdkRegs{};

                const auto snapshot = m_engine->get_snapshot();
                const bool isPausedState = snapshot.state == Debugger::EngineState::Paused;

                StatusCode regStatus{};
                if (isPausedState)
                {
                    const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
                    regStatus = Runtime::get_status(result);
                }
                else
                {
                    const auto suspendResult = Runtime::safe_call(plugin.internal_vertex_debugger_suspend_thread, threadId);
                    if (Runtime::get_status(suspendResult) != StatusCode::STATUS_OK)
                    {
                        return Runtime::get_status(suspendResult);
                    }

                    const auto result = Runtime::safe_call(plugin.internal_vertex_debugger_get_registers, threadId, &sdkRegs);
                    regStatus = Runtime::get_status(result);

                    std::ignore = Runtime::safe_call(plugin.internal_vertex_debugger_resume_thread, threadId);
                }

                if (regStatus != StatusCode::STATUS_OK)
                {
                    return regStatus;
                }

                Debugger::RegisterSet newRegs{};
                newRegs.instructionPointer = sdkRegs.instructionPointer;
                newRegs.stackPointer = sdkRegs.stackPointer;
                newRegs.basePointer = sdkRegs.basePointer;

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
                            newRegs.segment.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_FLAGS:
                            reg.category = Debugger::RegisterCategory::Flags;
                            newRegs.flags.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_FLOATING_POINT:
                            reg.category = Debugger::RegisterCategory::FloatingPoint;
                            newRegs.floatingPoint.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_VECTOR:
                            reg.category = Debugger::RegisterCategory::Vector;
                            newRegs.vector.push_back(std::move(reg));
                            break;
                        case VERTEX_REG_GENERAL:
                        default:
                            reg.category = Debugger::RegisterCategory::General;
                            newRegs.generalPurpose.push_back(std::move(reg));
                            break;
                    }
                }

                wxTheApp->CallAfter([this, generation, threadId, newRegs = std::move(newRegs),
                    ip = sdkRegs.instructionPointer]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedRegisters = std::move(newRegs);
                        m_cachedSnapshot.currentThreadId = threadId;
                        if (ip != 0)
                        {
                            m_cachedSnapshot.currentAddress = ip;
                            m_navigationAddress = 0;
                        }
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Registers, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
    }

    void DebuggerModel::request_call_stack()
    {
        auto& tracker = m_queryCallStack;

        tracker.pendingRedispatch.store(true, std::memory_order_release);

        if (tracker.inflight.load(std::memory_order_acquire))
        {
            return;
        }

        tracker.inflight.store(true, std::memory_order_release);
        tracker.pendingRedispatch.store(false, std::memory_order_release);

        const auto generation = m_engine->get_generation();
        tracker.dispatchGeneration = generation;

        const std::uint32_t threadId = m_cachedSnapshot.currentThreadId;
        if (threadId == 0)
        {
            tracker.inflight.store(false, std::memory_order_release);
            return;
        }

        std::packaged_task<StatusCode()> task(
            [this, generation, threadId]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryCallStack.inflight.store(false, std::memory_order_release);
                        if (m_queryCallStack.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_call_stack();
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();
                ::CallStack sdkCallStack{};

                const auto stackResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_call_stack, threadId, &sdkCallStack);
                const auto status = Runtime::get_status(stackResult);

                if (status == StatusCode::STATUS_ERROR_FUNCTION_NOT_FOUND || status != StatusCode::STATUS_OK)
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryCallStack.inflight.store(false, std::memory_order_release);
                        if (m_queryCallStack.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_call_stack();
                        }
                    });
                    return status;
                }

                Debugger::CallStack newCallStack{};
                newCallStack.currentFrameIndex = sdkCallStack.currentFrameIndex;

                for (const auto& sdkFrame
                    : std::span{sdkCallStack.frames, std::min<std::uint32_t>(sdkCallStack.frameCount, VERTEX_MAX_STACK_FRAMES)})
                {
                    Debugger::StackFrame frame{};
                    frame.frameIndex = sdkFrame.frameIndex;
                    frame.returnAddress = sdkFrame.returnAddress;
                    frame.framePointer = sdkFrame.framePointer;
                    frame.stackPointer = sdkFrame.stackPointer;
                    frame.functionName = sdkFrame.functionName;
                    frame.moduleName = sdkFrame.moduleName;
                    frame.sourceFile = sdkFrame.sourceFile;
                    frame.sourceLine = sdkFrame.sourceLine;

                    newCallStack.frames.push_back(std::move(frame));
                }

                const auto frameCount = newCallStack.frames.size();
                m_loggerService.log_info(fmt::format("{}: Loaded {} stack frames for thread {}",
                    MODEL_NAME, frameCount, threadId));

                wxTheApp->CallAfter([this, generation, callStack = std::move(newCallStack)]() mutable
                {
                    m_queryCallStack.inflight.store(false, std::memory_order_release);

                    if (m_engine->get_generation() != generation)
                    {
                        if (m_queryCallStack.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_call_stack();
                        }
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedCallStack = std::move(callStack);
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::CallStack, m_engine->get_snapshot());
                    }

                    if (m_queryCallStack.pendingRedispatch.load(std::memory_order_acquire))
                    {
                        request_call_stack();
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!result.has_value())
        {
            tracker.inflight.store(false, std::memory_order_release);
        }
    }

    void DebuggerModel::request_call_stack_for_thread(std::uint32_t threadId)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK || threadId == 0)
        {
            return;
        }

        const auto generation = m_engine->get_generation();

        std::packaged_task<StatusCode()> task(
            [this, generation, threadId]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();
                ::CallStack sdkCallStack{};

                const auto stackResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_call_stack, threadId, &sdkCallStack);
                const auto status = Runtime::get_status(stackResult);

                if (status != StatusCode::STATUS_OK)
                {
                    return status;
                }

                Debugger::CallStack newCallStack{};
                newCallStack.currentFrameIndex = sdkCallStack.currentFrameIndex;

                for (const auto& sdkFrame
                    : std::span{sdkCallStack.frames, std::min<std::uint32_t>(sdkCallStack.frameCount, VERTEX_MAX_STACK_FRAMES)})
                {
                    Debugger::StackFrame frame{};
                    frame.frameIndex = sdkFrame.frameIndex;
                    frame.returnAddress = sdkFrame.returnAddress;
                    frame.framePointer = sdkFrame.framePointer;
                    frame.stackPointer = sdkFrame.stackPointer;
                    frame.functionName = sdkFrame.functionName;
                    frame.moduleName = sdkFrame.moduleName;
                    frame.sourceFile = sdkFrame.sourceFile;
                    frame.sourceLine = sdkFrame.sourceLine;

                    newCallStack.frames.push_back(std::move(frame));
                }

                wxTheApp->CallAfter([this, generation, callStack = std::move(newCallStack)]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedCallStack = std::move(callStack);
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::CallStack, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
    }

    void DebuggerModel::request_threads()
    {
        auto& tracker = m_queryThreads;

        tracker.pendingRedispatch.store(true, std::memory_order_release);

        if (tracker.inflight.load(std::memory_order_acquire))
        {
            return;
        }

        tracker.inflight.store(true, std::memory_order_release);
        tracker.pendingRedispatch.store(false, std::memory_order_release);

        const auto generation = m_engine->get_generation();
        tracker.dispatchGeneration = generation;

        std::packaged_task<StatusCode()> task(
            [this, generation]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryThreads.inflight.store(false, std::memory_order_release);
                        if (m_queryThreads.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_threads();
                        }
                    });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                const auto& plugin = pluginOpt.value().get();

                ::ThreadList threadList{};
                std::vector<Debugger::ThreadInfo> loadedThreads{};

                const auto threadResult = Runtime::safe_call(plugin.internal_vertex_debugger_get_threads, &threadList);
                const auto taskStatus = Runtime::get_status(threadResult);
                if (taskStatus != StatusCode::STATUS_OK)
                {
                    wxTheApp->CallAfter([this]()
                    {
                        m_queryThreads.inflight.store(false, std::memory_order_release);
                        if (m_queryThreads.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_threads();
                        }
                    });
                    return taskStatus;
                }

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

                    loadedThreads.push_back(std::move(thread));
                }

                const auto currentThreadId = threadList.currentThreadId;

                wxTheApp->CallAfter([this, generation, threads = std::move(loadedThreads), currentThreadId]() mutable
                {
                    m_queryThreads.inflight.store(false, std::memory_order_release);

                    if (m_engine->get_generation() != generation)
                    {
                        if (m_queryThreads.pendingRedispatch.load(std::memory_order_acquire))
                        {
                            request_threads();
                        }
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedThreads = std::move(threads);

                        if (currentThreadId != 0 && m_cachedSnapshot.currentThreadId == 0)
                        {
                            m_cachedSnapshot.currentThreadId = currentThreadId;
                        }
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::Threads, m_engine->get_snapshot());
                    }

                    if (m_queryThreads.pendingRedispatch.load(std::memory_order_acquire))
                    {
                        request_threads();
                    }
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));

        if (!result.has_value())
        {
            tracker.inflight.store(false, std::memory_order_release);
        }
    }

    void DebuggerModel::request_module_imports_exports(const std::string_view moduleName)
    {
        if (m_loaderService.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return;
        }

        const auto it = std::ranges::find_if(m_cachedModules, [moduleName](const auto& mod)
        {
            return mod.name == moduleName;
        });

        if (it == m_cachedModules.end())
        {
            return;
        }

        const auto& targetModule = *it;
        const auto generation = m_engine->get_generation();

        ModuleInformation moduleInfo{};
        const auto nameLen = std::min(targetModule.name.size(), static_cast<std::size_t>(VERTEX_MAX_NAME_LENGTH - 1));
        std::copy_n(targetModule.name.data(), nameLen, moduleInfo.moduleName);
        moduleInfo.moduleName[nameLen] = '\0';

        const auto pathLen = std::min(targetModule.path.size(), static_cast<std::size_t>(VERTEX_MAX_PATH_LENGTH - 1));
        std::copy_n(targetModule.path.data(), pathLen, moduleInfo.modulePath);
        moduleInfo.modulePath[pathLen] = '\0';

        moduleInfo.baseAddress = targetModule.baseAddress;
        moduleInfo.size = targetModule.size;

        std::packaged_task<StatusCode()> task(
            [this, generation, moduleInfo]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                const auto& plugin = pluginOpt.value().get();

                std::vector<Debugger::ImportEntry> loadedImports{};
                std::vector<Debugger::ExportEntry> loadedExports{};

                ModuleImport* imports = nullptr;
                std::uint32_t importCount = 0;

                auto modInfo = moduleInfo;
                const auto importsResult = Runtime::safe_call(plugin.internal_vertex_process_get_module_imports, &modInfo, &imports, &importCount);
                if (Runtime::status_ok(importsResult) && imports && importCount > 0)
                {
                    loadedImports = std::span{imports, importCount}
                        | std::views::transform([](const auto& imp)
                        {
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

                const auto exportsResult = Runtime::safe_call(plugin.internal_vertex_process_get_module_exports, &modInfo, &exports, &exportCount);
                if (Runtime::status_ok(exportsResult) && exports && exportCount > 0)
                {
                    loadedExports = std::span{exports, exportCount}
                        | std::views::transform([](const auto& exp)
                        {
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

                wxTheApp->CallAfter([this, generation,
                    loadedImports = std::move(loadedImports),
                    loadedExports = std::move(loadedExports)]() mutable
                {
                    if (m_engine->get_generation() != generation)
                    {
                        return;
                    }

                    {
                        std::scoped_lock lock{m_cacheMutex};
                        m_cachedImports = std::move(loadedImports);
                        m_cachedExports = std::move(loadedExports);
                    }

                    if (m_eventHandler)
                    {
                        m_eventHandler(Debugger::DirtyFlags::ImportsExports, m_engine->get_snapshot());
                    }
                });

                return StatusCode::STATUS_OK;
            });

        std::ignore = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::Low,
            std::move(task));
    }

    Debugger::DisassemblyLine DebuggerModel::convert_disasm_result(const ::DisassemblerResult& instr, const std::uint64_t currentAddress)
    {
        Debugger::DisassemblyLine line{};
        line.address = instr.address;
        line.mnemonic = instr.mnemonic;
        line.operands = instr.operands;
        line.isCurrentInstruction = (instr.address == currentAddress);

        if (instr.comment[0] != '\0')
        {
            line.comment = instr.comment;
        }

        if (instr.targetSymbol[0] != '\0')
        {
            line.targetSymbolName = instr.targetSymbol;
        }

        if (instr.sectionName[0] != '\0')
        {
            line.sectionName = instr.sectionName;
        }

        line.functionStart = instr.functionStart;
        line.isFunctionEntry = (instr.flags & VERTEX_FLAG_ENTRY_POINT) != 0 ||
                               (instr.functionStart != 0 && instr.functionStart == instr.address);

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

        return line;
    }

    void DebuggerModel::resolve_disassembly_symbols(Debugger::DisassemblyRange& range,
                                                     const std::span<const Debugger::ModuleInfo> modules,
                                                     const std::unordered_map<std::uint64_t, std::string>& symbolTable)
    {
        if (range.lines.empty())
        {
            return;
        }

        std::unordered_set<std::uint64_t> branchTargets{};
        for (const auto& line : range.lines)
        {
            if (line.branchTarget.has_value())
            {
                branchTargets.insert(*line.branchTarget);
            }
        }

        for (auto& line : range.lines)
        {
            for (const auto& mod : modules)
            {
                if (line.address >= mod.baseAddress &&
                    line.address < mod.baseAddress + mod.size)
                {
                    line.moduleName = mod.name;
                    break;
                }
            }

            if (auto it = symbolTable.find(line.address); it != symbolTable.end())
            {
                line.symbolName = it->second;
                line.isFunctionEntry = true;
            }
            else if (line.isFunctionEntry && line.symbolName.empty())
            {
                line.symbolName = fmt::format("sub_{:X}", line.address);
            }
            else if (!line.isFunctionEntry && line.symbolName.empty() &&
                     branchTargets.contains(line.address))
            {
                line.symbolName = fmt::format("loc_{:X}", line.address);
            }

            if (line.branchTarget.has_value() && line.targetSymbolName.empty())
            {
                if (auto it = symbolTable.find(*line.branchTarget); it != symbolTable.end())
                {
                    line.targetSymbolName = it->second;
                }
            }

            if (line.comment.empty())
            {
                if (line.isFunctionEntry)
                {
                    if (!line.moduleName.empty())
                    {
                        line.comment = fmt::format("{}!{}", line.moduleName, line.symbolName);
                    }
                    else
                    {
                        line.comment = line.symbolName;
                    }
                }
                else if (!line.targetSymbolName.empty())
                {
                    line.comment = fmt::format("-> {}", line.targetSymbolName);
                }
                else if (!line.moduleName.empty() && !line.sectionName.empty())
                {
                    std::string prevModule{};
                    const auto lineAddr = line.address;
                    for (const auto& prev : range.lines)
                    {
                        if (prev.address >= lineAddr)
                        {
                            break;
                        }
                        prevModule = prev.moduleName;
                    }
                    if (prevModule != line.moduleName)
                    {
                        line.comment = fmt::format("[{}:{}]", line.moduleName, line.sectionName);
                    }
                }
            }
        }
    }

    void DebuggerModel::clear_cached_data()
    {
        std::scoped_lock lock{m_cacheMutex};
        m_cachedSnapshot = {};
        m_navigationAddress = 0;
        m_cachedRegisters = {};
        m_cachedDisassembly = {};
        m_cachedCallStack = {};
        m_cachedBreakpoints.clear();
        m_pendingBreakpointAdds.clear();
        m_cachedModules.clear();
        m_cachedThreads.clear();
        m_cachedImports.clear();
        m_cachedExports.clear();
        m_cachedWatchpoints.clear();
        m_cachedException = {};
        m_symbolTable.clear();
    }

    void DebuggerModel::query_xrefs_to(const std::uint64_t address, XrefResultCallback callback)
    {
        std::uint64_t moduleBase{};
        std::uint64_t moduleSize{};
        std::string moduleName{};
        std::unordered_map<std::uint64_t, std::string> symbolTableCopy{};
        std::vector<Debugger::ModuleInfo> modulesCopy{};

        {
            std::scoped_lock lock{m_cacheMutex};
            for (const auto& mod : m_cachedModules)
            {
                if (address >= mod.baseAddress && address < mod.baseAddress + mod.size)
                {
                    moduleName = mod.name;
                    moduleBase = mod.baseAddress;
                    moduleSize = mod.size;
                    break;
                }
            }
            symbolTableCopy = m_symbolTable;
            modulesCopy = m_cachedModules;
        }

        if (moduleBase == 0 || moduleSize == 0)
        {
            m_loggerService.log_warn(fmt::format("{}: Xref query for 0x{:X} - no containing module found", MODEL_NAME, address));
            wxTheApp->CallAfter([cb = std::move(callback)]() { cb({}); });
            return;
        }

        m_loggerService.log_info(fmt::format("{}: Scanning module {} (0x{:X}+0x{:X}) for xrefs to 0x{:X}",
            MODEL_NAME, moduleName, moduleBase, moduleSize, address));

        auto sharedCallback = std::make_shared<XrefResultCallback>(std::move(callback));

        std::packaged_task<StatusCode()> task(
            [this, address, moduleBase, moduleSize, sharedCallback,
             symbols = std::move(symbolTableCopy), modules = std::move(modulesCopy)]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([cb = sharedCallback]() { (*cb)({}); });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();
                std::vector<Debugger::XrefEntry> results{};
                std::uint64_t scanAddr = moduleBase;
                const std::uint64_t scanEnd = moduleBase + moduleSize;

                while (scanAddr < scanEnd)
                {
                    const auto chunkSize = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(XREF_SCAN_CHUNK_BYTES, scanEnd - scanAddr));

                    std::vector<::DisassemblerResult> resultBuffer(XREF_SCAN_CHUNK_INSTRUCTIONS);
                    ::DisassemblerResults disasmResults{};
                    disasmResults.results = resultBuffer.data();
                    disasmResults.count = 0;
                    disasmResults.capacity = static_cast<std::uint32_t>(XREF_SCAN_CHUNK_INSTRUCTIONS);
                    disasmResults.startAddress = scanAddr;

                    const auto status = Runtime::safe_call(
                        plugin.internal_vertex_process_disassemble_range,
                        scanAddr, chunkSize, &disasmResults);

                    if (!Runtime::status_ok(status) || disasmResults.count == 0)
                    {
                        scanAddr += chunkSize;
                        continue;
                    }

                    for (const auto& instr : std::span{disasmResults.results, disasmResults.count})
                    {
                        if (instr.targetAddress != address)
                        {
                            continue;
                        }

                        Debugger::XrefEntry entry{};
                        entry.address = instr.address;
                        entry.targetAddress = address;

                        if (auto it = symbols.find(instr.address); it != symbols.end())
                        {
                            entry.symbolName = it->second;
                        }

                        for (const auto& mod : modules)
                        {
                            if (instr.address >= mod.baseAddress &&
                                instr.address < mod.baseAddress + mod.size)
                            {
                                entry.moduleName = mod.name;
                                break;
                            }
                        }

                        switch (instr.branchType)
                        {
                            case VERTEX_BRANCH_CALL:
                            case VERTEX_BRANCH_INDIRECT_CALL:
                                entry.type = Debugger::XrefType::Call;
                                break;
                            case VERTEX_BRANCH_CONDITIONAL:
                                entry.type = Debugger::XrefType::ConditionalJump;
                                break;
                            case VERTEX_BRANCH_LOOP:
                                entry.type = Debugger::XrefType::Loop;
                                break;
                            default:
                                entry.type = Debugger::XrefType::UnconditionalJump;
                                break;
                        }

                        results.push_back(std::move(entry));
                    }

                    const auto& lastInstr = disasmResults.results[disasmResults.count - 1];
                    const auto nextAddr = lastInstr.address + lastInstr.size;
                    scanAddr = (nextAddr > scanAddr) ? nextAddr : scanAddr + chunkSize;
                }

                wxTheApp->CallAfter([cb = sharedCallback, res = std::move(results)]() mutable
                {
                    (*cb)(std::move(res));
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Scanner,
            Thread::DispatchPriority::Normal,
            std::move(task));

        if (!result.has_value())
        {
            m_loggerService.log_error(fmt::format("{}: Failed to dispatch xref-to scan", MODEL_NAME));
            wxTheApp->CallAfter([cb = sharedCallback]() { (*cb)({}); });
        }
    }

    void DebuggerModel::query_xrefs_from(const std::uint64_t address, XrefResultCallback callback)
    {
        std::uint64_t functionStart{};
        std::uint64_t moduleBase{};
        std::uint64_t moduleSize{};
        std::unordered_map<std::uint64_t, std::string> symbolTableCopy{};
        std::vector<Debugger::ModuleInfo> modulesCopy{};

        {
            std::scoped_lock lock{m_cacheMutex};

            for (const auto& line : m_cachedDisassembly.lines)
            {
                if (line.address == address && line.functionStart != 0)
                {
                    functionStart = line.functionStart;
                    break;
                }
            }

            for (const auto& mod : m_cachedModules)
            {
                if (address >= mod.baseAddress && address < mod.baseAddress + mod.size)
                {
                    moduleBase = mod.baseAddress;
                    moduleSize = mod.size;

                    if (functionStart == 0)
                    {
                        std::uint64_t bestSymbol{};
                        for (const auto& [symAddr, symName] : m_symbolTable)
                        {
                            if (symAddr >= mod.baseAddress && symAddr <= address && symAddr > bestSymbol)
                            {
                                bestSymbol = symAddr;
                            }
                        }
                        functionStart = bestSymbol;
                    }

                    if (functionStart == 0)
                    {
                        std::uint64_t bestExport{};
                        for (const auto& exp : m_cachedExports)
                        {
                            if (exp.address >= mod.baseAddress && exp.address <= address && exp.address > bestExport)
                            {
                                bestExport = exp.address;
                            }
                        }
                        functionStart = bestExport;
                    }

                    break;
                }
            }

            symbolTableCopy = m_symbolTable;
            modulesCopy = m_cachedModules;
        }

        if (moduleBase == 0 || moduleSize == 0)
        {
            wxTheApp->CallAfter([cb = std::move(callback)]() { cb({}); });
            return;
        }

        const bool needsProbe = (functionStart == 0);

        m_loggerService.log_info(fmt::format("{}: Scanning for outgoing xrefs from 0x{:X} (functionStart=0x{:X}, probe={})",
            MODEL_NAME, address, functionStart, needsProbe));

        auto sharedCallback = std::make_shared<XrefResultCallback>(std::move(callback));

        std::packaged_task<StatusCode()> task(
            [this, address, functionStart, moduleBase, moduleSize, needsProbe, sharedCallback,
             symbols = std::move(symbolTableCopy), modules = std::move(modulesCopy)]() -> StatusCode
            {
                auto pluginOpt = m_loaderService.get_active_plugin();
                if (!pluginOpt.has_value())
                {
                    wxTheApp->CallAfter([cb = sharedCallback]() { (*cb)({}); });
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
                }

                auto& plugin = pluginOpt.value().get();

                auto resolvedStart = functionStart;

                if (needsProbe)
                {
                    const auto probeStart = std::max(moduleBase, address - XREF_BACKWARD_PROBE_BYTES);
                    const auto probeSize = static_cast<std::uint32_t>(address - probeStart + 1);

                    std::vector<::DisassemblerResult> probeBuffer(XREF_SCAN_CHUNK_INSTRUCTIONS);
                    ::DisassemblerResults probeResults{};
                    probeResults.results = probeBuffer.data();
                    probeResults.count = 0;
                    probeResults.capacity = static_cast<std::uint32_t>(probeBuffer.size());
                    probeResults.startAddress = probeStart;

                    const auto probeStatus = Runtime::safe_call(
                        plugin.internal_vertex_process_disassemble_range,
                        probeStart, probeSize, &probeResults);

                    if (Runtime::status_ok(probeStatus) && probeResults.count > 0)
                    {
                        for (const auto& instr : std::span{probeResults.results, probeResults.count})
                        {
                            if (instr.address > address)
                            {
                                break;
                            }

                            if ((instr.flags & VERTEX_FLAG_ENTRY_POINT) != 0 ||
                                (instr.functionStart != 0 && instr.functionStart == instr.address))
                            {
                                resolvedStart = instr.address;
                            }
                        }
                    }

                    if (resolvedStart == 0)
                    {
                        resolvedStart = probeStart;
                    }
                }

                std::vector<Debugger::XrefEntry> results{};

                struct PairHash final
                {
                    std::size_t operator()(const std::pair<std::uint64_t, std::uint64_t>& p) const noexcept
                    {
                        return std::hash<std::uint64_t>{}(p.first) ^ (std::hash<std::uint64_t>{}(p.second) << 32);
                    }
                };
                std::unordered_set<std::pair<std::uint64_t, std::uint64_t>, PairHash> seenPairs{};

                const auto moduleEnd = moduleBase + moduleSize;
                auto scanCursor = resolvedStart;
                bool pastFunctionStart{};
                bool hitFunctionBoundary{};

                std::vector<::DisassemblerResult> resultBuffer(XREF_SCAN_CHUNK_INSTRUCTIONS * 2);

                while (scanCursor < moduleEnd && !hitFunctionBoundary)
                {
                    const auto chunkSize = static_cast<std::uint32_t>(
                        std::min<std::uint64_t>(XREF_SCAN_CHUNK_BYTES, moduleEnd - scanCursor));

                    if (chunkSize == 0)
                    {
                        break;
                    }

                    ::DisassemblerResults disasmResults{};
                    disasmResults.results = resultBuffer.data();
                    disasmResults.count = 0;
                    disasmResults.capacity = static_cast<std::uint32_t>(resultBuffer.size());
                    disasmResults.startAddress = scanCursor;

                    const auto status = Runtime::safe_call(
                        plugin.internal_vertex_process_disassemble_range,
                        scanCursor, chunkSize, &disasmResults);

                    if (!Runtime::status_ok(status) || disasmResults.count == 0)
                    {
                        break;
                    }

                    for (const auto& instr : std::span{disasmResults.results, disasmResults.count})
                    {
                        if (instr.address == resolvedStart)
                        {
                            pastFunctionStart = true;
                        }
                        else if (pastFunctionStart && instr.address > address &&
                                 ((instr.flags & VERTEX_FLAG_ENTRY_POINT) != 0 ||
                                  (instr.functionStart != 0 && instr.functionStart == instr.address)))
                        {
                            hitFunctionBoundary = true;
                            break;
                        }

                        if (!pastFunctionStart || instr.targetAddress == 0)
                        {
                            continue;
                        }

                        if (!seenPairs.emplace(instr.address, instr.targetAddress).second)
                        {
                            continue;
                        }

                        Debugger::XrefEntry entry{};
                        entry.address = instr.address;
                        entry.targetAddress = instr.targetAddress;

                        if (auto it = symbols.find(instr.targetAddress); it != symbols.end())
                        {
                            entry.symbolName = it->second;
                        }
                        else if (instr.targetSymbol[0] != '\0')
                        {
                            entry.symbolName = instr.targetSymbol;
                        }

                        for (const auto& mod : modules)
                        {
                            if (instr.address >= mod.baseAddress &&
                                instr.address < mod.baseAddress + mod.size)
                            {
                                entry.moduleName = mod.name;
                                break;
                            }
                        }

                        switch (instr.branchType)
                        {
                            case VERTEX_BRANCH_CALL:
                            case VERTEX_BRANCH_INDIRECT_CALL:
                                entry.type = Debugger::XrefType::Call;
                                break;
                            case VERTEX_BRANCH_CONDITIONAL:
                                entry.type = Debugger::XrefType::ConditionalJump;
                                break;
                            case VERTEX_BRANCH_LOOP:
                                entry.type = Debugger::XrefType::Loop;
                                break;
                            default:
                                entry.type = Debugger::XrefType::UnconditionalJump;
                                break;
                        }

                        results.push_back(std::move(entry));
                    }

                    const auto& lastInstr = resultBuffer[disasmResults.count - 1];
                    const auto nextAddress = lastInstr.address + lastInstr.size;

                    if (nextAddress <= scanCursor)
                    {
                        break;
                    }

                    scanCursor = nextAddress;
                }

                wxTheApp->CallAfter([cb = sharedCallback, res = std::move(results)]() mutable
                {
                    (*cb)(std::move(res));
                });

                return StatusCode::STATUS_OK;
            });

        auto result = m_dispatcher.dispatch_with_priority(
            Thread::ThreadChannel::Scanner,
            Thread::DispatchPriority::Normal,
            std::move(task));

        if (!result.has_value())
        {
            m_loggerService.log_error(fmt::format("{}: Failed to dispatch xref-from scan", MODEL_NAME));
            wxTheApp->CallAfter([cb = sharedCallback]() { (*cb)({}); });
        }
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
