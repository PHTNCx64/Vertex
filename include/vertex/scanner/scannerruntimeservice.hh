//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/log/ilog.hh>
#include <vertex/runtime/command.hh>
#include <vertex/runtime/fanout.hh>
#include <vertex/runtime/result_channel.hh>
#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanner_command.hh>
#include <vertex/scanner/scanner_event.hh>
#include <vertex/scanner/scanner_typeschema.hh>
#include <vertex/scanner/type_registry.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <atomic>
#include <chrono>
#include <cstdint>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <unordered_map>
#include <vector>

namespace Vertex::Scanner
{
    class ScannerRuntimeService final : public IScannerRuntimeService
    {
      public:
        static constexpr std::chrono::milliseconds REAP_INTERVAL{100};
        static constexpr std::chrono::milliseconds COMPLETED_GRACE{1000};
        static constexpr std::chrono::milliseconds PLUGIN_UNLOAD_DRAIN_TIMEOUT{2000};
        static constexpr std::chrono::milliseconds PLUGIN_UNLOAD_DRAIN_POLL{20};

        ScannerRuntimeService(IMemoryScanner& scanner,
                              Thread::IThreadDispatcher& dispatcher,
                              Log::ILog& log);
        ~ScannerRuntimeService() override;

        [[nodiscard]] Runtime::CommandId
        send_command(service::Command command,
                     std::chrono::milliseconds timeout) override;

        void subscribe_result(Runtime::CommandId id, ResultCallback callback) override;

        [[nodiscard]] service::CommandResult
        await_result(Runtime::CommandId id, std::chrono::milliseconds timeout) override;

        [[nodiscard]] Runtime::SubscriptionId
        subscribe(ScannerEventKindMask mask, EventCallback callback) override;

        void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept override;

        [[nodiscard]] std::uint64_t results_count() const override;
        [[nodiscard]] StatusCode snapshot_results(std::vector<IMemoryScanner::ScanResultEntry>& out,
                                                  std::size_t startIndex,
                                                  std::size_t count) const override;
        [[nodiscard]] bool can_undo() const override;
        [[nodiscard]] bool is_scanning() const override;

        [[nodiscard]] std::optional<TypeSchema> find_type(TypeId id) const override;
        [[nodiscard]] std::vector<TypeSchema> list_types() const override;
        [[nodiscard]] std::uint32_t invalidate_plugin_types(std::size_t pluginIndex) override;

        void on_scanner_event(ScannerEvent event) override;
        void on_scanner_command_result(service::CommandResult result) override;

        void shutdown() override;

      private:
        using ResultChannelPtr = std::shared_ptr<Runtime::ResultChannel<service::CommandResult>>;

        struct PendingResult final
        {
            Runtime::CommandId id{Runtime::INVALID_COMMAND_ID};
            std::chrono::steady_clock::time_point deadline{};
            std::chrono::steady_clock::time_point evictAfter{};
            ResultChannelPtr channel{};
            bool completed{false};
        };

        [[nodiscard]] Runtime::CommandId allocate_command_id();
        [[nodiscard]] ResultChannelPtr register_pending(Runtime::CommandId id,
                                                        std::chrono::milliseconds timeout);
        void synthesise_rejection(Runtime::CommandId id,
                                  StatusCode code,
                                  service::CommandResultPayload payload = {});
        void post_result(Runtime::CommandId id,
                         StatusCode code,
                         service::CommandResultPayload payload = {});

        void register_builtin_types();
        [[nodiscard]] std::shared_ptr<const TypeSchema> resolve_schema(TypeId id) const;

        Runtime::CommandId dispatch_start_scan(service::CmdStartScan command,
                                                std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_next_scan(service::CmdNextScan command,
                                               std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_undo_scan(service::CmdUndoScan command,
                                               std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_stop_scan(service::CmdStopScan command,
                                               std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_cancel(service::CmdCancel command,
                                            std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_refresh_values(service::CmdRefreshValues command,
                                                    std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_register_type(service::CmdRegisterType command,
                                                    std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_unregister_type(service::CmdUnregisterType command,
                                                     std::chrono::milliseconds timeout);
        Runtime::CommandId dispatch_query_types(service::CmdQueryTypes command,
                                                 std::chrono::milliseconds timeout);

        [[nodiscard]] ResultChannelPtr find_channel_locked(Runtime::CommandId id) const;
        void complete_locked(PendingResult& pending);
        [[nodiscard]] bool try_complete_pending(Runtime::CommandId id,
                                                  StatusCode code,
                                                  service::CommandResultPayload payload = {});
        StatusCode reap_timeouts();
        void drain_pending_on_shutdown();

      public:
        void on_scan_complete();
        void on_scan_progress();

      private:

        IMemoryScanner& m_scanner;
        Thread::IThreadDispatcher& m_dispatcher;
        Log::ILog& m_log;

        Runtime::Fanout<ScannerEventKind, ScannerEvent> m_fanout{};

        mutable std::mutex m_pendingMutex{};
        std::unordered_map<Runtime::CommandId, PendingResult> m_pending{};

        std::atomic<Runtime::CommandId> m_nextCommandId{1};
        std::atomic<bool> m_shuttingDown{false};

        TypeRegistry m_registry{};

        std::atomic<Runtime::CommandId> m_activeScanId{Runtime::INVALID_COMMAND_ID};
        std::atomic<TypeId> m_activeScanTypeId{TypeId::Invalid};
        std::atomic<std::chrono::steady_clock::time_point> m_activeScanStart{};
        std::atomic<bool> m_backendScanActive{false};

        Thread::RecurringTaskHandle m_timeoutReaperHandle{};
        bool m_reaperActive{false};
    };
}
