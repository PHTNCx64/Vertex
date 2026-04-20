//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/debugger/engine_event.hh>
#include <vertex/debugger/idebuggerruntimeservice.hh>
#include <vertex/model/accesstrackermodel.hh>
#include <vertex/runtime/command.hh>
#include <vertex/runtime/drain.hh>
#include <vertex/runtime/lifetime_token.hh>
#include <vertex/runtime/operation_token.hh>
#include <vertex/runtime/session_epoch.hh>
#include <vertex/runtime/subscription_guard.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/viewmodel/enrichmentqueue.hh>
#include <vertex/debugger/debuggertypes.hh>
#include <vertex/log/ilog.hh>
#include <vertex/utility.hh>

#include <sdk/statuscode.h>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

namespace Vertex::ViewModel
{
    struct AccessEntry final
    {
        std::uint64_t instructionAddress {};
        std::uint32_t hitCount {};
        std::uint8_t accessSize {};
        Debugger::WatchpointType lastAccessType {Debugger::WatchpointType::ReadWrite};
        Debugger::RegisterSet lastRegisters {};
        std::vector<Debugger::StackFrame> lastCallStack {};
        std::uint64_t lastAccessValue {};
        std::uint64_t lastUpdatedGeneration {};
        std::string lastMnemonic {};
    };

    enum class TrackingStatus : std::uint8_t
    {
        Idle,
        Starting,
        Active,
        Stopping,
        StartFailed,
        StopFailed
    };

    struct TrackingState final
    {
        TrackingStatus status {TrackingStatus::Idle};
        StatusCode lastError {StatusCode::STATUS_OK};
        std::uint64_t targetAddress {};
        std::uint32_t targetSize {};
        std::uint32_t watchpointId {};
    };

    using AccessTrackerEntriesChanged = std::move_only_function<void() const>;

    struct AccessTrackerSharedState final
    {
        Debugger::IDebuggerRuntimeService& runtime;
        std::atomic<bool> disabled {false};
        Runtime::SessionEpoch sessionEpoch {};
        mutable std::mutex entriesMutex {};
        std::vector<AccessEntry> entries {};
        std::unordered_map<std::uint64_t, std::size_t> entriesByPc {};
        std::atomic<std::shared_ptr<AccessTrackerEntriesChanged>> entriesChangedCallback {};

        explicit AccessTrackerSharedState(Debugger::IDebuggerRuntimeService& r) noexcept
            : runtime(r)
        {
        }

        ~AccessTrackerSharedState() = default;

        AccessTrackerSharedState(const AccessTrackerSharedState&) = delete;
        AccessTrackerSharedState& operator=(const AccessTrackerSharedState&) = delete;
        AccessTrackerSharedState(AccessTrackerSharedState&&) = delete;
        AccessTrackerSharedState& operator=(AccessTrackerSharedState&&) = delete;
    };

    class AccessTrackerViewModel final
    {
    public:
        using TrackingStatusChanged = std::move_only_function<void(const TrackingState&) const>;
        using TrackingCompletion = std::move_only_function<void(StatusCode) const>;
        using EntriesChanged = AccessTrackerEntriesChanged;

        class [[nodiscard]] EntriesReadGuard final
        {
        public:
            [[nodiscard]] std::span<const AccessEntry> entries() const noexcept
            {
                return m_entries;
            }

            [[nodiscard]] std::size_t size() const noexcept
            {
                return m_entries.size();
            }

            [[nodiscard]] const AccessEntry& at(std::size_t index) const
            {
                return m_entries[index];
            }

            EntriesReadGuard(const EntriesReadGuard&) = delete;
            EntriesReadGuard& operator=(const EntriesReadGuard&) = delete;
            EntriesReadGuard(EntriesReadGuard&&) = delete;
            EntriesReadGuard& operator=(EntriesReadGuard&&) = delete;
            ~EntriesReadGuard() = default;

        private:
            friend class AccessTrackerViewModel;

