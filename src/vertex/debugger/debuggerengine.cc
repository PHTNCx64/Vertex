//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerengine.hh>
#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>
#include <vertex/thread/threadchannel.hh>

#include <sdk/debugger.h>

#include <sdk/feature.h>

#include <fmt/format.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <optional>
#include <span>
#include <vector>

#include <wx/app.h>

namespace Vertex::Debugger
{
    static constexpr std::uint32_t MAX_COMMAND_BURST {32};

    namespace
    {
        [[nodiscard]] constexpr DebuggerState engine_to_debugger_state(EngineState state) noexcept
        {
            switch (state)
            {
                case EngineState::Idle:     return DebuggerState::Detached;
                case EngineState::Detached: return DebuggerState::Detached;
                case EngineState::Running:  return DebuggerState::Running;
                case EngineState::Paused:   return DebuggerState::Paused;
                case EngineState::Exited:   return DebuggerState::Detached;
                case EngineState::Stopped:  return DebuggerState::Detached;
            }
            std::unreachable();
        }
    }

    DebuggerEngine::DebuggerEngine(Runtime::ILoader& loader, Thread::IThreadDispatcher& dispatcher, Log::ILog& logger)
        : m_loader(loader),
          m_dispatcher(dispatcher),
          m_loggerService(logger)
    {
    }

    DebuggerEngine::~DebuggerEngine()
    {
        m_uiLifetime->store(false, std::memory_order_release);

        if (m_running.load(std::memory_order_acquire))
        {
            [[maybe_unused]] const auto _ = stop();
        }

        if (m_stopFuture.valid())
        {
            static constexpr auto SHUTDOWN_TIMEOUT = std::chrono::seconds {5};
            const std::future_status status = m_stopFuture.wait_for(SHUTDOWN_TIMEOUT);
            if (status == std::future_status::timeout)
            {
                m_loggerService.log_error(fmt::format("{}: Timeout while waiting for shutdown to complete", ENGINE_NAME));
            }
        }
    }

