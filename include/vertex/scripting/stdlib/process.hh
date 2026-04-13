//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/event/eventbus.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <sdk/statuscode.h>

#include <functional>
#include <mutex>
#include <string>
#include <vector>

class asIScriptEngine;

namespace Vertex::Scripting::Stdlib
{
    struct ScriptProcessInfo final
    {
        std::string name{};
        std::string owner{};
        std::uint32_t id{};
        std::uint32_t parentId{};
    };

    struct ScriptModuleInfo final
    {
        std::string name{};
        std::string path{};
        std::uint64_t baseAddress{};
        std::uint64_t size{};
    };

    class ScriptProcess final
    {
    public:
        ScriptProcess(Runtime::ILoader& loader, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher);
        ~ScriptProcess();

        ScriptProcess(const ScriptProcess&) = delete;
        ScriptProcess& operator=(const ScriptProcess&) = delete;
        ScriptProcess(ScriptProcess&&) = delete;
        ScriptProcess& operator=(ScriptProcess&&) = delete;

        [[nodiscard]] StatusCode register_api(asIScriptEngine& engine);

        [[nodiscard]] bool is_process_open() const;
        [[nodiscard]] std::string get_process_name() const;
        [[nodiscard]] std::uint32_t get_process_id() const;
        [[nodiscard]] StatusCode close_process() const;
        [[nodiscard]] StatusCode kill_process() const;
        [[nodiscard]] StatusCode open_process(std::uint32_t processId) const;

        [[nodiscard]] StatusCode refresh_process_list();
        [[nodiscard]] std::uint32_t get_process_count() const;
        [[nodiscard]] ScriptProcessInfo get_process_at(std::uint32_t index) const;

        [[nodiscard]] StatusCode refresh_modules_list();
        [[nodiscard]] std::uint32_t get_module_count() const;
        [[nodiscard]] ScriptModuleInfo get_module_at(std::uint32_t index) const;

    private:
        std::reference_wrapper<Runtime::ILoader> m_loader;
        std::reference_wrapper<Event::EventBus> m_eventBus;
        std::reference_wrapper<Thread::IThreadDispatcher> m_dispatcher;

        mutable std::mutex m_stateMutex{};
        std::string m_processName{};
        std::uint32_t m_processId{};
        bool m_processOpen{};

        mutable std::mutex m_processListMutex{};
        std::vector<ScriptProcessInfo> m_processList{};

        mutable std::mutex m_modulesListMutex{};
        std::vector<ScriptModuleInfo> m_modulesList{};

        Event::SubscriptionId m_openSubscriptionId{};
        Event::SubscriptionId m_closeSubscriptionId{};

        [[nodiscard]] static StatusCode register_process_info_type(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_module_info_type(asIScriptEngine& engine);
    };
}
