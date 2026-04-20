//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/pluginruntimeservice.hh>

#include <fmt/format.h>

#include <utility>

namespace Vertex::Runtime::PluginRuntime
{
    namespace
    {
        [[nodiscard]] std::size_t index_of(const std::vector<Runtime::Plugin>& plugins, const Runtime::Plugin& needle)
        {
            for (std::size_t i{}; i < plugins.size(); ++i)
            {
                if (&plugins[i] == &needle) return i;
            }
            return static_cast<std::size_t>(-1);
        }
    }

    PluginRuntimeService::PluginRuntimeService(Runtime::ILoader& loader, Log::ILog& log)
        : m_loader{loader}, m_log{log}
    {
    }

    PluginRuntimeService::~PluginRuntimeService()
    {
        shutdown();
    }

    Runtime::CommandId PluginRuntimeService::allocate_command_id()
    {
        return m_nextCommandId.fetch_add(1, std::memory_order_relaxed);
    }

    PluginRuntimeService::ResultChannelPtr
    PluginRuntimeService::register_pending(Runtime::CommandId id)
    {
        auto channel = std::make_shared<Runtime::ResultChannel<engine::CommandResult>>();
        PendingResult pending{.id = id, .channel = channel, .completed = false};
        std::scoped_lock lock{m_pendingMutex};
        if (m_shuttingDown.load(std::memory_order_acquire)) return {};
        m_pending.emplace(id, std::move(pending));
        return channel;
    }

