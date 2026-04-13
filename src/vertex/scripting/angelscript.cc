//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/angelscript.hh>
#include <vertex/scripting/scriptarray.h>
#include <vertex/event/types/scriptoutputevent.hh>
#include <vertex/event/types/scriptdiagnosticevent.hh>

#include <fmt/format.h>

#include <algorithm>
#include <chrono>
#include <cstdint>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <ranges>

namespace
{
    constexpr asPWORD VERTEX_CONTEXT_DATA_ID{0x56525458}; // corresponds to VRTX
    constexpr auto QUANTUM_DURATION{std::chrono::microseconds{1000}};
    constexpr auto IDLE_SLEEP_DURATION{std::chrono::milliseconds{1}};
    constexpr std::uintmax_t MAX_SCRIPT_FILE_SIZE{1024 * 1024};

    struct QuantumBudget final
    {
        std::chrono::steady_clock::time_point deadline{};
    };

    struct LineCallbackData final
    {
        QuantumBudget budget{};
        Vertex::Scripting::ScriptContext* scriptContext{};
    };

    void line_callback(asIScriptContext* ctx, const LineCallbackData* data)
    {
        if (!ctx || !data)
        {
            return;
        }

        if (std::chrono::steady_clock::now() >= data->budget.deadline) [[unlikely]]
        {
            ctx->Suspend();
            return;
        }

        if (!data->scriptContext || !data->scriptContext->breakpointMutex)
        {
            return;
        }

        const auto currentLine = ctx->GetLineNumber(0, nullptr, nullptr);
        if (currentLine <= 0)
        {
            return;
        }

        {
            std::scoped_lock lock{*data->scriptContext->breakpointMutex};
            if (data->scriptContext->breakpoints.contains(currentLine))
            {
                data->scriptContext->breakpointHit = true;
                data->scriptContext->breakpointLine = currentLine;
            }
        }

        if (data->scriptContext->breakpointHit)
        {
            ctx->Suspend();
        }
    }

    void script_yield()
    {
        auto* ctx = asGetActiveContext();
        if (ctx) [[likely]]
        {
            ctx->Suspend();
        }
    }

    void script_sleep(const asUINT milliseconds)
    {
        auto* ctx = asGetActiveContext();
        if (!ctx) [[unlikely]]
        {
            return;
        }

        auto* scriptCtx = static_cast<Vertex::Scripting::ScriptContext*>(ctx->GetUserData(VERTEX_CONTEXT_DATA_ID));

        if (!scriptCtx) [[unlikely]]
        {
            return;
        }

        scriptCtx->wakeTime = std::chrono::steady_clock::now() +
                              std::chrono::milliseconds{milliseconds};
        scriptCtx->sleepRequested = true;
        ctx->Suspend();
    }

    void script_wait_condition(asIScriptFunction* condition, const bool inverted)
    {
        auto* ctx = asGetActiveContext();
        if (!ctx || !condition) [[unlikely]]
        {
            return;
        }

        auto* scriptCtx = static_cast<Vertex::Scripting::ScriptContext*>(
            ctx->GetUserData(VERTEX_CONTEXT_DATA_ID));

        if (!scriptCtx) [[unlikely]]
        {
            return;
        }

        std::ignore = condition->AddRef();
        scriptCtx->yieldCondition = condition;
        scriptCtx->yieldInverted = inverted;
        scriptCtx->yieldRequested = true;
        ctx->Suspend();
    }

    void script_wait_until(asIScriptFunction* condition)
    {
        script_wait_condition(condition, false);
    }

    void script_wait_while(asIScriptFunction* condition)
    {
        script_wait_condition(condition, true);
    }

    void script_wait_ticks(const asUINT ticks)
    {
        auto* ctx = asGetActiveContext();
        if (!ctx) [[unlikely]]
        {
            return;
        }

        auto* scriptCtx = static_cast<Vertex::Scripting::ScriptContext*>(
            ctx->GetUserData(VERTEX_CONTEXT_DATA_ID));

        if (!scriptCtx) [[unlikely]]
        {
            return;
        }

        scriptCtx->yieldTicks = ticks;
        scriptCtx->yieldRequested = true;
        ctx->Suspend();
    }

