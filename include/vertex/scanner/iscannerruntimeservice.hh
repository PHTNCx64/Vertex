//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/command.hh>
#include <vertex/scanner/memoryscanner/imemoryscanner.hh>
#include <vertex/scanner/scanner_command.hh>
#include <vertex/scanner/scanner_event.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace Vertex::Scanner
{
    class IScannerRuntimeService
    {
      public:
        using EventCallback = std::move_only_function<void(const ScannerEvent&) const>;
        using ResultCallback = std::move_only_function<void(const service::CommandResult&) const>;

        static constexpr std::chrono::milliseconds DEFAULT_COMMAND_TIMEOUT{std::chrono::seconds{30}};
        static constexpr std::chrono::milliseconds DEFAULT_SNAPSHOT_TIMEOUT{std::chrono::seconds{2}};

        IScannerRuntimeService() = default;
        virtual ~IScannerRuntimeService() = default;

        IScannerRuntimeService(const IScannerRuntimeService&) = delete;
        IScannerRuntimeService& operator=(const IScannerRuntimeService&) = delete;
        IScannerRuntimeService(IScannerRuntimeService&&) = delete;
        IScannerRuntimeService& operator=(IScannerRuntimeService&&) = delete;

        [[nodiscard]] virtual Runtime::CommandId
        send_command(service::Command command,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        virtual void subscribe_result(Runtime::CommandId id, ResultCallback callback) = 0;

        [[nodiscard]] virtual service::CommandResult
        await_result(Runtime::CommandId id,
                     std::chrono::milliseconds timeout = DEFAULT_COMMAND_TIMEOUT) = 0;

        [[nodiscard]] virtual Runtime::SubscriptionId
        subscribe(ScannerEventKindMask mask, EventCallback callback) = 0;

        virtual void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept = 0;

        [[nodiscard]] virtual std::uint64_t results_count() const = 0;
        [[nodiscard]] virtual StatusCode snapshot_results(std::vector<IMemoryScanner::ScanResultEntry>& out,
                                                          std::size_t startIndex,
                                                          std::size_t count) const = 0;
        [[nodiscard]] virtual bool can_undo() const = 0;
        [[nodiscard]] virtual bool is_scanning() const = 0;

        [[nodiscard]] virtual std::optional<TypeSchema> find_type(TypeId id) const = 0;
        [[nodiscard]] virtual std::vector<TypeSchema> list_types() const = 0;
        [[nodiscard]] virtual std::uint32_t invalidate_plugin_types(std::size_t pluginIndex) = 0;

        virtual void on_scanner_event(ScannerEvent event) = 0;
        virtual void on_scanner_command_result(service::CommandResult result) = 0;

        virtual void shutdown() = 0;
    };
}