    void PluginRuntimeService::post_result(Runtime::CommandId id,
                                             StatusCode code,
                                             std::size_t index,
                                             std::string filename)
    {
        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            auto it = m_pending.find(id);
            if (it == m_pending.end() || it->second.completed) return;
            it->second.completed = true;
            channel = it->second.channel;
        }
        if (channel)
        {
            (void) channel->post(engine::CommandResult{
                .id = id, .code = code, .index = index, .filename = std::move(filename)});
        }
    }

    void PluginRuntimeService::fire_plugin_state(std::size_t index, bool loaded, std::string filename)
    {
        m_fanout.fire(PluginEvent{
            .kind = PluginEventKind::PluginStateChanged,
            .detail = PluginStateInfo{.index = index, .loaded = loaded, .filename = std::move(filename)}});
    }

    void PluginRuntimeService::fire_plugin_load_error(std::filesystem::path path, StatusCode code, std::string description)
    {
        m_fanout.fire(PluginEvent{
            .kind = PluginEventKind::PluginLoadError,
            .detail = PluginLoadErrorInfo{.path = std::move(path), .code = code, .description = std::move(description)}});
    }

    void PluginRuntimeService::fire_active_changed(std::size_t index, std::string filename)
    {
        m_fanout.fire(PluginEvent{
            .kind = PluginEventKind::ActivePluginChanged,
            .detail = ActivePluginChangedInfo{.index = index, .filename = std::move(filename)}});
    }

    Runtime::CommandId
    PluginRuntimeService::send_command(engine::PluginCommand command, std::chrono::milliseconds /*timeout*/)
    {
        if (m_shuttingDown.load(std::memory_order_acquire))
        {
            return Runtime::INVALID_COMMAND_ID;
        }

        return std::visit(
            [&]<class T>(T&& cmd) -> Runtime::CommandId
            {
                using D = std::decay_t<T>;
                if constexpr (std::is_same_v<D, engine::CmdLoad>)
                {
                    return dispatch_load(std::forward<T>(cmd));
                }
                else if constexpr (std::is_same_v<D, engine::CmdUnload>)
                {
                    return dispatch_unload(std::forward<T>(cmd));
                }
                else if constexpr (std::is_same_v<D, engine::CmdActivate>)
                {
                    return dispatch_activate(std::forward<T>(cmd));
                }
                else if constexpr (std::is_same_v<D, engine::CmdDeactivate>)
                {
                    return dispatch_deactivate(std::forward<T>(cmd));
                }
                else
                {
                    static_assert(!sizeof(D*), "unhandled plugin command");
                }
            },
            std::move(command));
    }

    Runtime::CommandId
    PluginRuntimeService::dispatch_load(engine::CmdLoad cmd)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id)) return Runtime::INVALID_COMMAND_ID;

        auto path = cmd.path;
        const auto status = m_loader.load_plugin(path);
        if (status != STATUS_OK)
        {
            fire_plugin_load_error(path, status, fmt::format("load_plugin returned {}", static_cast<int>(status)));
            post_result(id, status, static_cast<std::size_t>(-1), path.filename().string());
            return id;
        }

        std::size_t idx{static_cast<std::size_t>(-1)};
        std::string filename{};
        {
            const auto& plugins = m_loader.get_plugins();
            for (std::size_t i{}; i < plugins.size(); ++i)
            {
                if (plugins[i].get_path() == path)
                {
                    idx = i;
                    filename = plugins[i].get_filename();
                    break;
                }
            }
        }

        fire_plugin_state(idx, true, filename);
        post_result(id, STATUS_OK, idx, filename);
        return id;
    }

    Runtime::CommandId
    PluginRuntimeService::dispatch_unload(engine::CmdUnload cmd)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id)) return Runtime::INVALID_COMMAND_ID;

        std::string filename{};
        {
            const auto& plugins = m_loader.get_plugins();
            if (cmd.index >= plugins.size())
            {
                post_result(id, StatusCode::STATUS_ERROR_GENERAL_OUT_OF_BOUNDS, cmd.index, {});
                return id;
            }
            filename = plugins[cmd.index].get_filename();
        }

        const auto status = m_loader.unload_plugin(cmd.index);
        if (status == STATUS_OK)
        {
            {
                std::scoped_lock lock{m_activeIndexMutex};
                if (m_activeIndex.has_value() && *m_activeIndex == cmd.index)
                {
                    m_activeIndex.reset();
                    fire_active_changed(static_cast<std::size_t>(-1), {});
                }
            }
            fire_plugin_state(cmd.index, false, filename);
        }
        post_result(id, status, cmd.index, filename);
        return id;
    }

    Runtime::CommandId
    PluginRuntimeService::dispatch_activate(engine::CmdActivate cmd)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id)) return Runtime::INVALID_COMMAND_ID;

        const auto status = m_loader.set_active_plugin(cmd.index);
        if (status != STATUS_OK)
        {
            post_result(id, status, cmd.index, {});
            return id;
        }

        std::string filename{};
        {
            const auto& plugins = m_loader.get_plugins();
            if (cmd.index < plugins.size())
            {
                filename = plugins[cmd.index].get_filename();
            }
        }
        {
            std::scoped_lock lock{m_activeIndexMutex};
            m_activeIndex = cmd.index;
        }
        fire_active_changed(cmd.index, filename);
        post_result(id, STATUS_OK, cmd.index, filename);
        return id;
    }

    Runtime::CommandId
    PluginRuntimeService::dispatch_deactivate(engine::CmdDeactivate /*cmd*/)
    {
        const auto id = allocate_command_id();
        if (!register_pending(id)) return Runtime::INVALID_COMMAND_ID;

        std::size_t clearedIndex{static_cast<std::size_t>(-1)};
        {
            std::scoped_lock lock{m_activeIndexMutex};
            if (m_activeIndex.has_value())
            {
                clearedIndex = *m_activeIndex;
                m_activeIndex.reset();
            }
        }
        if (clearedIndex != static_cast<std::size_t>(-1))
        {
            fire_active_changed(static_cast<std::size_t>(-1), {});
        }
        post_result(id, STATUS_OK, clearedIndex, {});
        return id;
    }

    void PluginRuntimeService::subscribe_result(Runtime::CommandId id, ResultCallback callback)
    {
        if (id == Runtime::INVALID_COMMAND_ID || !callback) return;
        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            auto it = m_pending.find(id);
            if (it == m_pending.end()) { callback(engine::CommandResult{.id = id, .code = STATUS_TIMEOUT}); return; }
            channel = it->second.channel;
        }
        channel->on_result([cb = std::move(callback)](const engine::CommandResult& r) { cb(r); });
    }

    engine::CommandResult
    PluginRuntimeService::await_result(Runtime::CommandId id, std::chrono::milliseconds timeout)
    {
        if (id == Runtime::INVALID_COMMAND_ID)
        {
            return engine::CommandResult{.id = id, .code = STATUS_ERROR_INVALID_PARAMETER};
        }
        ResultChannelPtr channel{};
        {
            std::scoped_lock lock{m_pendingMutex};
            auto it = m_pending.find(id);
            if (it == m_pending.end()) return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
            channel = it->second.channel;
        }
        if (!channel->wait_for(timeout))
        {
            return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
        }
        auto result = channel->copy_result();
        if (result.has_value()) return std::move(*result);
        return engine::CommandResult{.id = id, .code = STATUS_TIMEOUT};
    }

    Runtime::SubscriptionId
    PluginRuntimeService::subscribe(PluginEventKindMask mask, EventCallback callback)
    {
        return m_fanout.subscribe(mask, std::move(callback));
    }

    void PluginRuntimeService::unsubscribe(Runtime::SubscriptionId subscriptionId) noexcept
    {
        m_fanout.unsubscribe(subscriptionId);
    }

    std::vector<PluginSnapshotEntry> PluginRuntimeService::snapshot_plugins() const
    {
        std::vector<PluginSnapshotEntry> out{};
        const auto& plugins = m_loader.get_plugins();
        out.reserve(plugins.size());

        std::optional<std::size_t> activeIdx{};
        {
            std::scoped_lock lock{m_activeIndexMutex};
            activeIdx = m_activeIndex;
        }

        for (std::size_t i{}; i < plugins.size(); ++i)
        {
            out.push_back(PluginSnapshotEntry{
                .index = i,
                .loaded = plugins[i].is_loaded(),
                .active = activeIdx.has_value() && *activeIdx == i,
                .filename = plugins[i].get_filename(),
            });
        }
        return out;
    }

    std::optional<PluginSnapshotEntry> PluginRuntimeService::snapshot_active() const
    {
        std::optional<std::size_t> activeIdx{};
        {
            std::scoped_lock lock{m_activeIndexMutex};
            activeIdx = m_activeIndex;
        }
        if (!activeIdx.has_value()) return std::nullopt;

        const auto& plugins = m_loader.get_plugins();
        if (*activeIdx >= plugins.size()) return std::nullopt;

        return PluginSnapshotEntry{
            .index = *activeIdx,
            .loaded = plugins[*activeIdx].is_loaded(),
            .active = true,
            .filename = plugins[*activeIdx].get_filename(),
        };
    }

    void PluginRuntimeService::shutdown()
    {
        bool expected = false;
        if (!m_shuttingDown.compare_exchange_strong(expected, true, std::memory_order_acq_rel)) return;

        m_fanout.shutdown();

        std::vector<std::pair<ResultChannelPtr, engine::CommandResult>> fired{};
        {
            std::scoped_lock lock{m_pendingMutex};
            for (auto& [id, entry] : m_pending)
            {
                if (!entry.completed)
                {
                    entry.completed = true;
                    fired.emplace_back(entry.channel,
                                        engine::CommandResult{.id = id, .code = STATUS_SHUTDOWN});
                }
            }
            m_pending.clear();
        }
        for (auto& [channel, result] : fired)
        {
            if (channel) (void) channel->post(std::move(result));
        }
    }
}