    [[nodiscard]] Vertex::Event::ScriptOutputSeverity output_severity_from_message_type(const int type)
    {
        switch (type)
        {
            case asMSGTYPE_ERROR:
                return Vertex::Event::ScriptOutputSeverity::Error;
            case asMSGTYPE_WARNING:
                return Vertex::Event::ScriptOutputSeverity::Warning;
            case asMSGTYPE_INFORMATION:
            default: return Vertex::Event::ScriptOutputSeverity::Info;
            }
    }

    [[nodiscard]] Vertex::Event::ScriptDiagnosticSeverity diagnostic_severity_from_message_type(const int type)
    {
        switch (type)
        {
            case asMSGTYPE_ERROR:
                return Vertex::Event::ScriptDiagnosticSeverity::Error;
            case asMSGTYPE_WARNING:
                return Vertex::Event::ScriptDiagnosticSeverity::Warning;
            case asMSGTYPE_INFORMATION:
            default:
                return Vertex::Event::ScriptDiagnosticSeverity::Info;
        }
    }

    [[nodiscard]] std::string primitive_type_name(const int typeId)
    {
        switch (typeId)
        {
            case asTYPEID_BOOL:   return "bool";
            case asTYPEID_INT8:   return "int8";
            case asTYPEID_INT16:  return "int16";
            case asTYPEID_INT32:  return "int";
            case asTYPEID_INT64:  return "int64";
            case asTYPEID_UINT8:  return "uint8";
            case asTYPEID_UINT16: return "uint16";
            case asTYPEID_UINT32: return "uint";
            case asTYPEID_UINT64: return "uint64";
            case asTYPEID_FLOAT:  return "float";
            case asTYPEID_DOUBLE: return "double";
            default:              return "<primitive>";
        }
    }

    [[nodiscard]] constexpr int object_type_id(const int typeId)
    {
        return typeId & ~(asTYPEID_OBJHANDLE | asTYPEID_HANDLETOCONST);
    }

    [[nodiscard]] std::string format_variable_type_name(asIScriptEngine& engine, const int typeId)
    {
        if ((typeId & asTYPEID_MASK_OBJECT) == 0)
        {
            return primitive_type_name(typeId);
        }

        const auto* typeInfo = engine.GetTypeInfoById(object_type_id(typeId));
        std::string typeName = (typeInfo && typeInfo->GetName())
            ? std::string{typeInfo->GetName()}
            : std::string{"<unknown>"};

        if ((typeId & asTYPEID_OBJHANDLE) != 0)
        {
            typeName += "@";
        }

        return typeName;
    }

    [[nodiscard]] std::string format_primitive_variable_value(const int typeId, const void* address)
    {
        if (!address)
        {
            return "<unavailable>";
        }

        switch (typeId)
        {
            case asTYPEID_BOOL:
                return (*static_cast<const asBYTE*>(address) != 0) ? "true" : "false";
            case asTYPEID_INT8:
                return fmt::format("{}", static_cast<int>(*static_cast<const std::int8_t*>(address)));
            case asTYPEID_INT16:
                return fmt::format("{}", *static_cast<const std::int16_t*>(address));
            case asTYPEID_INT32:
                return fmt::format("{}", *static_cast<const std::int32_t*>(address));
            case asTYPEID_INT64:
                return fmt::format("{}", *static_cast<const std::int64_t*>(address));
            case asTYPEID_UINT8:
                return fmt::format("{}", static_cast<unsigned int>(*static_cast<const std::uint8_t*>(address)));
            case asTYPEID_UINT16:
                return fmt::format("{}", *static_cast<const std::uint16_t*>(address));
            case asTYPEID_UINT32:
                return fmt::format("{}", *static_cast<const std::uint32_t*>(address));
            case asTYPEID_UINT64:
                return fmt::format("{}", *static_cast<const std::uint64_t*>(address));
            case asTYPEID_FLOAT:
                return fmt::format("{}", *static_cast<const float*>(address));
            case asTYPEID_DOUBLE:
                return fmt::format("{}", *static_cast<const double*>(address));
            default:
                return "<primitive>";
        }
    }

