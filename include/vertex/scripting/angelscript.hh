//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/scripting/iangelscript.hh>
#include <vertex/scripting/stdlib/io.hh>
#include <vertex/scripting/stdlib/process.hh>
#include <vertex/scripting/stdlib/memory.hh>
#include <vertex/scripting/stdlib/ui.hh>
#include <vertex/scripting/stdlib/util.hh>
#include <vertex/log/ilog.hh>
#include <vertex/event/eventbus.hh>
#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>
#include <vertex/macrohelp.hh>

#include <angelscript.h>

#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <shared_mutex>
#include <thread>
#include <unordered_map>
#include <unordered_set>

namespace Vertex::Scripting
{
    struct ScriptMessageCallbackContext final
    {
        Log::ILog* logService{};
        Event::EventBus* eventBus{};
    };

    struct AsEngineDeleter final
    {
        void operator()(asIScriptEngine* engine) const noexcept
        {
            if (engine)
            {
                engine->ShutDownAndRelease();
            }
        }
    };

    struct AsContextDeleter final
    {
        void operator()(const asIScriptContext* ctx) const noexcept
        {
            if (ctx)
            {
                std::ignore = ctx->Release();
            }
        }
    };

    using AsEnginePtr = std::unique_ptr<asIScriptEngine, AsEngineDeleter>;
    using AsContextPtr = std::unique_ptr<asIScriptContext, AsContextDeleter>;

    struct ScriptContext final
    {
        std::string moduleName{};
        asIScriptModule* module{};
        AsContextPtr context{};
        ScriptState state{ScriptState::Ready};
        std::chrono::steady_clock::time_point wakeTime{};
        bool sleepRequested{};
        BindingPolicy bindingPolicy{UseActive{}};

        asIScriptFunction* yieldCondition{};
        bool yieldInverted{};
        uint32_t yieldTicks{};
        bool yieldRequested{};
        std::unordered_set<int> breakpoints{};
        std::shared_ptr<std::mutex> breakpointMutex{std::make_shared<std::mutex>()};
        bool breakpointHit{};
        int breakpointLine{};
    };

    class VertexStringFactory final : public asIStringFactory
    {
    public:
        const void* GetStringConstant(const char* data, asUINT length) override;
        int ReleaseStringConstant(const void* str) override;
        int GetRawStringData(const void* str, char* data, asUINT* length) const override;

    private:
        std::unordered_map<const void*, std::unique_ptr<std::string>> m_strings{};
        mutable std::mutex m_mutex{};
    };

    class AngelScript final : public IAngelScript
    {
    public:
        AngelScript(Log::ILog& logService, Runtime::ILoader& loader, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher);
        ~AngelScript() override;

        [[nodiscard]] StatusCode start() override;
        [[nodiscard]] StatusCode stop() override;
        [[nodiscard]] bool is_running() const noexcept override;

        [[nodiscard]] std::expected<ContextId, StatusCode> create_context(
            std::string_view moduleName,
            const std::filesystem::path& scriptPath,
            BindingPolicy policy) override;

        [[nodiscard]] StatusCode remove_context(ContextId id) override;
        [[nodiscard]] StatusCode suspend_context(ContextId id) override;
        [[nodiscard]] StatusCode resume_context(ContextId id) override;
        [[nodiscard]] StatusCode set_breakpoint(ContextId id, int line) override;
        [[nodiscard]] StatusCode remove_breakpoint(ContextId id, int line) override;

        [[nodiscard]] std::expected<ScriptState, StatusCode> get_context_state(ContextId id) const override;
        [[nodiscard]] std::vector<ContextInfo> get_context_list() const override;
        [[nodiscard]] std::expected<std::vector<ContextVariable>, StatusCode> get_context_variables(ContextId id) const override;

    private:
        [[nodiscard]] StatusCode scheduler_loop(const std::stop_token& token);
        [[nodiscard]] int tick_context(ScriptContext& ctx) const;
        [[nodiscard]] StatusCode initialize_engine();
        [[nodiscard]] StatusCode register_api();
        [[nodiscard]] bool evaluate_yield_condition(asIScriptFunction* condition) const;
        static void clear_yield_state(ScriptContext& ctx);

        AsEnginePtr m_engine{};
        AsContextPtr m_evalContext{};
        std::jthread m_schedulerThread{};

        mutable std::shared_mutex m_engineMutex{};
        mutable std::mutex m_contextsMutex{};
        std::unordered_map<ContextId, ScriptContext> m_contexts{};

        START_PADDING_WARNING_SUPPRESSION

        std::atomic<bool> m_isRunning{};
        bool m_initialized{};
        std::atomic<ContextId> m_nextContextId{1};

        END_PADDING_WARNING_SUPPRESSION

        std::reference_wrapper<Log::ILog> m_logService;
        std::reference_wrapper<Runtime::ILoader> m_loader;

        ScriptMessageCallbackContext m_messageContext{};
        VertexStringFactory m_stringFactory{};
        Stdlib::ScriptLogger m_scriptLogger;
        Stdlib::ScriptProcess m_scriptProcess;
        Stdlib::ScriptMemory m_scriptMemory;
        Stdlib::ScriptUtility m_scriptUtility;
        Stdlib::ScriptUI m_scriptUI{};

    };
}
