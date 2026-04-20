//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/viewmodel/accesstrackerviewmodel.hh>

#include <vertex/runtime/lifetime_token.hh>
#include <vertex/thread/threadchannel.hh>

#include <fmt/format.h>
#include <wx/app.h>

#include <future>
#include <span>
#include <utility>

namespace Vertex::ViewModel
{
    AccessTrackerViewModel::AccessTrackerViewModel(
        std::unique_ptr<Model::AccessTrackerModel> model,
        Thread::IThreadDispatcher& dispatcher,
        Log::ILog& logService,
        std::string name
    )
        : m_viewModelName(std::move(name))
        , m_model(std::move(model))
        , m_dispatcher(dispatcher)
        , m_logService(logService)
        , m_runtime(m_model->runtime())
        , m_shared(std::make_shared<AccessTrackerSharedState>(m_model->runtime()))
    {
        const auto watchpointSubId = m_runtime.subscribe(
            Debugger::mask_of(Debugger::EngineEventKind::WatchpointHit),
            [this, weak = m_lifetime.weak()](const Debugger::EngineEvent& event)
            {
                auto alive = weak.lock();
                if (!alive || !alive->load(std::memory_order_acquire))
                {
                    return;
                }
                if (const auto* info = std::get_if<Debugger::WatchpointHitInfo>(&event.detail))
                {
                    on_watchpoint_hit(*info);
                }
            });
        m_watchpointHitSubscription =
            Runtime::SubscriptionGuard<Debugger::IDebuggerRuntimeService> {m_runtime, watchpointSubId};

        const auto stateSubId = m_runtime.subscribe(
            Debugger::mask_of(Debugger::EngineEventKind::StateChanged),
            [this, weak = m_lifetime.weak()](const Debugger::EngineEvent& event)
            {
                auto alive = weak.lock();
                if (!alive || !alive->load(std::memory_order_acquire))
                {
                    return;
                }
                if (const auto* info = std::get_if<Debugger::StateChangedInfo>(&event.detail))
                {
                    on_state_changed(*info);
                }
            });
        m_stateChangedSubscription =
            Runtime::SubscriptionGuard<Debugger::IDebuggerRuntimeService> {m_runtime, stateSubId};
    }

    AccessTrackerViewModel::~AccessTrackerViewModel()
    {
        m_watchpointHitSubscription.reset();
        m_stateChangedSubscription.reset();

        m_shared->entriesChangedCallback.store(nullptr, std::memory_order_release);
        m_shared->disabled.store(true, std::memory_order_release);

        m_lifetime.kill();

        (void)m_shared->sessionEpoch.bump();
        m_enrichmentQueue->cancel_pending();
        const bool enrichmentDrained = m_enrichmentQueue->wait_for_drain(DRAIN_TIMEOUT);
        if (!enrichmentDrained)
        {
            m_logService.log_warn(fmt::format(
                "{}: Enrichment drain timed out after {}ms with {} worker(s) outstanding; "
                "workers will exit against shared state",
                m_viewModelName,
                static_cast<long long>(DRAIN_TIMEOUT.count()),
                m_enrichmentQueue->in_flight()));
        }

        const bool drained = m_drain->wait(DRAIN_TIMEOUT);
        if (!drained)
        {
            m_logService.log_warn(fmt::format(
                "{}: Marshal drain timed out after {}ms with {} task(s) outstanding; "
                "late callbacks no-op via lifetime token, drain survives via shared_ptr",
                m_viewModelName,
                static_cast<long long>(DRAIN_TIMEOUT.count()),
                m_drain->in_flight()));
        }

        TrackingStatus liveStatus {TrackingStatus::Idle};
        std::uint32_t liveWatchpointId {};
        std::optional<Runtime::OperationToken<TrackingCompletion>::Pending> pending;
        {
            std::scoped_lock lock {m_stateMutex};
            liveStatus = m_state.status;
            liveWatchpointId = m_state.watchpointId;
            pending = m_operationToken.consume_current_locked();
        }

        const bool hasLiveWatchpoint = liveWatchpointId != 0
            && (liveStatus == TrackingStatus::Active
                || liveStatus == TrackingStatus::Stopping
                || liveStatus == TrackingStatus::StopFailed);
        if (hasLiveWatchpoint)
        {
            (void)m_model->stop_tracking(liveWatchpointId, {});
        }

        if (pending && pending->onComplete)
        {
            invoke_completion(std::move(pending->onComplete), StatusCode::STATUS_CANCELED);
        }
    }

