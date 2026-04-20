//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/scanner/scannerruntimeservice.hh>

#include <fmt/format.h>

#include <cassert>
#include <functional>
#include <utility>
#include <vector>

namespace Vertex::Scanner
{
    namespace
    {
        constexpr auto BUILTIN_VALUE_TYPES = std::to_array<ValueType>({
            ValueType::Int8,
            ValueType::Int16,
            ValueType::Int32,
            ValueType::Int64,
            ValueType::UInt8,
            ValueType::UInt16,
            ValueType::UInt32,
            ValueType::UInt64,
            ValueType::Float,
            ValueType::Double,
            ValueType::StringASCII,
            ValueType::StringUTF8,
            ValueType::StringUTF16,
            ValueType::StringUTF32,
        });
    }

    ScannerRuntimeService::ScannerRuntimeService(IMemoryScanner& scanner,
                                                   Thread::IThreadDispatcher& dispatcher,
                                                   Log::ILog& log)
        : m_scanner{scanner}
        , m_dispatcher{dispatcher}
        , m_log{log}
    {
        register_builtin_types();

        m_scanner.set_scan_completion_callback([this] { on_scan_complete(); });
        m_scanner.set_scan_progress_callback([this] { on_scan_progress(); });

        const auto handle = m_dispatcher.schedule_recurring_persistent(
            Thread::ThreadChannel::Scanner,
            Thread::DispatchPriority::Low,
            Thread::RecurringPolicy::FixedDelay,
            REAP_INTERVAL,
            [this] { return reap_timeouts(); },
            Thread::RecurringFailurePolicy::Continue);

        if (handle.has_value())
        {
            m_timeoutReaperHandle = *handle;
            m_reaperActive = true;
            return;
        }

        m_log.log_error(fmt::format("ScannerRuntimeService: persistent timeout reaper schedule failed (status={})",
                                     static_cast<int>(handle.error())));
    }

    ScannerRuntimeService::~ScannerRuntimeService()
    {
        shutdown();
    }

    Runtime::CommandId ScannerRuntimeService::allocate_command_id()
    {
        return m_nextCommandId.fetch_add(1, std::memory_order_relaxed);
    }

