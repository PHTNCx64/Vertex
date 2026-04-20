//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//





#include <vertex/scanner/iscannerruntimeservice.hh>
#include <vertex/scanner/scanner_command.hh>
#include <vertex/scanner/scanner_event.hh>
#include <vertex/scanner/scanner_typeschema.hh>

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <mutex>
#include <optional>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Vertex::Testing::Fakes
{
    class FakeScannerRuntimeService final : public Scanner::IScannerRuntimeService
    {
      public:
        FakeScannerRuntimeService() = default;
        ~FakeScannerRuntimeService() override = default;

        [[nodiscard]] Runtime::CommandId
        send_command(Scanner::service::Command ,
                     std::chrono::milliseconds ) override
        {
            return m_nextId.fetch_add(1, std::memory_order_relaxed);
        }

        void subscribe_result(Runtime::CommandId id, ResultCallback callback) override
        {
            std::scoped_lock lock{m_resultMutex};
            m_resultCallbacks.emplace(id, std::move(callback));
        }

        [[nodiscard]] Scanner::service::CommandResult
        await_result(Runtime::CommandId id,
                     std::chrono::milliseconds ) override
        {
            std::scoped_lock lock{m_resultMutex};
            auto it = m_completed.find(id);
            if (it == m_completed.end())
            {
                return Scanner::service::CommandResult{.id = id, .code = STATUS_TIMEOUT};
            }
            return it->second;
        }

        [[nodiscard]] Runtime::SubscriptionId
        subscribe(Scanner::ScannerEventKindMask mask, EventCallback callback) override
        {
            std::scoped_lock lock{m_subMutex};
            const auto id = m_nextSubId.fetch_add(1, std::memory_order_relaxed);
            m_subs.push_back(Subscription{id, mask, std::move(callback)});
            return id;
        }

        void unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept override
        {
            std::scoped_lock lock{m_subMutex};
            for (auto it = m_subs.begin(); it != m_subs.end(); ++it)
            {
                if (it->id == subscriptionId)
                {
                    m_subs.erase(it);
                    return;
                }
            }
        }

        [[nodiscard]] std::uint64_t results_count() const override { return m_resultsCount; }
        [[nodiscard]] StatusCode snapshot_results(std::vector<Scanner::IMemoryScanner::ScanResultEntry>& out,
                                                    std::size_t , std::size_t ) const override
        {
            out.clear();
            return STATUS_OK;
        }
        [[nodiscard]] bool can_undo() const override { return m_canUndo; }
        [[nodiscard]] bool is_scanning() const override { return m_isScanning; }

        [[nodiscard]] std::optional<Scanner::TypeSchema> find_type(Scanner::TypeId id) const override
        {
            std::scoped_lock lock{m_registryMutex};
            auto it = m_types.find(id);
            if (it == m_types.end()) return std::nullopt;
            return it->second;
        }

        [[nodiscard]] std::vector<Scanner::TypeSchema> list_types() const override
        {
            std::scoped_lock lock{m_registryMutex};
            std::vector<Scanner::TypeSchema> out{};
            out.reserve(m_types.size());
            for (const auto& [id, schema] : m_types) out.push_back(schema);
            return out;
        }

        [[nodiscard]] std::uint32_t invalidate_plugin_types(std::size_t pluginIndex) override
        {
            std::uint32_t removed{};
            std::vector<Scanner::TypeSchema> removedSchemas{};
            {
                std::scoped_lock lock{m_registryMutex};
                for (auto it = m_types.begin(); it != m_types.end();)
                {
                    if (it->second.sourcePluginIndex == pluginIndex)
                    {
                        removedSchemas.push_back(it->second);
                        it = m_types.erase(it);
                        ++removed;
                    }
                    else
                    {
                        ++it;
                    }
                }
            }
            for (const auto& schema : removedSchemas)
            {
                inject_event(Scanner::ScannerEvent{
                    .kind = Scanner::ScannerEventKind::TypeUnregistered,
                    .detail = Scanner::TypeUnregisteredInfo{.id = schema.id, .name = schema.name}});
            }
            if (removed > 0)
            {
                inject_event(Scanner::ScannerEvent{
                    .kind = Scanner::ScannerEventKind::RegistryInvalidated,
                    .detail = Scanner::RegistryInvalidatedInfo{.sourcePluginIndex = pluginIndex,
                                                                .removedCount = removed}});
            }
            return removed;
        }

        void on_scanner_event(Scanner::ScannerEvent event) override { inject_event(std::move(event)); }
        void on_scanner_command_result(Scanner::service::CommandResult result) override
        {
            complete_command(result.id, std::move(result));
        }

        void shutdown() override { m_shutdown = true; }

        

        void inject_event(Scanner::ScannerEvent event)
        {
            std::vector<Subscription*> snapshot{};
            {
                std::scoped_lock lock{m_subMutex};
                for (auto& sub : m_subs)
                {
                    const auto bit = static_cast<std::underlying_type_t<Scanner::ScannerEventKind>>(event.kind);
                    if ((sub.mask & bit) != 0)
                    {
                        snapshot.push_back(&sub);
                    }
                }
                for (auto* sub : snapshot)
                {
                    if (sub->callback) sub->callback(event);
                }
            }
        }

        void complete_command(Runtime::CommandId id, Scanner::service::CommandResult result)
        {
            ResultCallback cb{};
            {
                std::scoped_lock lock{m_resultMutex};
                m_completed[id] = result;
                auto it = m_resultCallbacks.find(id);
                if (it != m_resultCallbacks.end())
                {
                    cb = std::move(it->second);
                    m_resultCallbacks.erase(it);
                }
            }
            if (cb) cb(result);
        }

        void trigger_timeout(Runtime::CommandId id)
        {
            complete_command(id, Scanner::service::CommandResult{.id = id, .code = STATUS_TIMEOUT});
        }

        std::uint32_t simulate_plugin_unload(std::size_t pluginIndex)
        {
            return invalidate_plugin_types(pluginIndex);
        }

        void install_type(Scanner::TypeSchema schema)
        {
            std::scoped_lock lock{m_registryMutex};
            m_types[schema.id] = std::move(schema);
        }

        [[nodiscard]] std::size_t active_subscription_count() const
        {
            std::scoped_lock lock{m_subMutex};
            return m_subs.size();
        }

        std::atomic<std::uint64_t> m_resultsCount{};
        std::atomic<bool> m_canUndo{false};
        std::atomic<bool> m_isScanning{false};
        std::atomic<bool> m_shutdown{false};

      private:
        struct Subscription
        {
            Runtime::SubscriptionId id{};
            Scanner::ScannerEventKindMask mask{};
            EventCallback callback{};
        };

        mutable std::mutex m_subMutex{};
        std::vector<Subscription> m_subs{};
        std::atomic<Runtime::SubscriptionId> m_nextSubId{1};

        mutable std::mutex m_resultMutex{};
        std::unordered_map<Runtime::CommandId, ResultCallback> m_resultCallbacks{};
        std::unordered_map<Runtime::CommandId, Scanner::service::CommandResult> m_completed{};
        std::atomic<Runtime::CommandId> m_nextId{1};

        mutable std::mutex m_registryMutex{};
        std::unordered_map<Scanner::TypeId, Scanner::TypeSchema> m_types{};
    };
}