    void AccessTrackerViewModel::start_tracking(const std::uint64_t address,
                                                 const std::uint32_t size,
                                                 TrackingCompletion onComplete)
    {
        std::uint64_t opId {};
        TrackingState stateSnapshot {};
        std::optional<Runtime::OperationToken<TrackingCompletion>::Pending> evicted;
        bool invalidTransition {};

        {
            std::scoped_lock lock {m_stateMutex};

            if (m_state.status != TrackingStatus::Idle && m_state.status != TrackingStatus::StartFailed)
            {
                invalidTransition = true;
            }
            else
            {
                evicted = m_operationToken.bump_locked(std::move(onComplete));
                opId = m_operationToken.current_locked();

                m_state = TrackingState {
                    .status = TrackingStatus::Starting,
                    .lastError = StatusCode::STATUS_OK,
                    .targetAddress = address,
                    .targetSize = size,
                    .watchpointId = 0,
                };
                stateSnapshot = m_state;
            }
        }

        if (evicted && evicted->onComplete)
        {
            invoke_completion(std::move(evicted->onComplete), StatusCode::STATUS_CANCELED);
        }
        if (invalidTransition)
        {
            invoke_completion(std::move(onComplete), StatusCode::STATUS_ERROR_INVALID_STATE);
            return;
        }

        notify_status_changed(stateSnapshot);

        const auto cmdId = m_model->start_tracking(address, size,
            [this, opId, weak = m_lifetime.weak()](Model::StartTrackingResult result)
            {
                auto alive = weak.lock();
                if (!alive || !alive->load(std::memory_order_acquire))
                {
                    return;
                }
                marshal_to_ui([this, opId, result] { complete_start(opId, result); });
            });

        if (cmdId == Runtime::INVALID_COMMAND_ID)
        {
            complete_start(opId, Model::StartTrackingResult {
                .status = StatusCode::STATUS_SHUTDOWN,
                .watchpointId = 0,
            });
        }
    }

    void AccessTrackerViewModel::stop_tracking(TrackingCompletion onComplete)
    {
        std::uint64_t opId {};
        std::uint32_t watchpointId {};
        TrackingState stateSnapshot {};
        std::optional<Runtime::OperationToken<TrackingCompletion>::Pending> evicted;
        bool invalidTransition {};

        {
            std::scoped_lock lock {m_stateMutex};

            if (m_state.status != TrackingStatus::Active && m_state.status != TrackingStatus::StopFailed)
            {
                invalidTransition = true;
            }
            else
            {
                evicted = m_operationToken.bump_locked(std::move(onComplete));
                opId = m_operationToken.current_locked();

                watchpointId = m_state.watchpointId;
                m_state.status = TrackingStatus::Stopping;
                m_state.lastError = StatusCode::STATUS_OK;
                stateSnapshot = m_state;
            }
        }

        if (evicted && evicted->onComplete)
        {
            invoke_completion(std::move(evicted->onComplete), StatusCode::STATUS_CANCELED);
        }
        if (invalidTransition)
        {
            invoke_completion(std::move(onComplete), StatusCode::STATUS_ERROR_INVALID_STATE);
            return;
        }

        (void)m_shared->sessionEpoch.bump();
        m_enrichmentQueue->cancel_pending();
        notify_status_changed(stateSnapshot);

        const auto cmdId = m_model->stop_tracking(watchpointId,
            [this, opId, weak = m_lifetime.weak()](StatusCode status)
            {
                auto alive = weak.lock();
                if (!alive || !alive->load(std::memory_order_acquire))
                {
                    return;
                }
                marshal_to_ui([this, opId, status] { complete_stop(opId, status); });
            });

        if (cmdId == Runtime::INVALID_COMMAND_ID)
        {
            complete_stop(opId, StatusCode::STATUS_SHUTDOWN);
        }
    }

