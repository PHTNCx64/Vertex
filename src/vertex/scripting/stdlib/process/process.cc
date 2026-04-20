//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/process.hh>
#include <vertex/event/eventid.hh>
#include <vertex/event/types/processopenevent.hh>
#include <vertex/event/types/processcloseevent.hh>
#include <vertex/runtime/caller.hh>

#include <sdk/event.h>
#include <sdk/process.h>

#include <angelscript.h>

namespace Vertex::Scripting::Stdlib
{
    static constexpr std::string_view SUBSCRIBER_NAME{"ScriptProcess"};
    static constexpr std::uint32_t MAX_PROCESS_LIST_RETRIES{3};

    static void process_info_default_construct(void* memory)
    {
        new (memory) ScriptProcessInfo();
    }

    static void process_info_copy_construct(const ScriptProcessInfo& other, void* memory)
    {
        new (memory) ScriptProcessInfo(other);
    }

    static void process_info_destruct(ScriptProcessInfo* self)
    {
        self->~ScriptProcessInfo();
    }

    static ScriptProcessInfo& process_info_assign(const ScriptProcessInfo& other, ScriptProcessInfo* self)
    {
        *self = other;
        return *self;
    }

    static void module_info_default_construct(void* memory)
    {
        new (memory) ScriptModuleInfo();
    }

    static void module_info_copy_construct(const ScriptModuleInfo& other, void* memory)
    {
        new (memory) ScriptModuleInfo(other);
    }

    static void module_info_destruct(ScriptModuleInfo* self)
    {
        self->~ScriptModuleInfo();
    }

    static ScriptModuleInfo& module_info_assign(const ScriptModuleInfo& other, ScriptModuleInfo* self)
    {
        *self = other;
        return *self;
    }

    ScriptProcess::ScriptProcess(Runtime::ILoader& loader, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher)
        : m_loader{loader}, m_eventBus{eventBus}, m_dispatcher{dispatcher}
    {
        m_openSubscriptionId = m_eventBus.get().subscribe<Event::ProcessOpenEvent>(
            SUBSCRIBER_NAME, Event::PROCESS_OPEN_EVENT,
            [this](const Event::ProcessOpenEvent& event)
            {
                std::scoped_lock lock{m_stateMutex};
                m_processName = event.get_process_name();
                m_processId = event.get_process_id();
                m_processOpen = true;
            });

        m_closeSubscriptionId = m_eventBus.get().subscribe<Event::ProcessCloseEvent>(
            SUBSCRIBER_NAME, Event::PROCESS_CLOSED_EVENT,
            [this](const Event::ProcessCloseEvent&)
            {
                std::scoped_lock lock{m_stateMutex};
                m_processName.clear();
                m_processId = 0;
                m_processOpen = false;
            });
    }

    ScriptProcess::~ScriptProcess()
    {
        std::ignore = m_eventBus.get().unsubscribe(m_openSubscriptionId);
        std::ignore = m_eventBus.get().unsubscribe(m_closeSubscriptionId);
    }