    [[nodiscard]] std::string format_context_variable_value(asIScriptEngine& engine, const int typeId, void* address)
    {
        if ((typeId & asTYPEID_MASK_OBJECT) == 0)
        {
            return format_primitive_variable_value(typeId, address);
        }

        const bool isHandle = (typeId & asTYPEID_OBJHANDLE) != 0;
        const int objectTypeId = object_type_id(typeId);
        const auto* typeInfo = engine.GetTypeInfoById(objectTypeId);
        const auto typeName = (typeInfo && typeInfo->GetName())
            ? std::string{typeInfo->GetName()}
            : std::string{"object"};

        if (!address)
        {
            return "<unavailable>";
        }

        if (isHandle)
        {
            auto* instancePtr = static_cast<void* const*>(address);
            void* instance = instancePtr ? *instancePtr : nullptr;
            if (!instance)
            {
                return "null";
            }

            return fmt::format("<{} {}>", typeName, fmt::ptr(instance));
        }

        if (typeName == "string")
        {
            return *static_cast<const std::string*>(address);
        }

        return fmt::format("<{} {}>", typeName, fmt::ptr(address));
    }

    void engine_message_callback(const asSMessageInfo* msg, void* param) noexcept
    {
        const auto* callbackContext = static_cast<Vertex::Scripting::ScriptMessageCallbackContext*>(param);
        if (!callbackContext || !callbackContext->logService || !msg) [[unlikely]]
        {
            return;
        }

        try
        {
            const auto section = std::string{msg->section ? msg->section : ""};
            const auto message = std::string{msg->message ? msg->message : ""};
            const auto formatted = fmt::format("{} ({}, {}) : {}",
                                         section, msg->row, msg->col, message);

            switch (msg->type)
            {
                case asMSGTYPE_ERROR:
                    std::ignore = callbackContext->logService->log_error(formatted);
                    break;
                case asMSGTYPE_WARNING:
                    std::ignore = callbackContext->logService->log_warn(formatted);
                    break;
                case asMSGTYPE_INFORMATION:
                    std::ignore = callbackContext->logService->log_info(formatted);
                    break;
            }

            if (callbackContext->eventBus)
            {
                callbackContext->eventBus->broadcast(Vertex::Event::ScriptOutputEvent{
                    output_severity_from_message_type(msg->type), formatted});

                callbackContext->eventBus->broadcast(Vertex::Event::ScriptDiagnosticEvent{
                    diagnostic_severity_from_message_type(msg->type),
                    section,
                    static_cast<int>(msg->row),
                    static_cast<int>(msg->col),
                    message});
            }
        }
        catch (...)
        {
        }
    }