    void AccessTrackerViewModel::clear()
    {
        {
            std::scoped_lock lock {m_shared->entriesMutex};
            m_shared->entries.clear();
            m_shared->entriesByPc.clear();
        }
        (void)m_shared->sessionEpoch.bump();
        m_enrichmentQueue->cancel_pending();
        notify_entries_changed_shared();
    }

    void AccessTrackerViewModel::acknowledge_start_failure()
    {
        TrackingState snapshot {};
        std::optional<Runtime::OperationToken<TrackingCompletion>::Pending> evicted;

        {
            std::scoped_lock lock {m_stateMutex};
            if (m_state.status != TrackingStatus::StartFailed)
            {
                return;
            }

            evicted = m_operationToken.bump_locked({});
            m_state = TrackingState {};
            snapshot = m_state;
        }

        if (evicted && evicted->onComplete)
        {
            invoke_completion(std::move(evicted->onComplete), StatusCode::STATUS_CANCELED);
        }

        notify_status_changed(snapshot);
    }

    TrackingState AccessTrackerViewModel::get_state() const noexcept
    {
        std::scoped_lock lock {m_stateMutex};
        return m_state;
    }

    std::vector<AccessEntry> AccessTrackerViewModel::snapshot_entries() const
    {
        std::scoped_lock lock {m_shared->entriesMutex};
        return m_shared->entries;
    }

    std::optional<AccessEntry>
    AccessTrackerViewModel::snapshot_entry_at(const std::size_t index) const
    {
        std::scoped_lock lock {m_shared->entriesMutex};
        if (index >= m_shared->entries.size())
        {
            return std::nullopt;
        }
        return m_shared->entries[index];
    }

    AccessTrackerViewModel::EntriesReadGuard
    AccessTrackerViewModel::lock_entries_for_read() const
    {
        std::unique_lock lock {m_shared->entriesMutex};
        std::span<const AccessEntry> view {m_shared->entries};
        return EntriesReadGuard {m_shared, std::move(lock), view};
    }

    void AccessTrackerViewModel::set_status_changed_callback(TrackingStatusChanged callback)
    {
        m_statusChangedCallback.store(
            callback ? std::make_shared<TrackingStatusChanged>(std::move(callback)) : nullptr,
            std::memory_order_release);

        TrackingState snapshot {};
        {
            std::scoped_lock lock {m_stateMutex};
            snapshot = m_state;
        }

        notify_status_changed(snapshot);
    }

    void AccessTrackerViewModel::set_entries_changed_callback(EntriesChanged callback)
    {
        m_shared->entriesChangedCallback.store(
            callback ? std::make_shared<EntriesChanged>(std::move(callback)) : nullptr,
            std::memory_order_release);

        notify_entries_changed_shared();
    }

    void AccessTrackerViewModel::on_watchpoint_hit(const Debugger::WatchpointHitInfo& info)
    {
        std::uint64_t capturedEpoch {};
        bool shouldMerge {};
        {
            std::scoped_lock lock {m_stateMutex};
            if (m_state.status == TrackingStatus::Starting)
            {
                if (m_pendingHits.size() >= MAX_PENDING_HITS)
                {
                    m_pendingHits.pop_front();
                }
                m_pendingHits.push_back(info);
                return;
            }

            if (m_state.status != TrackingStatus::Active || info.watchpointId != m_state.watchpointId)
            {
                return;
            }

            capturedEpoch = m_shared->sessionEpoch.load();
            shouldMerge = true;
        }

        if (!shouldMerge)
        {
            return;
        }

        std::uint64_t generation {};
        bool merged {};
        {
            std::scoped_lock lock {m_shared->entriesMutex};
            if (m_shared->sessionEpoch.load() != capturedEpoch)
            {
                return;
            }
            generation = merge_watchpoint_hit_locked(info);
            merged = true;
        }

        if (merged)
        {
            m_enrichmentQueue->enqueue(EnrichmentJob {
                .pc = info.instructionAddress,
                .threadId = info.threadId,
                .sessionEpoch = capturedEpoch,
                .requestGeneration = generation,
            });
            schedule_enrichment_worker();
            notify_entries_changed_shared();
        }
    }