    StatusCode ScriptProcess::register_api(asIScriptEngine& engine)
    {
        if (const auto status = register_process_info_type(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_module_info_type(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (engine.RegisterGlobalFunction(
                "bool is_process_open()",
                asMETHOD(ScriptProcess, is_process_open),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "string get_process_name()",
                asMETHOD(ScriptProcess, get_process_name),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "uint get_process_id()",
                asMETHOD(ScriptProcess, get_process_id),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int close_process()",
                asMETHOD(ScriptProcess, close_process),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int kill_process()",
                asMETHOD(ScriptProcess, kill_process),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int open_process(uint)",
                asMETHOD(ScriptProcess, open_process),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int refresh_process_list()",
                asMETHOD(ScriptProcess, refresh_process_list),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "uint get_process_count()",
                asMETHOD(ScriptProcess, get_process_count),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "ProcessInfo get_process_at(uint)",
                asMETHOD(ScriptProcess, get_process_at),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int refresh_modules_list()",
                asMETHOD(ScriptProcess, refresh_modules_list),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "uint get_module_count()",
                asMETHOD(ScriptProcess, get_module_count),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "ModuleInfo get_module_at(uint)",
                asMETHOD(ScriptProcess, get_module_at),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    bool ScriptProcess::is_process_open() const
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return false;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef)
        {
            return false;
        }

        const auto& plugin = pluginRef->get();
        if (!plugin.is_loaded())
        {
            return false;
        }

        std::packaged_task<StatusCode()> task(
            [this]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(Runtime::safe_call(ref->get().internal_vertex_process_is_valid));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return false;
        }

        return dispatchResult.value().get() == StatusCode::STATUS_OK;
    }

    std::string ScriptProcess::get_process_name() const
    {
        std::scoped_lock lock{m_stateMutex};
        return m_processName;
    }

    std::uint32_t ScriptProcess::get_process_id() const
    {
        std::scoped_lock lock{m_stateMutex};
        return m_processId;
    }

    StatusCode ScriptProcess::close_process() const
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::packaged_task<StatusCode()> task(
            [this]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                const auto closeStatus = Runtime::get_status(Runtime::safe_call(ref->get().internal_vertex_process_close));
                std::ignore = m_loader.get().dispatch_event(VERTEX_PROCESS_CLOSED, nullptr);
                return closeStatus;
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptProcess::kill_process() const
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::packaged_task<StatusCode()> task(
            [this]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(Runtime::safe_call(ref->get().internal_vertex_process_kill));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptProcess::register_process_info_type(asIScriptEngine& engine)
    {
        if (engine.RegisterObjectType(
                "ProcessInfo", sizeof(ScriptProcessInfo),
                asOBJ_VALUE | asGetTypeTraits<ScriptProcessInfo>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ProcessInfo", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(process_info_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ProcessInfo", asBEHAVE_CONSTRUCT, "void f(const ProcessInfo &in)",
                asFUNCTION(process_info_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ProcessInfo", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(process_info_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "ProcessInfo", "ProcessInfo &opAssign(const ProcessInfo &in)",
                asFUNCTION(process_info_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ProcessInfo", "string name", asOFFSET(ScriptProcessInfo, name)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ProcessInfo", "string owner", asOFFSET(ScriptProcessInfo, owner)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ProcessInfo", "uint id", asOFFSET(ScriptProcessInfo, id)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ProcessInfo", "uint parentId", asOFFSET(ScriptProcessInfo, parentId)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptProcess::refresh_process_list()
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::vector<ProcessInformation> rawProcesses{};

        std::packaged_task<StatusCode()> task(
            [this, &rawProcesses]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                auto& plugin = ref->get();

                std::uint32_t processCount{};
                const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_list, nullptr, &processCount);
                if (!Runtime::status_ok(countResult))
                {
                    return Runtime::get_status(countResult);
                }

                rawProcesses.resize(processCount);
                bool fillSucceeded{};

                for (std::uint32_t attempt{}; attempt < MAX_PROCESS_LIST_RETRIES; ++attempt)
                {
                    ProcessInformation* buffer = rawProcesses.data();
                    auto bufferSize = static_cast<std::uint32_t>(rawProcesses.size());

                    const auto listResult = Runtime::safe_call(plugin.internal_vertex_process_get_list, &buffer, &bufferSize);
                    const auto listStatus = Runtime::get_status(listResult);

                    if (Runtime::status_ok(listResult))
                    {
                        rawProcesses.resize(bufferSize);
                        fillSucceeded = true;
                        break;
                    }

                    if (listStatus == StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL)
                    {
                        rawProcesses.resize(bufferSize);
                        continue;
                    }

                    return listStatus;
                }

                if (!fillSucceeded)
                {
                    return StatusCode::STATUS_ERROR_MEMORY_OPERATION_ABORTED;
                }

                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        std::vector<ScriptProcessInfo> converted{};
        converted.reserve(rawProcesses.size());
        for (const auto& [processName, processOwner, processId, parentProcessId] : rawProcesses)
        {
            converted.emplace_back(ScriptProcessInfo{
                .name = processName,
                .owner = processOwner,
                .id = processId,
                .parentId = parentProcessId
            });
        }

        {
            std::scoped_lock lock{m_processListMutex};
            m_processList = std::move(converted);
        }

        return StatusCode::STATUS_OK;
    }

    std::uint32_t ScriptProcess::get_process_count() const
    {
        std::scoped_lock lock{m_processListMutex};
        return static_cast<std::uint32_t>(m_processList.size());
    }

    ScriptProcessInfo ScriptProcess::get_process_at(const std::uint32_t index) const
    {
        std::scoped_lock lock{m_processListMutex};
        if (index >= m_processList.size())
        {
            return {};
        }
        return m_processList[index];
    }

    StatusCode ScriptProcess::open_process(const std::uint32_t processId) const
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::packaged_task<StatusCode()> task(
            [this, processId]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }

                const auto openResult = Runtime::safe_call(ref->get().internal_vertex_process_open, processId);
                if (!Runtime::status_ok(openResult))
                {
                    return Runtime::get_status(openResult);
                }

                ProcessEventData eventData{};
                eventData.processId = processId;
                eventData.processHandle = nullptr;
                return m_loader.get().dispatch_event(VERTEX_PROCESS_OPENED, &eventData);
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptProcess::register_module_info_type(asIScriptEngine& engine)
    {
        if (engine.RegisterObjectType(
                "ModuleInfo", sizeof(ScriptModuleInfo),
                asOBJ_VALUE | asGetTypeTraits<ScriptModuleInfo>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ModuleInfo", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(module_info_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ModuleInfo", asBEHAVE_CONSTRUCT, "void f(const ModuleInfo &in)",
                asFUNCTION(module_info_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "ModuleInfo", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(module_info_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "ModuleInfo", "ModuleInfo &opAssign(const ModuleInfo &in)",
                asFUNCTION(module_info_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ModuleInfo", "string name", asOFFSET(ScriptModuleInfo, name)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ModuleInfo", "string path", asOFFSET(ScriptModuleInfo, path)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ModuleInfo", "uint64 baseAddress", asOFFSET(ScriptModuleInfo, baseAddress)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("ModuleInfo", "uint64 size", asOFFSET(ScriptModuleInfo, size)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptProcess::refresh_modules_list()
    {
        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::vector<ModuleInformation> rawModules{};

        std::packaged_task<StatusCode()> task(
            [this, &rawModules]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                auto& plugin = ref->get();

                std::uint32_t count{};
                const auto countResult = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, nullptr, &count);
                if (!Runtime::status_ok(countResult))
                {
                    return Runtime::get_status(countResult);
                }

                if (count == 0)
                {
                    return StatusCode::STATUS_OK;
                }

                rawModules.resize(count);
                auto* modulesPtr = rawModules.data();
                const auto listResult = Runtime::safe_call(plugin.internal_vertex_process_get_modules_list, &modulesPtr, &count);
                if (!Runtime::status_ok(listResult))
                {
                    return Runtime::get_status(listResult);
                }

                rawModules.resize(count);
                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::ProcessList, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        std::vector<ScriptModuleInfo> converted{};
        converted.reserve(rawModules.size());
        for (const auto& [moduleName, modulePath, baseAddress, size] : rawModules)
        {
            converted.emplace_back(ScriptModuleInfo{
                .name = moduleName,
                .path = modulePath,
                .baseAddress = baseAddress,
                .size = size
            });
        }

        {
            std::scoped_lock lock{m_modulesListMutex};
            m_modulesList = std::move(converted);
        }

        return StatusCode::STATUS_OK;
    }

    std::uint32_t ScriptProcess::get_module_count() const
    {
        std::scoped_lock lock{m_modulesListMutex};
        return static_cast<std::uint32_t>(m_modulesList.size());
    }

    ScriptModuleInfo ScriptProcess::get_module_at(const std::uint32_t index) const
    {
        std::scoped_lock lock{m_modulesListMutex};
        if (index >= m_modulesList.size())
        {
            return {};
        }
        return m_modulesList[index];
    }
}