            EntriesReadGuard(std::shared_ptr<const AccessTrackerSharedState> shared,
                             std::unique_lock<std::mutex> lock,
                             std::span<const AccessEntry> entries) noexcept
                : m_shared{std::move(shared)}
                , m_lock{std::move(lock)}
                , m_entries{entries}
            {
            }

            std::shared_ptr<const AccessTrackerSharedState> m_shared;
            std::unique_lock<std::mutex> m_lock;
            std::span<const AccessEntry> m_entries;
        };

        explicit AccessTrackerViewModel(
            std::unique_ptr<Model::AccessTrackerModel> model,
            Thread::IThreadDispatcher& dispatcher,
            Log::ILog& logService,
            std::string name = ViewModelName::ACCESS_TRACKER
        );

        ~AccessTrackerViewModel();

        AccessTrackerViewModel(const AccessTrackerViewModel&) = delete;
        AccessTrackerViewModel& operator=(const AccessTrackerViewModel&) = delete;
        AccessTrackerViewModel(AccessTrackerViewModel&&) = delete;
        AccessTrackerViewModel& operator=(AccessTrackerViewModel&&) = delete;

        void start_tracking(std::uint64_t address, std::uint32_t size, TrackingCompletion onComplete = {});
        void stop_tracking(TrackingCompletion onComplete = {});
        void clear();
        void acknowledge_start_failure();

        [[nodiscard]] TrackingState get_state() const noexcept;
        [[nodiscard]] std::vector<AccessEntry> snapshot_entries() const;
        [[nodiscard]] std::optional<AccessEntry> snapshot_entry_at(std::size_t index) const;
        [[nodiscard]] EntriesReadGuard lock_entries_for_read() const;

        void set_status_changed_callback(TrackingStatusChanged callback);
        void set_entries_changed_callback(EntriesChanged callback);

    private:
        static constexpr std::size_t MAX_PENDING_HITS {64};
        static constexpr std::chrono::milliseconds DRAIN_TIMEOUT {std::chrono::seconds{2}};

        void on_watchpoint_hit(const Debugger::WatchpointHitInfo& info);
        void on_state_changed(const Debugger::StateChangedInfo& info);
        [[nodiscard]] std::uint64_t merge_watchpoint_hit_locked(const Debugger::WatchpointHitInfo& info);
        void complete_start(std::uint64_t opId, Model::StartTrackingResult result);
        void complete_stop(std::uint64_t opId, StatusCode status);
        void implicit_stop_on_detach();
        void notify_status_changed(const TrackingState& stateSnapshot);
        void notify_entries_changed_shared();
        void marshal_to_ui(std::function<void()> fn);
        void schedule_enrichment_worker();
        static void run_enrichment_job(AccessTrackerSharedState& state, const EnrichmentJob& job);
        static void invoke_completion(TrackingCompletion completion, StatusCode status);

        std::string m_viewModelName {};
        std::unique_ptr<Model::AccessTrackerModel> m_model {};
        Thread::IThreadDispatcher& m_dispatcher;
        Log::ILog& m_logService;
        Debugger::IDebuggerRuntimeService& m_runtime;
        std::shared_ptr<AccessTrackerSharedState> m_shared;
        std::shared_ptr<EnrichmentQueue> m_enrichmentQueue {std::make_shared<EnrichmentQueue>()};

        mutable std::mutex m_stateMutex {};
        TrackingState m_state {};
        std::deque<Debugger::WatchpointHitInfo> m_pendingHits {};
        std::atomic<std::shared_ptr<TrackingStatusChanged>> m_statusChangedCallback {};

        Runtime::OperationToken<TrackingCompletion> m_operationToken {};
        Runtime::LifetimeToken m_lifetime {};
        std::shared_ptr<Runtime::BoundedDrain> m_drain {std::make_shared<Runtime::BoundedDrain>()};
        Runtime::SubscriptionGuard<Debugger::IDebuggerRuntimeService> m_watchpointHitSubscription {};
        Runtime::SubscriptionGuard<Debugger::IDebuggerRuntimeService> m_stateChangedSubscription {};
    };
}