    void AccessTrackerViewModel::on_state_changed(const Debugger::StateChangedInfo& info)
    {
        if (info.current != Debugger::DebuggerState::Detached)
        {
            return;
        }

        TrackingStatus status {TrackingStatus::Idle};
        {
            std::scoped_lock lock {m_stateMutex};
            status = m_state.status;
        }

        if (status == TrackingStatus::Active
            || status == TrackingStatus::Starting
            || status == TrackingStatus::Stopping
            || status == TrackingStatus::StopFailed)
        {
            marshal_to_ui([this] { implicit_stop_on_detach(); });
        }
    }

    void AccessTrackerViewModel::implicit_stop_on_detach()
    {
        TrackingState snapshot {};
        std::optional<Runtime::OperationToken<TrackingCompletion>::Pending> evicted;

        {
            std::scoped_lock lock {m_stateMutex};
            if (m_state.status == TrackingStatus::Idle)
            {
                return;
            }
            evicted = m_operationToken.bump_locked({});
            m_state = TrackingState {};
            snapshot = m_state;
        }

        (void)m_shared->sessionEpoch.bump();
        m_enrichmentQueue->cancel_pending();

        if (evicted && evicted->onComplete)
        {
            invoke_completion(std::move(evicted->onComplete), StatusCode::STATUS_CANCELED);
        }

        notify_status_changed(snapshot);
    }

    std::uint64_t AccessTrackerViewModel::merge_watchpoint_hit_locked(const Debugger::WatchpointHitInfo& info)
    {
        auto& entries = m_shared->entries;
        auto& byPc = m_shared->entriesByPc;

        if (const auto it = byPc.find(info.instructionAddress); it != byPc.end())
        {
            auto& existing = entries[it->second];
            ++existing.hitCount;
            existing.accessSize = info.accessSize;
            existing.lastAccessType = info.accessType;
            ++existing.lastUpdatedGeneration;
            return existing.lastUpdatedGeneration;
        }

        byPc.emplace(info.instructionAddress, entries.size());
        entries.push_back(AccessEntry {
            .instructionAddress = info.instructionAddress,
            .hitCount = 1,
            .accessSize = info.accessSize,
            .lastAccessType = info.accessType,
            .lastUpdatedGeneration = 1,
        });
        return 1;
    }