    ScannerRuntimeService::ResultChannelPtr
    ScannerRuntimeService::register_pending(Runtime::CommandId id,
                                              std::chrono::milliseconds timeout)
    {
        const auto now = std::chrono::steady_clock::now();
        auto channel = std::make_shared<Runtime::ResultChannel<service::CommandResult>>();
        PendingResult pending{.id = id,
                              .deadline = now + timeout,
                              .evictAfter = {},
                              .channel = channel,
                              .completed = false};

        std::scoped_lock lock{m_pendingMutex};
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return {};
        }
        m_pending.emplace(id, std::move(pending));
        return channel;
    }

    void ScannerRuntimeService::synthesise_rejection(Runtime::CommandId id,
                                                      StatusCode code,
                                                      service::CommandResultPayload payload)
    {
        post_result(id, code, std::move(payload));
    }

    void ScannerRuntimeService::post_result(Runtime::CommandId id,
                                             StatusCode code,
                                             service::CommandResultPayload payload)
    {
        service::CommandResult result{.id = id, .code = code, .payload = std::move(payload)};
        on_scanner_command_result(std::move(result));
    }

    void ScannerRuntimeService::register_builtin_types()
    {
        for (const auto valueType : BUILTIN_VALUE_TYPES)
        {
            const auto schemaPtr = make_builtin_schema(valueType);
            m_registry.install_builtin(*schemaPtr);
        }
    }

    std::shared_ptr<const TypeSchema> ScannerRuntimeService::resolve_schema(TypeId id) const
    {
        auto schema = m_registry.find(id);
        if (!schema.has_value())
        {
            return {};
        }
        return std::make_shared<const TypeSchema>(std::move(*schema));
    }

    Runtime::CommandId
    ScannerRuntimeService::send_command(service::Command command,
                                         std::chrono::milliseconds timeout)
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        return std::visit(
            [&]<class TCommand>(TCommand&& cmd) -> Runtime::CommandId
            {
                using Decayed = std::decay_t<TCommand>;
                if constexpr (std::is_same_v<Decayed, service::CmdStartScan>)
                {
                    return dispatch_start_scan(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdNextScan>)
                {
                    return dispatch_next_scan(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdUndoScan>)
                {
                    return dispatch_undo_scan(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdStopScan>)
                {
                    return dispatch_stop_scan(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdCancel>)
                {
                    return dispatch_cancel(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdRefreshValues>)
                {
                    return dispatch_refresh_values(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdRegisterType>)
                {
                    return dispatch_register_type(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdUnregisterType>)
                {
                    return dispatch_unregister_type(std::forward<TCommand>(cmd), timeout);
                }
                else if constexpr (std::is_same_v<Decayed, service::CmdQueryTypes>)
                {
                    return dispatch_query_types(std::forward<TCommand>(cmd), timeout);
                }
                else
                {
                    static_assert(!sizeof(Decayed*), "unhandled scanner command");
                }
            },
            std::move(command));
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_start_scan(service::CmdStartScan command, const std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        Runtime::CommandId expected = Runtime::INVALID_COMMAND_ID;
        if (!m_activeScanId.compare_exchange_strong(expected, id, std::memory_order_acq_rel))
        {
            synthesise_rejection(id, STATUS_ERROR_THREAD_IS_BUSY);
            return id;
        }

        auto schema = resolve_schema(command.config.typeId);
        if (!schema)
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, STATUS_ERROR_GENERAL_NOT_FOUND);
            return id;
        }

        if (schema->kind == TypeKind::PluginDefined &&
            (!schema->sdkType ||
             command.config.scanMode >= schema->sdkType->scanModeCount))
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, STATUS_ERROR_INVALID_PARAMETER);
            return id;
        }

        m_activeScanTypeId.store(schema->id, std::memory_order_release);
        m_activeScanStart.store(std::chrono::steady_clock::now(), std::memory_order_release);

        const auto status = m_scanner.initialize_scan(command.config, schema, command.regions);
        if (status != STATUS_OK)
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            m_activeScanTypeId.store(TypeId::Invalid, std::memory_order_release);
            synthesise_rejection(id, status);
            return id;
        }
        m_backendScanActive.store(true, std::memory_order_release);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_next_scan(service::CmdNextScan command,
                                                std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        Runtime::CommandId expected = Runtime::INVALID_COMMAND_ID;
        if (!m_activeScanId.compare_exchange_strong(expected, id, std::memory_order_acq_rel))
        {
            synthesise_rejection(id, STATUS_ERROR_THREAD_IS_BUSY);
            return id;
        }

        auto schema = resolve_schema(command.config.typeId);
        if (!schema)
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, STATUS_ERROR_GENERAL_NOT_FOUND);
            return id;
        }

        const auto priorTypeId = m_activeScanTypeId.load(std::memory_order_acquire);
        if (priorTypeId != TypeId::Invalid && priorTypeId != schema->id)
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, STATUS_ERROR_INVALID_PARAMETER);
            return id;
        }

        if (schema->kind == TypeKind::PluginDefined &&
            (!schema->sdkType ||
             command.config.scanMode >= schema->sdkType->scanModeCount))
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, STATUS_ERROR_INVALID_PARAMETER);
            return id;
        }

        m_activeScanTypeId.store(schema->id, std::memory_order_release);
        m_activeScanStart.store(std::chrono::steady_clock::now(), std::memory_order_release);

        const auto status = m_scanner.initialize_next_scan(command.config, schema);
        if (status != STATUS_OK)
        {
            m_activeScanId.store(Runtime::INVALID_COMMAND_ID, std::memory_order_release);
            synthesise_rejection(id, status);
            return id;
        }
        m_backendScanActive.store(true, std::memory_order_release);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_undo_scan(service::CmdUndoScan /*command*/,
                                                std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        const auto status = m_scanner.undo_scan();
        post_result(id, status);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_stop_scan(service::CmdStopScan /*command*/,
                                                std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        const auto status = m_scanner.stop_scan();
        post_result(id, status);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_cancel(service::CmdCancel command,
                                            std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        const auto activeScan = m_activeScanId.load(std::memory_order_acquire);
        if (command.target == Runtime::INVALID_COMMAND_ID || command.target != activeScan)
        {
            synthesise_rejection(id, STATUS_ERROR_GENERAL_NOT_FOUND);
            return id;
        }

        m_scanner.set_scan_abort_state(true);

        ScannerEvent event{.kind = ScannerEventKind::Cancelled,
                           .detail = CancelledInfo{.cancelledId = command.target}};
        m_fanout.fire(event);

        post_result(id, STATUS_OK);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_refresh_values(service::CmdRefreshValues /*command*/,
                                                     std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        ScannerEvent event{.kind = ScannerEventKind::ValuesChanged,
                           .detail = ValuesChangedInfo{
                               .generation = 0,
                               .changedCount = static_cast<std::uint32_t>(m_scanner.get_results_count())}};
        m_fanout.fire(event);

        post_result(id, STATUS_OK);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_register_type(service::CmdRegisterType command,
                                                    std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        if (!command.sdkType || !command.sdkType->typeName)
        {
            synthesise_rejection(id, STATUS_ERROR_INVALID_PARAMETER);
            return id;
        }

        TypeSchema schema{};
        schema.name = command.sdkType->typeName;
        schema.kind = TypeKind::PluginDefined;
        schema.valueSize = static_cast<std::uint32_t>(command.sdkType->valueSize);
        schema.sdkType = command.sdkType;
        schema.sourcePluginIndex = command.sourcePluginIndex;
        schema.libraryKeepalive = command.libraryKeepalive;

        bool duplicate = false;
        const auto allocatedId = m_registry.register_type(schema, duplicate);
        if (duplicate)
        {
            synthesise_rejection(id, STATUS_ERROR_GENERAL_ALREADY_EXISTS);
            return id;
        }
        schema.id = allocatedId;

        ScannerEvent event{.kind = ScannerEventKind::TypeRegistered,
                           .detail = TypeRegisteredInfo{.schema = schema}};
        m_fanout.fire(event);

        post_result(id,
                    STATUS_OK,
                    service::RegisterTypeResultPayload{.id = allocatedId});
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_unregister_type(service::CmdUnregisterType command,
                                                     std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        bool builtin = false;
        auto removed = m_registry.unregister_type(command.id, builtin);
        if (builtin)
        {
            synthesise_rejection(id, STATUS_ERROR_INVALID_PARAMETER);
            return id;
        }
        if (!removed.has_value())
        {
            synthesise_rejection(id, STATUS_ERROR_GENERAL_NOT_FOUND);
            return id;
        }
        const std::string removedName = removed->name;

        if (m_activeScanTypeId.load(std::memory_order_acquire) == command.id)
        {
            const auto claimed = m_activeScanId.exchange(Runtime::INVALID_COMMAND_ID,
                                                          std::memory_order_acq_rel);
            if (claimed != Runtime::INVALID_COMMAND_ID)
            {
                m_activeScanTypeId.store(TypeId::Invalid, std::memory_order_release);
                m_scanner.set_scan_abort_state(true);
                if (try_complete_pending(claimed, STATUS_ERROR_GENERAL_NOT_FOUND))
                {
                    ScannerEvent cancel{.kind = ScannerEventKind::Cancelled,
                                        .detail = CancelledInfo{.cancelledId = claimed}};
                    m_fanout.fire(cancel);
                }
            }
        }

        ScannerEvent event{.kind = ScannerEventKind::TypeUnregistered,
                           .detail = TypeUnregisteredInfo{.id = command.id, .name = removedName}};
        m_fanout.fire(event);

        post_result(id, STATUS_OK);
        return id;
    }

    Runtime::CommandId
    ScannerRuntimeService::dispatch_query_types(service::CmdQueryTypes /*command*/,
                                                  std::chrono::milliseconds timeout)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id, timeout))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        service::QueryTypesResultPayload payload{.types = m_registry.snapshot()};
        post_result(id, STATUS_OK, std::move(payload));
        return id;
    }

    void ScannerRuntimeService::subscribe_result(Runtime::CommandId id, ResultCallback callback)
    {
        if (id == Runtime::INVALID_COMMAND_ID || !callback)
        {
            return;
        }

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            service::CommandResult timeout{.id = id, .code = STATUS_TIMEOUT};
            callback(timeout);
            return;
        }

        channel->on_result(
            [cb = std::move(callback)](const service::CommandResult& result)
            {
                cb(result);
            });
    }

    service::CommandResult
    ScannerRuntimeService::await_result(Runtime::CommandId id, std::chrono::milliseconds timeout)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return service::CommandResult{.id = id, .code = STATUS_ERROR_INVALID_PARAMETER};
        }

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            channel = find_channel_locked(id);
        }

        if (!channel)
        {
            return service::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }

        if (!channel->wait_for(timeout))
        {
            return service::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }

        auto result = channel->copy_result();
        if (result.has_value())
        {
            return std::move(*result);
        }
        return service::CommandResult{.id = id, .code = STATUS_TIMEOUT};
    }

    Runtime::SubscriptionId
    ScannerRuntimeService::subscribe(ScannerEventKindMask mask, EventCallback callback)
    {
        return m_fanout.subscribe(mask, std::move(callback));
    }

    void ScannerRuntimeService::unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept
    {
        m_fanout.unsubscribe(subscriptionId);
    }

    std::uint64_t ScannerRuntimeService::results_count() const
    {
        return m_scanner.get_results_count();
    }

    StatusCode ScannerRuntimeService::snapshot_results(std::vector<IMemoryScanner::ScanResultEntry>& out,
                                                        std::size_t startIndex,
                                                        std::size_t count) const
    {
        return m_scanner.get_scan_results_range(out, startIndex, count);
    }

    bool ScannerRuntimeService::can_undo() const
    {
        return m_scanner.can_undo();
    }

    bool ScannerRuntimeService::is_scanning() const
    {
        return m_backendScanActive.load(std::memory_order_acquire) ||
               m_activeScanId.load(std::memory_order_acquire) != Runtime::INVALID_COMMAND_ID;
    }

    std::optional<TypeSchema> ScannerRuntimeService::find_type(TypeId id) const
    {
        return m_registry.find(id);
    }

    std::vector<TypeSchema> ScannerRuntimeService::list_types() const
    {
        return m_registry.snapshot();
    }

    std::uint32_t ScannerRuntimeService::invalidate_plugin_types(std::size_t pluginIndex)
    {
        auto removed = m_registry.invalidate_plugin_types(pluginIndex);

        if (removed.empty())
        {
            return 0;
        }

        const auto activeType = m_activeScanTypeId.load(std::memory_order_acquire);
        const auto typeWasRemoved = std::ranges::any_of(removed,
                                 [activeType](const TypeSchema& s) { return s.id == activeType; });

        Runtime::CommandId claimedScan{Runtime::INVALID_COMMAND_ID};
        if (typeWasRemoved)
        {
            claimedScan = m_activeScanId.exchange(Runtime::INVALID_COMMAND_ID,
                                                    std::memory_order_acq_rel);
            if (claimedScan != Runtime::INVALID_COMMAND_ID)
            {
                m_activeScanTypeId.store(TypeId::Invalid, std::memory_order_release);
                m_scanner.set_scan_abort_state(true);
                if (try_complete_pending(claimedScan, STATUS_CANCELED))
                {
                    ScannerEvent cancel{.kind = ScannerEventKind::Cancelled,
                                        .detail = CancelledInfo{.cancelledId = claimedScan}};
                    m_fanout.fire(cancel);
                }
            }
        }
        const bool cancelsActiveScan = claimedScan != Runtime::INVALID_COMMAND_ID;
        const auto activeScan = claimedScan;

        for (const auto& schema : removed)
        {
            ScannerEvent event{.kind = ScannerEventKind::TypeUnregistered,
                               .detail = TypeUnregisteredInfo{.id = schema.id, .name = schema.name}};
            m_fanout.fire(event);
        }

        ScannerEvent aggregate{
            .kind = ScannerEventKind::RegistryInvalidated,
            .detail = RegistryInvalidatedInfo{.sourcePluginIndex = pluginIndex,
                                               .removedCount = static_cast<std::uint32_t>(removed.size())}};
        m_fanout.fire(aggregate);

        if (cancelsActiveScan)
        {
            const auto deadline = std::chrono::steady_clock::now() + PLUGIN_UNLOAD_DRAIN_TIMEOUT;
            while (std::chrono::steady_clock::now() < deadline)
            {
                if (!m_backendScanActive.load(std::memory_order_acquire))
                {
                    break;
                }
                std::this_thread::sleep_for(PLUGIN_UNLOAD_DRAIN_POLL);
            }
            if (m_backendScanActive.load(std::memory_order_acquire))
            {
                m_log.log_warn(fmt::format(
                    "[Scanner] plugin unload drain timed out (pluginIndex={}, outstanding scan id={})",
                    pluginIndex,
                    static_cast<std::uint64_t>(activeScan)));
            }
        }

        return static_cast<std::uint32_t>(removed.size());
    }

    void ScannerRuntimeService::shutdown()
    {
        bool expected = false;
        if (!m_shuttingDown.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
        {
            return;
        }

        m_scanner.set_scan_completion_callback({});
        m_scanner.set_scan_progress_callback({});

        if (m_reaperActive)
        {
            const auto code = m_dispatcher.cancel_recurring(m_timeoutReaperHandle);
            if (code != STATUS_OK)
            {
                m_log.log_warn("ScannerRuntimeService: cancel_recurring failed");
            }
            m_reaperActive = false;
        }

        m_fanout.shutdown();
        drain_pending_on_shutdown();
    }

    void ScannerRuntimeService::on_scanner_event(ScannerEvent event)
    {
        m_fanout.fire(event);
    }

    bool ScannerRuntimeService::try_complete_pending(Runtime::CommandId id,
                                                       StatusCode code,
                                                       service::CommandResultPayload payload)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return false;
        }

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            const auto it = m_pending.find(id);
            if (it == m_pending.end() || it->second.completed)
            {
                return false;
            }
            complete_locked(it->second);
            channel = it->second.channel;
        }

        if (channel)
        {
            service::CommandResult result{.id = id, .code = code, .payload = std::move(payload)};
            (void) channel->post(std::move(result));
        }
        return true;
    }

    void ScannerRuntimeService::on_scanner_command_result(service::CommandResult result)
    {
        const auto id = result.id;
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return;
        }

        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            const auto it = m_pending.find(id);
            if (it == m_pending.end() || it->second.completed)
            {
                return;
            }
            complete_locked(it->second);
            channel = it->second.channel;
        }

        if (channel)
        {
            (void) channel->post(std::move(result));
        }
    }

    ScannerRuntimeService::ResultChannelPtr
    ScannerRuntimeService::find_channel_locked(Runtime::CommandId id) const
    {
        const auto it = m_pending.find(id);
        if (it == m_pending.end())
        {
            return {};
        }
        return it->second.channel;
    }

    void ScannerRuntimeService::complete_locked(PendingResult& pending)
    {
        pending.completed = true;
        pending.evictAfter = std::chrono::steady_clock::now() + COMPLETED_GRACE;
    }

    StatusCode ScannerRuntimeService::reap_timeouts()
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return STATUS_OK;
        }

        const auto now = std::chrono::steady_clock::now();
        std::vector<std::pair<ResultChannelPtr, service::CommandResult>> firedTimeouts{};

        {
            std::scoped_lock lock{m_pendingMutex};
            for (auto it = m_pending.begin(); it != m_pending.end();)
            {
                auto& entry = it->second;
                if (entry.completed)
                {
                    if (now >= entry.evictAfter)
                    {
                        it = m_pending.erase(it);
                        continue;
                    }
                }
                else if (now >= entry.deadline)
                {
                    service::CommandResult timeout{.id = entry.id, .code = STATUS_TIMEOUT};
                    complete_locked(entry);
                    firedTimeouts.emplace_back(entry.channel, std::move(timeout));
                }
                ++it;
            }
        }

        for (auto& [channel, result] : firedTimeouts)
        {
            if (channel)
            {
                (void) channel->post(std::move(result));
            }
        }
        return STATUS_OK;
    }

    void ScannerRuntimeService::drain_pending_on_shutdown()
    {
        std::vector<std::pair<ResultChannelPtr, service::CommandResult>> firedShutdowns{};

        {
            std::scoped_lock lock{m_pendingMutex};
            for (auto& [id, entry] : m_pending)
            {
                if (!entry.completed)
                {
                    service::CommandResult shutdownResult{.id = id, .code = STATUS_SHUTDOWN};
                    complete_locked(entry);
                    firedShutdowns.emplace_back(entry.channel, std::move(shutdownResult));
                }
            }
            m_pending.clear();
        }

        for (auto& [channel, result] : firedShutdowns)
        {
            if (channel)
            {
                (void) channel->post(std::move(result));
            }
        }
    }

    void ScannerRuntimeService::on_scan_complete()
    {
        const auto id = m_activeScanId.exchange(Runtime::INVALID_COMMAND_ID, std::memory_order_acq_rel);
        m_backendScanActive.store(false, std::memory_order_release);
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return;
        }

        const auto typeId = m_activeScanTypeId.load(std::memory_order_acquire);
        const auto start = m_activeScanStart.load(std::memory_order_acquire);
        const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now() - start);
        const auto matchCount = m_scanner.get_results_count();
        const auto pluginError = m_scanner.get_last_plugin_error();

        if (pluginError != STATUS_OK)
        {
            if (try_complete_pending(id, pluginError))
            {
                ScannerEvent errorEvent{.kind = ScannerEventKind::ScanError,
                                         .detail = ScanErrorInfo{.code = pluginError,
                                                                  .description = "plugin compute callback failed"}};
                m_fanout.fire(errorEvent);
            }
            return;
        }

        if (try_complete_pending(id,
                                  STATUS_OK,
                                  service::StartScanResultPayload{.matchCount = matchCount, .elapsed = elapsed}))
        {
            ScannerEvent event{.kind = ScannerEventKind::ScanComplete,
                               .detail = ScanCompleteInfo{.matchCount = matchCount,
                                                           .elapsed = elapsed,
                                                           .typeId = typeId}};
            m_fanout.fire(event);
        }
    }

    void ScannerRuntimeService::on_scan_progress()
    {
        const auto scanned = m_scanner.get_regions_scanned();
        const auto total = m_scanner.get_total_regions();
        const auto percent = (total > 0)
            ? static_cast<std::uint8_t>(std::min<std::uint64_t>(100, (scanned * 100) / total))
            : std::uint8_t{0};

        ScannerEvent event{.kind = ScannerEventKind::ScanProgress,
                           .detail = ScanProgressInfo{.percentComplete = percent,
                                                       .addressesScanned = scanned,
                                                       .matchesSoFar = m_scanner.get_results_count()}};
        m_fanout.fire(event);
    }
}