    StatusCode DebuggerEngine::start()
    {
        bool expected {};
        if (!m_running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_ALREADY_RUNNING;
        }

        auto* plugin = get_plugin();
        if (plugin == nullptr)
        {
            m_running.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        m_isSingleThreadMode = m_dispatcher.is_single_threaded();
        m_isDebuggerDependent = plugin->has_feature(VERTEX_FEATURE_DEBUGGER_DEPENDENT);

        ::DebuggerCallbacks callbacks {};
        callbacks.on_breakpoint_hit = on_breakpoint_hit;
        callbacks.on_single_step = on_single_step;
        callbacks.on_exception = on_exception;
        callbacks.on_watchpoint_hit = on_watchpoint_hit;
        callbacks.on_thread_created = on_thread_created;
        callbacks.on_thread_exited = on_thread_exited;
        callbacks.on_module_loaded = on_module_loaded;
        callbacks.on_module_unloaded = on_module_unloaded;
        callbacks.on_process_exited = on_process_exited;
        callbacks.on_output_string = on_output_string;
        callbacks.on_error = on_error;
        callbacks.user_data = this;

        const auto setResult = Runtime::safe_call(
            plugin->internal_vertex_debugger_set_callbacks, &callbacks);
        if (!Runtime::status_ok(setResult))
        {
            m_running.store(false, std::memory_order_release);
            return Runtime::get_status(setResult);
        }

        if (!transition_state(EngineState::Detached))
        {
            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Detached)));
        }

        auto scheduleResult = m_dispatcher.schedule_recurring(
            Thread::ThreadChannel::Debugger,
            Thread::DispatchPriority::High,
            Thread::RecurringPolicy::AsSoonAsPossible,
            std::chrono::milliseconds{0},
            [this]() -> StatusCode
            {
                return tick_once();
            });

        if (!scheduleResult.has_value())
        {
            [[maybe_unused]] const auto clearResult =
                Runtime::safe_call(plugin->internal_vertex_debugger_set_callbacks, nullptr);
            if (!transition_state(EngineState::Idle))
            {
                m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Idle)));
            }
            m_running.store(false, std::memory_order_release);
            return scheduleResult.error();
        }

        m_pumpHandle = *scheduleResult;
        return StatusCode::STATUS_OK;
    }

    StatusCode DebuggerEngine::stop()
    {
        if (!m_running.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_RUNNING;
        }

        m_commandQueue.enqueue(engine::CmdShutdown{});

        [[maybe_unused]] const auto cancelResult =
            m_dispatcher.cancel_recurring(m_pumpHandle);
        m_pumpHandle = {};

        std::packaged_task<StatusCode()> finalTask(
            [this]() -> StatusCode
            {
                drain_all_commands();
                flush_events();
                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.dispatch(
            Thread::ThreadChannel::Debugger, std::move(finalTask));
        if (!dispatchResult.has_value()) [[unlikely]]
        {
            m_running.store(false, std::memory_order_release);
            return dispatchResult.error();
        }

        m_stopFuture = std::move(*dispatchResult);
        m_running.store(false, std::memory_order_release);
        return StatusCode::STATUS_OK;
    }

    void DebuggerEngine::send_command(const EngineCommand cmd)
    {
        m_commandQueue.enqueue(cmd);
        m_wakeSignal.release();
    }

    void DebuggerEngine::enqueue_service_command(Runtime::CommandId id, service::Command cmd)
    {
        m_serviceCommandQueue.enqueue(ServiceCommandRequest {.id = id, .command = std::move(cmd)});
        m_wakeSignal.release();
    }

    EngineSnapshot DebuggerEngine::get_snapshot() const
    {
        std::scoped_lock lock {m_snapshotMutex};
        return m_snapshot;
    }

    std::uint64_t DebuggerEngine::get_generation() const noexcept
    {
        return m_generation.load(std::memory_order_acquire);
    }

    void DebuggerEngine::set_event_callback(EngineEventCallback callback)
    {
        std::scoped_lock lock {m_callbackMutex};
        m_eventCallback = std::move(callback);
    }

    void DebuggerEngine::set_runtime_service(IDebuggerRuntimeService* service) noexcept
    {
        m_runtimeService.store(service, std::memory_order_release);
    }

    void DebuggerEngine::set_tick_timeout(const std::uint32_t activeMs, const std::uint32_t parkedMs)
    {
        m_activeTimeoutMs = activeMs;
        m_parkedTimeoutMs = parkedMs;
    }

    std::optional<EngineError> DebuggerEngine::consume_last_error()
    {
        std::scoped_lock lock {m_snapshotMutex};
        const auto error = m_lastError;
        m_lastError.reset();
        return error;
    }

    std::vector<LogEntry> DebuggerEngine::consume_log_entries()
    {
        std::scoped_lock lock {m_logMutex};
        auto entries = std::move(m_pendingLogs);
        m_pendingLogs.clear();
        return entries;
    }

    void DebuggerEngine::post_error(const std::string_view operation, const StatusCode code)
    {
        const auto stateAtError = m_state.load(std::memory_order_acquire);
        {
            std::scoped_lock lock {m_snapshotMutex};
            m_lastError = EngineError {
                .operation = operation,
                .code = code,
                .stateAtError = stateAtError
            };
        }

        m_loggerService.log_error(fmt::format(
            "{}: {} failed (code={}, state={})",
            ENGINE_NAME, operation, static_cast<int>(code), to_string_view(stateAtError)));

        m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }

    StatusCode DebuggerEngine::tick_once()
    {
        drain_commands();
        drain_service_commands();

        const auto currentState = m_state.load(std::memory_order_acquire);
        if (currentState == EngineState::Detached || currentState == EngineState::Stopped)
        {
            flush_events();
            std::ignore = m_wakeSignal.try_acquire_for(std::chrono::milliseconds{m_parkedTimeoutMs});
            return StatusCode::STATUS_OK;
        }

        auto* plugin = get_plugin();
        if (plugin == nullptr) [[unlikely]]
        {
            if (!transition_state(EngineState::Detached))
            {
                m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Detached)));
            }
            m_dirtyFlags.fetch_or(static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
            flush_events();
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        auto timeout = is_parked_state(currentState) ? m_parkedTimeoutMs : m_activeTimeoutMs;
        if (m_isSingleThreadMode && m_isDebuggerDependent)
        {
            timeout = std::min(timeout, m_singleThreadTickClampMs);
        }

        const auto tickResult = Runtime::safe_call(plugin->internal_vertex_debugger_tick, timeout);
        if (tickResult.has_value())
        {
            process_tick_result(*tickResult);
        }
        else
        {
            if (!transition_state(EngineState::Detached))
            {
                m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Detached)));
            }
            m_dirtyFlags.fetch_or(
                static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
            post_error("tick", StatusCode::STATUS_DEBUG_TICK_ERROR);
        }

        flush_events();
        return StatusCode::STATUS_OK;
    }

    void DebuggerEngine::drain_commands()
    {
        auto* plugin = get_plugin();

        EngineCommand cmd {};
        for (std::uint32_t i {}; i < MAX_COMMAND_BURST && m_commandQueue.try_dequeue(cmd); ++i)
        {
            if (plugin == nullptr) [[unlikely]]
            {
                if (std::holds_alternative<engine::CmdShutdown>(cmd))
                {
                    if (!transition_state(EngineState::Stopped))
                    {
                        m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Stopped)));
                    }
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                continue;
            }
            execute_command(plugin, cmd);
        }
    }

    void DebuggerEngine::drain_all_commands()
    {
        auto* plugin = get_plugin();

        EngineCommand cmd {};
        while (m_commandQueue.try_dequeue(cmd))
        {
            if (plugin == nullptr) [[unlikely]]
            {
                if (std::holds_alternative<engine::CmdShutdown>(cmd))
                {
                    if (!transition_state(EngineState::Stopped))
                    {
                        m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Stopped)));
                    }
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                continue;
            }
            execute_command(plugin, cmd);
        }

        drain_all_service_commands();
    }

    void DebuggerEngine::drain_service_commands()
    {
        auto* plugin = get_plugin();

        ServiceCommandRequest request {};
        for (std::uint32_t i {}; i < MAX_COMMAND_BURST && m_serviceCommandQueue.try_dequeue(request); ++i)
        {
            if (plugin == nullptr) [[unlikely]]
            {
                post_service_result(request.id, StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED);
                continue;
            }
            execute_service_command(plugin, request);
        }
    }

    void DebuggerEngine::drain_all_service_commands()
    {
        auto* plugin = get_plugin();

        ServiceCommandRequest request {};
        while (m_serviceCommandQueue.try_dequeue(request))
        {
            if (plugin == nullptr) [[unlikely]]
            {
                post_service_result(request.id, StatusCode::STATUS_SHUTDOWN);
                continue;
            }
            execute_service_command(plugin, request);
        }
    }

    void DebuggerEngine::post_service_result(Runtime::CommandId id,
                                              StatusCode status,
                                              service::CommandResultPayload payload)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return;
        }
        auto* service = m_runtimeService.load(std::memory_order_acquire);
        if (service == nullptr)
        {
            return;
        }
        service->on_engine_command_result(service::CommandResult {
            .id = id,
            .code = status,
            .payload = std::move(payload),
        });
    }

    void DebuggerEngine::execute_service_command(Runtime::Plugin* plugin,
                                                  const ServiceCommandRequest& request)
    {
        const auto commandId = request.id;
        std::visit(
            [this, plugin, commandId]<class T>(const T& arg)
            {
                using Cmd = std::decay_t<T>;

                if constexpr (std::is_same_v<Cmd, service::CmdAddWatchpoint>)
                {
                    ::Watchpoint wp {};
                    wp.address = arg.address;
                    wp.size = arg.size;
                    wp.active = true;
                    switch (arg.type)
                    {
                        case WatchpointType::Read:      wp.type = VERTEX_WP_READ; break;
                        case WatchpointType::Write:     wp.type = VERTEX_WP_WRITE; break;
                        case WatchpointType::Execute:   wp.type = VERTEX_WP_EXECUTE; break;
                        case WatchpointType::ReadWrite:
                        default:                        wp.type = VERTEX_WP_READWRITE; break;
                    }

                    std::uint32_t watchpointId {};
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_set_watchpoint, &wp, &watchpointId);
                    const auto status = Runtime::get_status(result);
                    if (status != StatusCode::STATUS_OK)
                    {
                        post_service_result(commandId, status);
                        return;
                    }

                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::Watchpoints), std::memory_order_relaxed);
                    post_service_result(commandId, StatusCode::STATUS_OK,
                        service::AddWatchpointResultPayload {.watchpointId = watchpointId});
                }
                else if constexpr (std::is_same_v<Cmd, service::CmdRemoveWatchpoint>)
                {
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_remove_watchpoint, arg.id);
                    const auto status = Runtime::get_status(result);
                    if (status == StatusCode::STATUS_OK)
                    {
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::Watchpoints), std::memory_order_relaxed);
                    }
                    post_service_result(commandId, status);
                }
                else if constexpr (std::is_same_v<Cmd, service::CmdReadRegisters>)
                {
                    ::RegisterSet sdkRegs {};
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_get_registers, arg.threadId, &sdkRegs);
                    const auto status = Runtime::get_status(result);
                    if (status != StatusCode::STATUS_OK)
                    {
                        post_service_result(commandId, status);
                        return;
                    }

                    RegisterSet registerSet {};
                    registerSet.instructionPointer = sdkRegs.instructionPointer;
                    registerSet.stackPointer = sdkRegs.stackPointer;
                    registerSet.basePointer = sdkRegs.basePointer;

                    const auto count = std::min<std::uint32_t>(sdkRegs.registerCount, VERTEX_MAX_REGISTERS);
                    for (const auto& sdkReg : std::span {sdkRegs.registers, count})
                    {
                        Register reg {};
                        reg.name = sdkReg.name;
                        reg.value = sdkReg.value;
                        reg.previousValue = sdkReg.previousValue;
                        reg.bitWidth = sdkReg.bitWidth;
                        reg.modified = sdkReg.modified != 0;
                        switch (sdkReg.category)
                        {
                            case VERTEX_REG_SEGMENT:
                                reg.category = RegisterCategory::Segment;
                                registerSet.segment.push_back(std::move(reg));
                                break;
                            case VERTEX_REG_FLAGS:
                                reg.category = RegisterCategory::Flags;
                                registerSet.flags.push_back(std::move(reg));
                                break;
                            case VERTEX_REG_FLOATING_POINT:
                                reg.category = RegisterCategory::FloatingPoint;
                                registerSet.floatingPoint.push_back(std::move(reg));
                                break;
                            case VERTEX_REG_VECTOR:
                                reg.category = RegisterCategory::Vector;
                                registerSet.vector.push_back(std::move(reg));
                                break;
                            case VERTEX_REG_GENERAL:
                            default:
                                reg.category = RegisterCategory::General;
                                registerSet.generalPurpose.push_back(std::move(reg));
                                break;
                        }
                    }

                    post_service_result(commandId, StatusCode::STATUS_OK,
                        service::RegisterSnapshotPayload {
                            .registers = std::move(registerSet),
                            .engineGeneration = m_generation.load(std::memory_order_acquire),
                        });
                }
                else if constexpr (std::is_same_v<Cmd, service::CmdReadCallStack>)
                {
                    ::CallStack sdkCallStack {};
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_get_call_stack, arg.threadId, &sdkCallStack);
                    const auto status = Runtime::get_status(result);
                    if (status != StatusCode::STATUS_OK)
                    {
                        post_service_result(commandId, status);
                        return;
                    }

                    std::vector<StackFrame> frames {};
                    const auto count = std::min<std::uint32_t>(sdkCallStack.frameCount, VERTEX_MAX_STACK_FRAMES);
                    frames.reserve(count);
                    for (const auto& sdkFrame : std::span {sdkCallStack.frames, count})
                    {
                        StackFrame frame {};
                        frame.frameIndex = sdkFrame.frameIndex;
                        frame.returnAddress = sdkFrame.returnAddress;
                        frame.framePointer = sdkFrame.framePointer;
                        frame.stackPointer = sdkFrame.stackPointer;
                        frame.functionName = sdkFrame.functionName;
                        frame.moduleName = sdkFrame.moduleName;
                        frame.sourceFile = sdkFrame.sourceFile;
                        frame.sourceLine = sdkFrame.sourceLine;
                        frames.push_back(std::move(frame));
                    }

                    post_service_result(commandId, StatusCode::STATUS_OK,
                        service::CallStackSnapshotPayload {
                            .frames = std::move(frames),
                            .engineGeneration = m_generation.load(std::memory_order_acquire),
                        });
                }
                else if constexpr (std::is_same_v<Cmd, service::CmdDisassemble>)
                {
                    static constexpr std::size_t MAX_INSTRUCTIONS {16};
                    static constexpr std::uint32_t DISASM_BYTE_WINDOW {64};
                    std::array<::DisassemblerResult, MAX_INSTRUCTIONS> buffer {};
                    ::DisassemblerResults sdkResults {};
                    sdkResults.results = buffer.data();
                    sdkResults.count = 0;
                    sdkResults.capacity = static_cast<std::uint32_t>(buffer.size());
                    sdkResults.startAddress = arg.address;

                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_process_disassemble_range,
                        arg.address, DISASM_BYTE_WINDOW, &sdkResults);
                    const auto status = Runtime::get_status(result);
                    if (status != StatusCode::STATUS_OK || sdkResults.count == 0)
                    {
                        post_service_result(commandId,
                            status == StatusCode::STATUS_OK ? StatusCode::STATUS_ERROR_NO_VALUES_FOUND : status);
                        return;
                    }

                    const auto take = std::min<std::uint32_t>(sdkResults.count, arg.instructionCount);
                    std::vector<DisassemblyLine> lines {};
                    lines.reserve(take);
                    for (const auto& instr : std::span {sdkResults.results, take})
                    {
                        DisassemblyLine line {};
                        line.address = instr.address;
                        line.bytes.assign(instr.rawBytes, instr.rawBytes + instr.size);
                        line.mnemonic = instr.mnemonic;
                        line.operands = instr.operands;
                        line.comment = instr.comment;
                        line.sectionName = instr.sectionName;
                        line.targetSymbolName = instr.targetSymbol;
                        line.functionStart = instr.functionStart;
                        line.branchTarget = instr.targetAddress != 0
                            ? std::optional<std::uint64_t> {instr.targetAddress}
                            : std::nullopt;
                        lines.push_back(std::move(line));
                    }

                    post_service_result(commandId, StatusCode::STATUS_OK,
                        service::DisassemblyPayload {
                            .lines = std::move(lines),
                            .engineGeneration = m_generation.load(std::memory_order_acquire),
                        });
                }
                else
                {
                    post_service_result(commandId, StatusCode::STATUS_ERROR_NOT_IMPLEMENTED);
                }
            },
            request.command);
    }

    void DebuggerEngine::execute_command(Runtime::Plugin* plugin, const EngineCommand& cmd)
    {
        std::visit(
            [this, plugin]<class T>(T&& arg)
            {
                using Cmd = std::decay_t<T>;

                if constexpr (std::is_same_v<Cmd, engine::CmdAttach>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Detached)
                    {
                        post_error("attach", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_attach);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("attach", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdDetach>)
                {
                    const auto currentState = m_state.load(std::memory_order_acquire);
                    if (currentState == EngineState::Detached || currentState == EngineState::Idle)
                    {
                        return;
                    }
                    const auto detachResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                    if (Runtime::status_ok(detachResult))
                    {
                        if (!transition_state(EngineState::Detached))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Detached)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("detach", Runtime::get_status(detachResult));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdContinue>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("continue", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_continue, arg.passException);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("continue", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdPause>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Running)
                    {
                        post_error("pause", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto pauseResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_pause);
                    if (!Runtime::status_ok(pauseResult))
                    {
                        post_error("pause", Runtime::get_status(pauseResult));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepInto>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_into", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_INTO);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_into", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepOver>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_over", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_OVER);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_over", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdStepOut>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("step_out", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_step, VERTEX_STEP_OUT);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("step_out", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdRunToAddress>)
                {
                    if (m_state.load(std::memory_order_acquire) != EngineState::Paused)
                    {
                        post_error("run_to_address", StatusCode::STATUS_ERROR_INVALID_STATE);
                        return;
                    }
                    const auto result = Runtime::safe_call(
                        plugin->internal_vertex_debugger_run_to_address, arg.address);
                    if (Runtime::status_ok(result))
                    {
                        if (!transition_state(EngineState::Running))
                        {
                            m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Running)));
                        }
                        m_dirtyFlags.fetch_or(
                            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                    }
                    else
                    {
                        post_error("run_to_address", Runtime::get_status(result));
                    }
                }
                else if constexpr (std::is_same_v<Cmd, engine::CmdShutdown>)
                {
                    const auto currentState = m_state.load(std::memory_order_acquire);

                    const auto clearResult =
                        Runtime::safe_call(plugin->internal_vertex_debugger_set_callbacks, nullptr);
                    if (!Runtime::status_ok(clearResult))
                    {
                        post_error("shutdown_clear_callbacks", Runtime::get_status(clearResult));
                    }

                    if (currentState != EngineState::Detached &&
                        currentState != EngineState::Idle &&
                        currentState != EngineState::Stopped)
                    {
                        const auto detachResult =
                            Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                        if (!Runtime::status_ok(detachResult))
                        {
                            post_error("shutdown_detach", Runtime::get_status(detachResult));
                        }
                    }
                    if (!transition_state(EngineState::Stopped))
                    {
                        m_loggerService.log_warn(fmt::format("{}: transition to {} was no-op", ENGINE_NAME, to_string_view(EngineState::Stopped)));
                    }
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
            },
            cmd);
    }

    void DebuggerEngine::process_tick_result(const StatusCode tickResult)
    {
        switch (tickResult)
        {
            case StatusCode::STATUS_DEBUG_TICK_NO_EVENT:
            case StatusCode::STATUS_DEBUG_TICK_PROCESSED:
                break;

            case StatusCode::STATUS_DEBUG_TICK_PAUSED:
            {
                if (transition_state(EngineState::Paused))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
                }
                break;
            }

            case StatusCode::STATUS_DEBUG_TICK_PROCESS_EXITED:
            {
                if (transition_state(EngineState::Exited))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }

            case StatusCode::STATUS_DEBUG_TICK_DETACHED:
            {
                if (transition_state(EngineState::Detached))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }

            case StatusCode::STATUS_DEBUG_TICK_ERROR:
            default:
            {
                if (transition_state(EngineState::Detached))
                {
                    m_dirtyFlags.fetch_or(
                        static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
                }
                break;
            }
        }
    }

    bool DebuggerEngine::transition_state(const EngineState newState)
    {
        const auto previous = m_state.exchange(newState, std::memory_order_acq_rel);
        if (previous == newState)
        {
            return false;
        }

        const auto gen = m_generation.fetch_add(1, std::memory_order_acq_rel) + 1;

        {
            std::scoped_lock lock {m_snapshotMutex};
            m_snapshot.state = newState;
            m_snapshot.generation = gen;

            if (newState != EngineState::Paused)
            {
                m_snapshot.hasException = false;
                m_snapshot.exceptionCode = 0;
                m_snapshot.exceptionFirstChance = false;
                m_snapshot.lastWatchpointId = 0;
                m_snapshot.lastWatchpointAddress = 0;
                m_snapshot.lastWatchpointAccessorAddress = 0;
                m_snapshot.lastWatchpointAccessType = WatchpointType::ReadWrite;
                m_snapshot.lastWatchpointAccessSize = 0;
            }
        }

        if (auto* service = m_runtimeService.load(std::memory_order_acquire))
        {
            EngineEvent event {
                .kind = EngineEventKind::StateChanged,
                .detail = StateChangedInfo {
                    .previous = engine_to_debugger_state(previous),
                    .current = engine_to_debugger_state(newState),
                    .pid = std::nullopt
                }
            };
            service->on_engine_event(std::move(event));
        }

        return true;
    }

    void DebuggerEngine::flush_events()
    {
        const auto rawFlags = m_dirtyFlags.exchange(0, std::memory_order_acq_rel);
        if (rawFlags == 0)
        {
            return;
        }

        m_pendingUiFlags.fetch_or(rawFlags, std::memory_order_acq_rel);

        bool expected {};
        if (!m_uiFlushPending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        std::scoped_lock lock {m_callbackMutex};
        if (m_eventCallback && wxTheApp)
        {
            auto callback = m_eventCallback;
            auto uiLifetime = m_uiLifetime;
            wxTheApp->CallAfter(
                [this, callback, uiLifetime]()
                {
                    if (!uiLifetime->load(std::memory_order_acquire))
                    {
                        return;
                    }

                    const auto accumulatedFlags = static_cast<DirtyFlags>(
                        m_pendingUiFlags.exchange(0, std::memory_order_acq_rel));

                    EngineSnapshot snapshot {};
                    {
                        std::scoped_lock snapshotLock {m_snapshotMutex};
                        snapshot = m_snapshot;
                    }

                    m_uiFlushPending.store(false, std::memory_order_release);
                    callback(accumulatedFlags, snapshot);
                });
        }
        else
        {
            m_uiFlushPending.store(false, std::memory_order_release);
        }
    }

    Runtime::Plugin* DebuggerEngine::get_plugin() const
    {
        if (m_loader.has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return nullptr;
        }

        const auto pluginOpt = m_loader.get_active_plugin();
        if (!pluginOpt.has_value())
        {
            return nullptr;
        }

        return &pluginOpt.value().get();
    }

    bool DebuggerEngine::is_parked_state(const EngineState state) noexcept
    {
        return state == EngineState::Paused || state == EngineState::Exited;
    }

    void DebuggerEngine::on_breakpoint_hit(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
        }

        LogEntry entry{};
        entry.level = LogLevel::Info;
        entry.threadId = event->threadId;
        entry.source = "engine.breakpoint";
        entry.timestamp = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        if (event->description[0] != '\0')
        {
            entry.message = event->description;
        }
        else
        {
            entry.message = fmt::format("Breakpoint hit at 0x{:X}", event->address);
        }
        {
            std::scoped_lock lock{engine->m_logMutex};
            engine->m_pendingLogs.push_back(std::move(entry));
        }

        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Breakpoints | DirtyFlags::State | DirtyFlags::Logs),
            std::memory_order_relaxed);
    }

    void DebuggerEngine::on_single_step(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State | DirtyFlags::Registers | DirtyFlags::CallStack),
            std::memory_order_relaxed);
    }

    void DebuggerEngine::on_exception(const ::DebugEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->address;
            engine->m_snapshot.currentThreadId = event->threadId;
            engine->m_snapshot.exceptionCode = event->exceptionCode;
            engine->m_snapshot.exceptionFirstChance = event->firstChance != 0;
            engine->m_snapshot.hasException = true;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::State), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_watchpoint_hit(const ::WatchpointEvent* event, void* userData)
    {
        if (event == nullptr || userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        WatchpointType accessType {WatchpointType::ReadWrite};
        switch (event->type)
        {
            case VERTEX_WP_READ:      accessType = WatchpointType::Read;      break;
            case VERTEX_WP_WRITE:     accessType = WatchpointType::Write;     break;
            case VERTEX_WP_EXECUTE:   accessType = WatchpointType::Execute;   break;
            case VERTEX_WP_READWRITE:
            default:                  accessType = WatchpointType::ReadWrite; break;
        }
        {
            std::scoped_lock lock {engine->m_snapshotMutex};
            engine->m_snapshot.currentAddress = event->accessAddress;
            engine->m_snapshot.currentThreadId = event->threadId;
            engine->m_snapshot.lastWatchpointId = event->breakpointId;
            engine->m_snapshot.lastWatchpointAddress = event->address;
            engine->m_snapshot.lastWatchpointAccessorAddress = event->accessAddress;
            engine->m_snapshot.lastWatchpointAccessType = accessType;
            engine->m_snapshot.lastWatchpointAccessSize = event->size;
        }
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Watchpoints | DirtyFlags::State),
            std::memory_order_relaxed);

        if (auto* service = engine->m_runtimeService.load(std::memory_order_acquire))
        {
            EngineEvent evt {
                .kind = EngineEventKind::WatchpointHit,
                .detail = WatchpointHitInfo {
                    .watchpointId = event->breakpointId,
                    .threadId = event->threadId,
                    .accessAddress = event->accessAddress,
                    .instructionAddress = event->address,
                    .accessType = accessType,
                    .accessSize = event->size
                }
            };
            service->on_engine_event(std::move(evt));
        }
    }

    void DebuggerEngine::on_thread_created([[maybe_unused]] const ::ThreadEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Threads), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_thread_exited([[maybe_unused]] const ::ThreadEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Threads), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_module_loaded([[maybe_unused]] const ::ModuleEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Modules), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_module_unloaded([[maybe_unused]] const ::ModuleEvent* event, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Modules), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_process_exited([[maybe_unused]] const std::int32_t exitCode, void* userData)
    {
        if (userData == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);
        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::All), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_output_string(const ::OutputStringEvent* event, void* userData)
    {
        if (userData == nullptr || event == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);

        LogEntry entry{};
        entry.level = LogLevel::Output;
        entry.message = event->message;
        entry.threadId = event->threadId;
        entry.source = "engine.output";
        entry.timestamp = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        {
            std::scoped_lock lock{engine->m_logMutex};
            engine->m_pendingLogs.push_back(std::move(entry));
        }

        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Logs), std::memory_order_relaxed);
    }

    void DebuggerEngine::on_error(const ::DebuggerError* error, void* userData)
    {
        if (userData == nullptr || error == nullptr)
        {
            return;
        }
        auto* engine = static_cast<DebuggerEngine*>(userData);

        LogEntry entry{};
        entry.level = LogLevel::Error;
        entry.message = fmt::format("{} (code={})", error->message, static_cast<int>(error->code));
        entry.threadId = 0;
        entry.source = "engine.error";
        entry.timestamp = static_cast<std::uint64_t>(std::chrono::system_clock::now().time_since_epoch().count());
        {
            std::scoped_lock lock{engine->m_logMutex};
            engine->m_pendingLogs.push_back(std::move(entry));
        }

        engine->m_dirtyFlags.fetch_or(
            static_cast<std::uint32_t>(DirtyFlags::Logs | DirtyFlags::State), std::memory_order_relaxed);
    }
}