    void AccessTrackerViewModel::complete_start(const std::uint64_t opId,
                                                 const Model::StartTrackingResult result)
    {
        TrackingCompletion completion {};
        TrackingState stateSnapshot {};
        bool shouldNotify {};
        bool shouldNotifyEntries {};
        std::vector<Debugger::WatchpointHitInfo> pendingHitsToMerge {};
        std::uint64_t capturedEpoch {};
        StatusCode terminalStatus = result.status;

        {
            std::scoped_lock lock {m_stateMutex};
            auto consumed = m_operationToken.consume_locked(opId);
            if (!consumed)
            {
                return;
            }
            completion = std::move(consumed->onComplete);

            const auto trackedAddress = m_state.targetAddress;
            const auto trackedSize = m_state.targetSize;

            if (result.status == StatusCode::STATUS_OK && result.watchpointId == 0)
            {
                m_logService.log_error(fmt::format(
                    "{}: start_tracking OK with watchpointId=0 at 0x{:X}; treating as failure",
                    m_viewModelName, trackedAddress));

                m_pendingHits.clear();
                m_state = TrackingState {
                    .status = TrackingStatus::StartFailed,
                    .lastError = StatusCode::STATUS_ERROR_GENERAL,
                    .targetAddress = trackedAddress,
                    .targetSize = trackedSize,
                    .watchpointId = 0,
                };
                terminalStatus = StatusCode::STATUS_ERROR_GENERAL;
            }
            else if (result.status == StatusCode::STATUS_OK)
            {
                pendingHitsToMerge.reserve(m_pendingHits.size());
                for (const auto& pendingHit : m_pendingHits)
                {
                    if (pendingHit.watchpointId == result.watchpointId)
                    {
                        pendingHitsToMerge.push_back(pendingHit);
                    }
                }
                m_pendingHits.clear();

                m_state = TrackingState {
                    .status = TrackingStatus::Active,
                    .lastError = StatusCode::STATUS_OK,
                    .targetAddress = trackedAddress,
                    .targetSize = trackedSize,
                    .watchpointId = result.watchpointId,
                };
                capturedEpoch = m_shared->sessionEpoch.load();
            }
            else
            {
                m_logService.log_error(fmt::format("{}: Failed to start tracking at 0x{:X} (status={})",
                    m_viewModelName, trackedAddress, static_cast<int>(result.status)));

                m_pendingHits.clear();
                m_state = TrackingState {
                    .status = TrackingStatus::StartFailed,
                    .lastError = result.status,
                    .targetAddress = trackedAddress,
                    .targetSize = trackedSize,
                    .watchpointId = 0,
                };
            }

            stateSnapshot = m_state;
            shouldNotify = true;
        }

        std::vector<EnrichmentJob> enrichmentJobs {};
        if (!pendingHitsToMerge.empty())
        {
            std::scoped_lock entriesLock {m_shared->entriesMutex};
            if (m_shared->sessionEpoch.load() == capturedEpoch)
            {
                enrichmentJobs.reserve(pendingHitsToMerge.size());
                for (const auto& pendingHit : pendingHitsToMerge)
                {
                    const auto gen = merge_watchpoint_hit_locked(pendingHit);
                    enrichmentJobs.push_back(EnrichmentJob {
                        .pc = pendingHit.instructionAddress,
                        .threadId = pendingHit.threadId,
                        .sessionEpoch = capturedEpoch,
                        .requestGeneration = gen,
                    });
                    shouldNotifyEntries = true;
                }
            }
        }

        if (!enrichmentJobs.empty())
        {
            for (const auto& job : enrichmentJobs)
            {
                m_enrichmentQueue->enqueue(job);
            }
            schedule_enrichment_worker();
        }

        if (shouldNotify)
        {
            notify_status_changed(stateSnapshot);
        }
        if (shouldNotifyEntries)
        {
            notify_entries_changed_shared();
        }
        if (completion)
        {
            invoke_completion(std::move(completion), terminalStatus);
        }
    }

    void AccessTrackerViewModel::complete_stop(const std::uint64_t opId, const StatusCode status)
    {
        TrackingCompletion completion {};
        TrackingState stateSnapshot {};
        bool shouldNotify {};

        {
            std::scoped_lock lock {m_stateMutex};
            auto consumed = m_operationToken.consume_locked(opId);
            if (!consumed)
            {
                return;
            }
            completion = std::move(consumed->onComplete);

            const auto trackedAddress = m_state.targetAddress;
            const auto trackedSize = m_state.targetSize;
            const auto watchpointId = m_state.watchpointId;

            if (status == StatusCode::STATUS_OK)
            {
                m_state = TrackingState {};
            }
            else
            {
                m_logService.log_error(fmt::format("{}: Failed to stop tracking at 0x{:X} (status={})",
                    m_viewModelName, trackedAddress, static_cast<int>(status)));

                m_state = TrackingState {
                    .status = TrackingStatus::StopFailed,
                    .lastError = status,
                    .targetAddress = trackedAddress,
                    .targetSize = trackedSize,
                    .watchpointId = watchpointId,
                };
            }

            stateSnapshot = m_state;
            shouldNotify = true;
        }

        if (shouldNotify)
        {
            notify_status_changed(stateSnapshot);
        }
        if (completion)
        {
            invoke_completion(std::move(completion), status);
        }
    }