    [[nodiscard]] std::expected<std::string, StatusCode> read_script_file(
        const std::filesystem::path& path)
    {
        std::error_code ec{};
        const auto fileSize = std::filesystem::file_size(path, ec);
        if (ec)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_FILE_NOT_FOUND);
        }

        if (fileSize > MAX_SCRIPT_FILE_SIZE)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_FS_FILE_TOO_LARGE);
        }

        std::ifstream file{path, std::ios::in};
        if (!file)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_FS_FILE_OPEN_FAILED);
        }

        std::string content{};
        content.reserve(static_cast<std::size_t>(fileSize));
        content.assign(std::istreambuf_iterator<char>{file},
                       std::istreambuf_iterator<char>{});
        return content;
    }

    void string_default_construct(std::string* self)
    {
        new (self) std::string{};
    }

    void string_copy_construct(const std::string& other, std::string* self)
    {
        new (self) std::string{other};
    }

    void string_destruct(std::string* self)
    {
        self->~basic_string();
    }

    std::string& string_assign(const std::string& other, std::string& self)
    {
        self = other;
        return self;
    }

    [[nodiscard]] std::string string_add(const std::string& self, const std::string& other)
    {
        return self + other;
    }

    std::string& string_add_assign(const std::string& other, std::string& self)
    {
        self += other;
        return self;
    }

    [[nodiscard]] bool string_equals(const std::string& self, const std::string& other)
    {
        return self == other;
    }

    [[nodiscard]] asUINT string_length(const std::string& self)
    {
        return static_cast<asUINT>(self.size());
    }

    [[nodiscard]] bool string_is_empty(const std::string& self)
    {
        return self.empty();
    }

    [[nodiscard]] std::string string_add_int(const std::string& self, const int value)
    {
        return self + std::to_string(value);
    }

    [[nodiscard]] std::string string_add_r_int(const std::string& self, const int value)
    {
        return std::to_string(value) + self;
    }

    [[nodiscard]] std::string string_add_uint(const std::string& self, const asUINT value)
    {
        return self + std::to_string(value);
    }

    [[nodiscard]] std::string string_add_r_uint(const std::string& self, const asUINT value)
    {
        return std::to_string(value) + self;
    }

    [[nodiscard]] std::string string_add_float(const std::string& self, const float value)
    {
        return self + fmt::format("{}", value);
    }

    [[nodiscard]] std::string string_add_r_float(const std::string& self, const float value)
    {
        return fmt::format("{}", value) + self;
    }

    [[nodiscard]] std::string string_add_double(const std::string& self, const double value)
    {
        return self + fmt::format("{}", value);
    }

    [[nodiscard]] std::string string_add_r_double(const std::string& self, const double value)
    {
        return fmt::format("{}", value) + self;
    }

    [[nodiscard]] std::string string_add_bool(const std::string& self, const bool value)
    {
        return self + (value ? "true" : "false");
    }

    [[nodiscard]] std::string string_add_r_bool(const std::string& self, const bool value)
    {
        return (value ? "true" : "false") + self;
    }

    [[nodiscard]] StatusCode register_string_type(asIScriptEngine& engine,
                                                    Vertex::Scripting::VertexStringFactory& stringFactory)
    {
        if (engine.RegisterObjectType(
                "string", sizeof(std::string),
                asOBJ_VALUE | asGetTypeTraits<std::string>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "string", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(string_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "string", asBEHAVE_CONSTRUCT, "void f(const string &in)",
                asFUNCTION(string_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "string", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(string_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string &opAssign(const string &in)",
                asFUNCTION(string_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd(const string &in) const",
                asFUNCTION(string_add), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string &opAddAssign(const string &in)",
                asFUNCTION(string_add_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "bool opEquals(const string &in) const",
                asFUNCTION(string_equals), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "uint length() const",
                asFUNCTION(string_length), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "bool is_empty() const",
                asFUNCTION(string_is_empty), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterStringFactory("string", &stringFactory) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    [[nodiscard]] StatusCode register_string_conversions(asIScriptEngine& engine)
    {
        if (engine.RegisterObjectMethod(
                "string", "string opAdd(int) const",
                asFUNCTION(string_add_int), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd_r(int) const",
                asFUNCTION(string_add_r_int), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd(uint) const",
                asFUNCTION(string_add_uint), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd_r(uint) const",
                asFUNCTION(string_add_r_uint), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd(float) const",
                asFUNCTION(string_add_float), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd_r(float) const",
                asFUNCTION(string_add_r_float), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd(double) const",
                asFUNCTION(string_add_double), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd_r(double) const",
                asFUNCTION(string_add_r_double), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd(bool) const",
                asFUNCTION(string_add_bool), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "string", "string opAdd_r(bool) const",
                asFUNCTION(string_add_r_bool), asCALL_CDECL_OBJFIRST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

}

namespace Vertex::Scripting
{
    const void* VertexStringFactory::GetStringConstant(const char* data, const asUINT length)
    {
        auto str = std::make_unique<std::string>(data, length);
        auto* ptr = str.get();
        std::scoped_lock lock{m_mutex};
        m_strings.emplace(ptr, std::move(str));
        return ptr;
    }

    int VertexStringFactory::ReleaseStringConstant(const void* str)
    {
        std::scoped_lock lock{m_mutex};
        m_strings.erase(str);
        return asSUCCESS;
    }

    int VertexStringFactory::GetRawStringData(const void* str, char* data, asUINT* length) const
    {
        std::scoped_lock lock{m_mutex};
        const auto it = m_strings.find(str);
        if (it == m_strings.end()) [[unlikely]]
        {
            return asERROR;
        }
        const auto& s = *it->second;
        if (length)
        {
            *length = static_cast<asUINT>(s.size());
        }
        if (data)
        {
            std::copy_n(s.data(), s.size(), data);
        }
        return asSUCCESS;
    }

    AngelScript::AngelScript(Log::ILog& logService, Runtime::ILoader& loader, Event::EventBus& eventBus, Thread::IThreadDispatcher& dispatcher)
        : m_logService{logService}, m_loader{loader}, m_messageContext{&logService, &eventBus}, m_scriptLogger{logService, eventBus},
          m_scriptProcess{loader, eventBus, dispatcher}, m_scriptMemory{loader, dispatcher},
          m_scriptUtility{loader}
    {
        m_initialized = (initialize_engine() == StatusCode::STATUS_OK);
    }

    AngelScript::~AngelScript()
    {
        if (m_isRunning.load(std::memory_order_acquire))
        {
            std::ignore = stop();
        }
        for (auto& ctx : m_contexts | std::views::values)
        {
            clear_yield_state(ctx);
        }
        m_contexts.clear();
        m_evalContext.reset();
        m_engine.reset();
    }

    StatusCode AngelScript::initialize_engine()
    {
        auto* rawEngine = asCreateScriptEngine();
        if (!rawEngine)
        {
            std::ignore = m_logService.get().log_error("Failed to create AngelScript engine");
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        m_engine.reset(rawEngine);

        if (m_engine->SetMessageCallback(
                asFUNCTION(engine_message_callback),
                &m_messageContext,
                asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterGlobalFunction(
                "void yield()", asFUNCTION(script_yield), asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterGlobalFunction(
                "void sleep(uint)", asFUNCTION(script_sleep), asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterFuncdef("bool CoroutineCondition()") < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterGlobalFunction(
                "void wait_until(CoroutineCondition@)",
                asFUNCTION(script_wait_until), asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterGlobalFunction(
                "void wait_while(CoroutineCondition@)",
                asFUNCTION(script_wait_while), asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (m_engine->RegisterGlobalFunction(
                "void wait_ticks(uint)",
                asFUNCTION(script_wait_ticks), asCALL_CDECL) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        auto* rawEvalCtx = m_engine->CreateContext();
        if (!rawEvalCtx)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }
        m_evalContext.reset(rawEvalCtx);

        return register_api();
    }

    StatusCode AngelScript::register_api()
    {
        if (const auto status = register_string_type(*m_engine, m_stringFactory);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = register_string_conversions(*m_engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        RegisterScriptArray(m_engine.get(), true);

        if (const auto status = m_scriptLogger.register_api(*m_engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = m_scriptProcess.register_api(*m_engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = m_scriptMemory.register_api(*m_engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (const auto status = m_scriptUtility.register_api(*m_engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        return m_scriptUI.register_api(*m_engine);
    }

    StatusCode AngelScript::start()
    {
        if (!m_initialized)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        bool expected{};
        if (!m_isRunning.compare_exchange_strong(expected, true,
                                                   std::memory_order_acq_rel,
                                                   std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ALREADY_RUNNING;
        }

        try
        {
            m_schedulerThread = std::jthread{
                [this](const std::stop_token& token)
                {
                    std::ignore = scheduler_loop(token);
                }
            };
        }
        catch (...)
        {
            m_isRunning.store(false, std::memory_order_release);
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode AngelScript::stop()
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            return StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING;
        }

        m_isRunning.store(false, std::memory_order_release);
        m_schedulerThread.request_stop();

        if (m_schedulerThread.joinable())
        {
            m_schedulerThread.join();
        }

        std::scoped_lock lock{m_engineMutex, m_contextsMutex};
        for (auto& ctx : m_contexts | std::views::values)
        {
            clear_yield_state(ctx);
            m_engine->DiscardModule(ctx.moduleName.c_str());
        }
        m_contexts.clear();

        return StatusCode::STATUS_OK;
    }

    bool AngelScript::is_running() const noexcept
    {
        return m_isRunning.load(std::memory_order_acquire);
    }

    std::expected<ContextId, StatusCode> AngelScript::create_context(const std::string_view moduleName,
        const std::filesystem::path& scriptPath,
        BindingPolicy policy)
    {
        if (!m_engine)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED);
        }

        auto fileContent = read_script_file(scriptPath);
        if (!fileContent)
        {
            return std::unexpected(fileContent.error());
        }

        const auto id = m_nextContextId.fetch_add(1, std::memory_order_relaxed);
        auto moduleNameStr = fmt::format("{}_{}", moduleName, id);

        std::unique_lock engineLock{m_engineMutex};

        auto* module = m_engine->GetModule(moduleNameStr.c_str(), asGM_ALWAYS_CREATE);
        if (!module)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_MODULE_CREATION_FAILED);
        }

        if (module->AddScriptSection(moduleNameStr.c_str(),
                                      fileContent->c_str(),
                                      fileContent->size()) < 0)
        {
            m_engine->DiscardModule(moduleNameStr.c_str());
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_COMPILE_FAILED);
        }

        if (module->Build() < 0)
        {
            m_engine->DiscardModule(moduleNameStr.c_str());
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_COMPILE_FAILED);
        }

        auto* entryFunc = module->GetFunctionByDecl("void main()");
        if (!entryFunc)
        {
            m_engine->DiscardModule(moduleNameStr.c_str());
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_PREPARE_FAILED);
        }

        auto* rawCtx = m_engine->CreateContext();
        if (!rawCtx)
        {
            m_engine->DiscardModule(moduleNameStr.c_str());
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_CREATION_FAILED);
        }

        AsContextPtr asCtx{rawCtx};

        if (asCtx->Prepare(entryFunc) < 0)
        {
            m_engine->DiscardModule(moduleNameStr.c_str());
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_PREPARE_FAILED);
        }

        engineLock.unlock();

        ScriptContext scriptCtx{
            .moduleName = std::move(moduleNameStr),
            .module = module,
            .context = std::move(asCtx),
            .state = ScriptState::Running,
            .wakeTime = {},
            .sleepRequested = false,
            .bindingPolicy = std::move(policy)
        };

        const auto moduleNameForCleanup = std::string{scriptCtx.moduleName};

        {
            std::scoped_lock contextLock{m_contextsMutex};
            auto [it, inserted] = m_contexts.emplace(id, std::move(scriptCtx));
            if (inserted) [[likely]]
            {
                it->second.context->SetUserData(&it->second, VERTEX_CONTEXT_DATA_ID);
                return id;
            }
        }

        std::scoped_lock engineGuard{m_engineMutex};
        m_engine->DiscardModule(moduleNameForCleanup.c_str());
        return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_CREATION_FAILED);
    }

    StatusCode AngelScript::remove_context(const ContextId id)
    {
        std::scoped_lock lock{m_engineMutex, m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND;
        }

        if (it->second.state == ScriptState::Executing)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE;
        }

        clear_yield_state(it->second);
        m_engine->DiscardModule(it->second.moduleName.c_str());
        m_contexts.erase(it);
        return StatusCode::STATUS_OK;
    }

    StatusCode AngelScript::suspend_context(const ContextId id)
    {
        std::scoped_lock lock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND;
        }

        auto& ctx = it->second;

        if (ctx.state != ScriptState::Running &&
            ctx.state != ScriptState::Sleeping &&
            ctx.state != ScriptState::Executing)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE;
        }

        ctx.state = ScriptState::Suspended;
        return StatusCode::STATUS_OK;
    }

    StatusCode AngelScript::resume_context(const ContextId id)
    {
        std::scoped_lock lock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND;
        }

        if (it->second.state != ScriptState::Suspended)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE;
        }

        clear_yield_state(it->second);
        it->second.state = ScriptState::Running;
        return StatusCode::STATUS_OK;
    }

    StatusCode AngelScript::set_breakpoint(const ContextId id, const int line)
    {
        if (line <= 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::scoped_lock lock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND;
        }

        if (it->second.state == ScriptState::Finished || it->second.state == ScriptState::Error)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE;
        }

        std::scoped_lock breakpointLock{*it->second.breakpointMutex};
        const auto [_, inserted] = it->second.breakpoints.insert(line);
        return inserted
            ? StatusCode::STATUS_OK
            : StatusCode::STATUS_ERROR_BREAKPOINT_ALREADY_EXISTS;
    }

    StatusCode AngelScript::remove_breakpoint(const ContextId id, const int line)
    {
        if (line <= 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::scoped_lock lock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND;
        }

        std::scoped_lock breakpointLock{*it->second.breakpointMutex};
        return it->second.breakpoints.erase(line) > 0
            ? StatusCode::STATUS_OK
            : StatusCode::STATUS_ERROR_BREAKPOINT_NOT_FOUND;
    }

    std::expected<ScriptState, StatusCode> AngelScript::get_context_state(const ContextId id) const
    {
        std::scoped_lock lock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND);
        }

        return it->second.state;
    }

    std::vector<ContextInfo> AngelScript::get_context_list() const
    {
        std::scoped_lock lock{m_contextsMutex};

        std::vector<ContextInfo> result{};
        result.reserve(m_contexts.size());

        for (const auto& [id, ctx] : m_contexts)
        {
            result.emplace_back(ContextInfo{
                .id = id,
                .name = ctx.moduleName,
                .state = ctx.state
            });
        }

        return result;
    }

    std::expected<std::vector<ContextVariable>, StatusCode> AngelScript::get_context_variables(
        const ContextId id) const
    {
        if (!m_engine)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED);
        }

        std::shared_lock engineLock{m_engineMutex};
        std::scoped_lock contextsLock{m_contextsMutex};

        const auto it = m_contexts.find(id);
        if (it == m_contexts.end())
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND);
        }

        if (it->second.state != ScriptState::Suspended)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_INVALID_STATE);
        }

        auto* context = it->second.context.get();
        if (!context)
        {
            return std::unexpected(StatusCode::STATUS_ERROR_SCRIPT_CONTEXT_NOT_FOUND);
        }

        const int varCount = static_cast<int>(context->GetVarCount());
        if (varCount <= 0)
        {
            return std::vector<ContextVariable>{};
        }

        std::vector<ContextVariable> variables{};
        variables.reserve(static_cast<std::size_t>(varCount));

        for (int index{}; index < varCount; ++index)
        {
            const auto variableIndex = static_cast<asUINT>(index);
            const char* varName{};
            int typeId{};
            context->GetVar(variableIndex, 0, &varName, &typeId);
            auto* address = context->GetAddressOfVar(variableIndex);

            variables.emplace_back(ContextVariable{
                .name = varName ? std::string{varName} : fmt::format("var{}", index),
                .type = format_variable_type_name(*m_engine, typeId),
                .value = format_context_variable_value(*m_engine, typeId, address)
            });
        }

        return variables;
    }

    StatusCode AngelScript::scheduler_loop(const std::stop_token& token)
    {
        std::vector<std::pair<ContextId, ScriptContext*>> runnables{};

        while (!token.stop_requested())
        {
            runnables.clear();

            {
                std::shared_lock engineLock{m_engineMutex};
                std::scoped_lock lock{m_contextsMutex};
                const auto now = std::chrono::steady_clock::now();

                for (auto& [id, ctx] : m_contexts)
                {
                    if (token.stop_requested())
                    {
                        break;
                    }

                    if (ctx.state == ScriptState::Sleeping)
                    {
                        bool shouldWake{};

                        if (ctx.yieldCondition)
                        {
                            const auto condResult = evaluate_yield_condition(ctx.yieldCondition);
                            shouldWake = ctx.yieldInverted ? !condResult : condResult;
                        }
                        else if (ctx.yieldTicks > 0)
                        {
                            shouldWake = (--ctx.yieldTicks == 0);
                        }
                        else if (now >= ctx.wakeTime)
                        {
                            shouldWake = true;
                        }

                        if (shouldWake)
                        {
                            clear_yield_state(ctx);
                            ctx.state = ScriptState::Running;
                        }
                    }

                    if (ctx.state == ScriptState::Running)
                    {
                        ctx.state = ScriptState::Executing;
                        runnables.emplace_back(id, &ctx);
                    }
                }
            }

            if (runnables.empty())
            {
                std::this_thread::sleep_for(IDLE_SLEEP_DURATION);
                continue;
            }

            std::vector<std::pair<ContextId, int>> results{};
            results.reserve(runnables.size());

            {
                std::shared_lock engineLock{m_engineMutex};
                for (auto& [id, ctxPtr] : runnables)
                {
                    {
                        std::scoped_lock lock{m_contextsMutex};
                        if (ctxPtr->state != ScriptState::Executing)
                        {
                            continue;
                        }
                    }
                    results.emplace_back(id, tick_context(*ctxPtr));
                }
            }

            struct BreakpointHitNotification final
            {
                ContextId id{};
                std::string moduleName{};
                int line{};
            };
            std::vector<BreakpointHitNotification> breakpointHits{};

            {
                std::scoped_lock lock{m_contextsMutex};

                for (const auto& [id, execResult] : results)
                {
                    const auto it = m_contexts.find(id);
                    if (it == m_contexts.end())
                    {
                        continue;
                    }

                    if (it->second.state != ScriptState::Executing)
                    {
                        continue;
                    }

                    switch (execResult)
                    {
                        case asEXECUTION_SUSPENDED:
                            if (it->second.sleepRequested)
                            {
                                it->second.sleepRequested = false;
                                it->second.state = ScriptState::Sleeping;
                            }
                            else if (it->second.yieldRequested)
                            {
                                it->second.yieldRequested = false;
                                it->second.state = ScriptState::Sleeping;
                            }
                            else if (it->second.breakpointHit)
                            {
                                it->second.breakpointHit = false;
                                const auto breakpointLine = it->second.breakpointLine;
                                it->second.breakpointLine = 0;
                                it->second.state = ScriptState::Suspended;
                                breakpointHits.push_back(BreakpointHitNotification{
                                    .id = id,
                                    .moduleName = it->second.moduleName,
                                    .line = breakpointLine
                                });
                            }
                            else
                            {
                                it->second.wakeTime = {};
                                it->second.state = ScriptState::Running;
                            }
                            break;
                        case asEXECUTION_FINISHED:
                            it->second.state = ScriptState::Finished;
                            break;
                        default:
                            it->second.state = ScriptState::Error;
                            break;
                    }
                }
            }

            if (!m_messageContext.eventBus || breakpointHits.empty())
            {
                continue;
            }

            for (const auto& hit : breakpointHits)
            {
                const auto outputMessage = fmt::format(
                    "Breakpoint hit in '{}' at line {} (context={})",
                    hit.moduleName,
                    hit.line,
                    hit.id);
                m_messageContext.eventBus->broadcast(Event::ScriptOutputEvent{
                    Event::ScriptOutputSeverity::Info,
                    outputMessage});

                m_messageContext.eventBus->broadcast(Event::ScriptDiagnosticEvent{
                    Event::ScriptDiagnosticSeverity::Warning,
                    hit.moduleName,
                    hit.line,
                    1,
                    fmt::format("Breakpoint hit ({})", hit.moduleName)});
            }
        }

        return StatusCode::STATUS_OK;
    }

    int AngelScript::tick_context(ScriptContext& ctx) const
    {
        {
            std::scoped_lock breakpointLock{*ctx.breakpointMutex};
            ctx.breakpointHit = false;
            ctx.breakpointLine = 0;
        }

        LineCallbackData callbackData{
            .budget = {.deadline = std::chrono::steady_clock::now() + QUANTUM_DURATION},
            .scriptContext = &ctx
        };

        ctx.context->SetLineCallback(asFUNCTION(line_callback), &callbackData, asCALL_CDECL);

        const auto result = ctx.context->Execute();

        if (result == asEXECUTION_EXCEPTION)
        {
            try
            {
                const auto* exceptionMsg = ctx.context->GetExceptionString();
                std::ignore = m_logService.get().log_error(
                    fmt::format("Script exception in '{}': {}",
                                ctx.moduleName,
                                exceptionMsg ? exceptionMsg : "unknown"));
            }
            catch (...)
            {
            }
        }

        return result;
    }

    bool AngelScript::evaluate_yield_condition(asIScriptFunction* condition) const
    {
        if (!condition || !m_evalContext) [[unlikely]]
        {
            return true;
        }

        if (m_evalContext->Prepare(condition) < 0)
        {
            return true;
        }

        const auto result = m_evalContext->Execute();
        if (result != asEXECUTION_FINISHED)
        {
            return true;
        }

        return m_evalContext->GetReturnByte() != 0;
    }

    void AngelScript::clear_yield_state(ScriptContext& ctx)
    {
        if (ctx.yieldCondition)
        {
            std::ignore = ctx.yieldCondition->Release();
            ctx.yieldCondition = nullptr;
        }
        ctx.yieldInverted = false;
        ctx.yieldTicks = 0;
        ctx.yieldRequested = false;
        ctx.wakeTime = {};
        ctx.sleepRequested = false;
        ctx.breakpointHit = false;
        ctx.breakpointLine = 0;
    }
}