    void AccessTrackerViewModel::notify_status_changed(const TrackingState& stateSnapshot)
    {
        const auto callback = m_statusChangedCallback.load(std::memory_order_acquire);
        if (callback && *callback)
        {
            (*callback)(stateSnapshot);
        }
    }

    void AccessTrackerViewModel::notify_entries_changed_shared()
    {
        const auto callback = m_shared->entriesChangedCallback.load(std::memory_order_acquire);
        if (callback && *callback)
        {
            (*callback)();
        }
    }

    void AccessTrackerViewModel::marshal_to_ui(std::function<void()> fn)
    {
        m_drain->acquire();
        if (!wxTheApp)
        {
            fn();
            m_drain->release();
            return;
        }
        wxTheApp->CallAfter(
            [fn = std::move(fn), weak = m_lifetime.weak(), drain = m_drain]() mutable
            {
                struct Release final
                {
                    Runtime::BoundedDrain& drain;
                    ~Release() { drain.release(); }
                } release {*drain};
                auto alive = weak.lock();
                if (!alive || !alive->load(std::memory_order_acquire))
                {
                    return;
                }
                fn();
            });
    }

    void AccessTrackerViewModel::invoke_completion(TrackingCompletion completion, const StatusCode status)
    {
        if (completion)
        {
            completion(status);
        }
    }

    void AccessTrackerViewModel::schedule_enrichment_worker()
    {
        if (m_enrichmentQueue->pending_size() == 0)
        {
            return;
        }
        if (m_enrichmentQueue->in_flight() >= EnrichmentQueue::MAX_IN_FLIGHT)
        {
            return;
        }

        auto queue = m_enrichmentQueue;
        auto shared = m_shared;
        std::packaged_task<StatusCode()> task {
            [queue = std::move(queue), shared = std::move(shared)]() mutable -> StatusCode
            {
                while (auto job = queue->pop_next())
                {
                    struct Completer final
                    {
                        EnrichmentQueue& q;
                        ~Completer() { q.complete_job(); }
                    } completer {*queue};

                    if (shared->disabled.load(std::memory_order_acquire))
                    {
                        continue;
                    }
                    run_enrichment_job(*shared, *job);
                }
                return StatusCode::STATUS_OK;
            }
        };

        (void)m_dispatcher.dispatch_fire_and_forget(
            Thread::ThreadChannel::Script, std::move(task));
    }

    void AccessTrackerViewModel::run_enrichment_job(AccessTrackerSharedState& state,
                                                     const EnrichmentJob& job)
    {
        if (state.disabled.load(std::memory_order_acquire))
        {
            return;
        }
        if (job.sessionEpoch != state.sessionEpoch.load())
        {
            return;
        }

        auto registers = state.runtime.snapshot_registers(job.threadId);
        auto callStack = state.runtime.snapshot_call_stack(job.threadId);
        auto disassembly = state.runtime.disassemble_one(job.pc);

        bool changed {};
        {
            std::scoped_lock lock {state.entriesMutex};
            if (state.disabled.load(std::memory_order_acquire))
            {
                return;
            }
            if (job.sessionEpoch != state.sessionEpoch.load())
            {
                return;
            }
            const auto it = state.entriesByPc.find(job.pc);
            if (it == state.entriesByPc.end())
            {
                return;
            }
            auto& entry = state.entries[it->second];
            if (job.requestGeneration != entry.lastUpdatedGeneration)
            {
                return;
            }

            if (registers.has_value())
            {
                entry.lastRegisters = std::move(*registers);
                changed = true;
            }
            if (!callStack.empty())
            {
                entry.lastCallStack = std::move(callStack);
                changed = true;
            }
            if (disassembly.has_value())
            {
                entry.lastMnemonic = disassembly->mnemonic;
                if (!disassembly->operands.empty())
                {
                    entry.lastMnemonic.append(" ");
                    entry.lastMnemonic.append(disassembly->operands);
                }
                changed = true;
            }
        }

        if (changed)
        {
            const auto callback = state.entriesChangedCallback.load(std::memory_order_acquire);
            if (callback && *callback)
            {
                (*callback)();
            }
        }
    }
}
