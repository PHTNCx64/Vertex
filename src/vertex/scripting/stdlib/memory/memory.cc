//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#include <vertex/scripting/stdlib/memory.hh>
#include <vertex/scripting/scriptarray.h>
#include <vertex/runtime/caller.hh>

#include <sdk/memory.h>

#include <angelscript.h>

#include <algorithm>
#include <cstdlib>
#include <limits>

namespace Vertex::Scripting::Stdlib
{
    static void region_default_construct(void* memory)
    {
        new (memory) ScriptMemoryRegion();
    }

    static void region_copy_construct(const ScriptMemoryRegion& other, void* memory)
    {
        new (memory) ScriptMemoryRegion(other);
    }

    static void region_destruct(ScriptMemoryRegion* self)
    {
        self->~ScriptMemoryRegion();
    }

    static ScriptMemoryRegion& region_assign(const ScriptMemoryRegion& other, ScriptMemoryRegion* self)
    {
        *self = other;
        return *self;
    }

    static void bulk_read_entry_default_construct(void* memory)
    {
        new (memory) ScriptBulkReadEntry();
    }

    static void bulk_read_entry_copy_construct(const ScriptBulkReadEntry& other, void* memory)
    {
        new (memory) ScriptBulkReadEntry(other);
    }

    static void bulk_read_entry_construct(const std::uint64_t address, const std::uint32_t size, void* memory)
    {
        new (memory) ScriptBulkReadEntry{address, size};
    }

    static void bulk_read_entry_destruct(ScriptBulkReadEntry* self)
    {
        self->~ScriptBulkReadEntry();
    }

    static ScriptBulkReadEntry& bulk_read_entry_assign(const ScriptBulkReadEntry& other, ScriptBulkReadEntry* self)
    {
        *self = other;
        return *self;
    }

    static void bulk_read_result_default_construct(void* memory)
    {
        new (memory) ScriptBulkReadResult();
    }

    static void bulk_read_result_copy_construct(const ScriptBulkReadResult& other, void* memory)
    {
        new (memory) ScriptBulkReadResult(other);
    }

    static void bulk_read_result_destruct(ScriptBulkReadResult* self)
    {
        self->~ScriptBulkReadResult();
    }

    static ScriptBulkReadResult& bulk_read_result_assign(const ScriptBulkReadResult& other, ScriptBulkReadResult* self)
    {
        *self = other;
        return *self;
    }

    static void bulk_write_entry_default_construct(void* memory)
    {
        new (memory) ScriptBulkWriteEntry();
    }

    static void bulk_write_entry_copy_construct(const ScriptBulkWriteEntry& other, void* memory)
    {
        new (memory) ScriptBulkWriteEntry(other);
    }

    static void bulk_write_entry_construct(const std::uint64_t address, const std::string& data, void* memory)
    {
        new (memory) ScriptBulkWriteEntry{address, data};
    }

    static void bulk_write_entry_destruct(ScriptBulkWriteEntry* self)
    {
        self->~ScriptBulkWriteEntry();
    }

    static ScriptBulkWriteEntry& bulk_write_entry_assign(const ScriptBulkWriteEntry& other, ScriptBulkWriteEntry* self)
    {
        *self = other;
        return *self;
    }

    static void bulk_write_result_default_construct(void* memory)
    {
        new (memory) ScriptBulkWriteResult();
    }

    static void bulk_write_result_copy_construct(const ScriptBulkWriteResult& other, void* memory)
    {
        new (memory) ScriptBulkWriteResult(other);
    }

    static void bulk_write_result_destruct(ScriptBulkWriteResult* self)
    {
        self->~ScriptBulkWriteResult();
    }

    static ScriptBulkWriteResult& bulk_write_result_assign(const ScriptBulkWriteResult& other, ScriptBulkWriteResult* self)
    {
        *self = other;
        return *self;
    }

    static void set_bulk_read_status(CScriptArray* results, const StatusCode status)
    {
        if (!results)
        {
            return;
        }

        const auto rawStatus = static_cast<std::int32_t>(status);
        for (asUINT index{}; index < results->GetSize(); ++index)
        {
            auto* entry = static_cast<ScriptBulkReadResult*>(results->At(index));
            if (!entry)
            {
                continue;
            }
            entry->status = rawStatus;
            entry->data.clear();
        }
    }

    static void set_bulk_write_status(CScriptArray* results, const StatusCode status)
    {
        if (!results)
        {
            return;
        }

        const auto rawStatus = static_cast<std::int32_t>(status);
        for (asUINT index{}; index < results->GetSize(); ++index)
        {
            auto* entry = static_cast<ScriptBulkWriteResult*>(results->At(index));
            if (!entry)
            {
                continue;
            }
            entry->status = rawStatus;
        }
    }

    ScriptMemory::ScriptMemory(Runtime::ILoader& loader, Thread::IThreadDispatcher& dispatcher)
        : m_loader{loader}, m_dispatcher{dispatcher}
    {
    }

    StatusCode ScriptMemory::register_api(asIScriptEngine& engine)
    {
        if (const auto status = register_bulk_types(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (engine.RegisterGlobalFunction(
                "int read_memory(uint64, uint, string &out)",
                asMETHOD(ScriptMemory, read_memory),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int write_memory(uint64, const string &in)",
                asMETHOD(ScriptMemory, write_memory),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int bulk_read(array<BulkReadEntry>@, array<BulkReadResult>@ &out)",
                asMETHOD(ScriptMemory, bulk_read),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int bulk_write(array<BulkWriteEntry>@, array<BulkWriteResult>@ &out)",
                asMETHOD(ScriptMemory, bulk_write),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int allocate_memory(uint64, uint64, uint64 &out)",
                asMETHOD(ScriptMemory, allocate_memory),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int free_memory(uint64, uint64)",
                asMETHOD(ScriptMemory, free_memory),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int get_pointer_size(uint64 &out)",
                asMETHOD(ScriptMemory, get_pointer_size),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int get_min_address(uint64 &out)",
                asMETHOD(ScriptMemory, get_min_address),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "int get_max_address(uint64 &out)",
                asMETHOD(ScriptMemory, get_max_address),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (const auto status = register_memory_region_type(engine);
            status != StatusCode::STATUS_OK)
        {
            return status;
        }

        if (engine.RegisterGlobalFunction(
                "int refresh_memory_regions()",
                asMETHOD(ScriptMemory, refresh_memory_regions),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "uint get_region_count()",
                asMETHOD(ScriptMemory, get_region_count),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterGlobalFunction(
                "MemoryRegion get_region_at(uint)",
                asMETHOD(ScriptMemory, get_region_at),
                asCALL_THISCALL_ASGLOBAL, this) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptMemory::read_memory(const std::uint64_t address, const std::uint32_t size, std::string& out) const
    {
        out.clear();

        if (size == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::string buffer(size, '\0');

        std::packaged_task<StatusCode()> task(
            [this, address, size, &buffer]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_read_process,
                                       address, static_cast<std::uint64_t>(size), buffer.data()));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status == StatusCode::STATUS_OK)
        {
            out = std::move(buffer);
        }

        return status;
    }

    StatusCode ScriptMemory::write_memory(const std::uint64_t address, const std::string& data) const
    {
        if (data.empty())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

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
            [this, address, &data]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_write_process,
                                       address, static_cast<std::uint64_t>(data.size()), data.data()));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptMemory::bulk_read(CScriptArray* entries, CScriptArray*& outResults) const
    {
        if (outResults)
        {
            outResults->Release();
            outResults = nullptr;
        }

        if (!entries)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (entries->GetSize() > std::numeric_limits<std::uint32_t>::max())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto* engine = entries->GetArrayObjectType() ? entries->GetArrayObjectType()->GetEngine() : nullptr;
        if (!engine)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        auto* resultArrayType = engine->GetTypeInfoByDecl("array<BulkReadResult>");
        if (!resultArrayType)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        outResults = CScriptArray::Create(resultArrayType, entries->GetSize());
        if (!outResults)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        if (entries->GetSize() == 0)
        {
            return StatusCode::STATUS_OK;
        }

        std::vector<ScriptBulkReadEntry> nativeEntries(entries->GetSize());
        for (asUINT index{}; index < entries->GetSize(); ++index)
        {
            const auto* entry = static_cast<const ScriptBulkReadEntry*>(entries->At(index));
            if (!entry)
            {
                set_bulk_read_status(outResults, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            nativeEntries[index] = *entry;
        }

        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            set_bulk_read_status(outResults, StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            set_bulk_read_status(outResults, StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::vector<std::vector<char>> buffers(nativeEntries.size());
        std::vector<::BulkReadRequest> requests(nativeEntries.size());
        std::vector<::BulkReadResult> results(nativeEntries.size());

        for (std::size_t index{}; index < nativeEntries.size(); ++index)
        {
            buffers[index].resize(nativeEntries[index].size);
            requests[index] = {
                nativeEntries[index].address,
                nativeEntries[index].size,
                buffers[index].data()
            };
        }

        std::packaged_task<StatusCode()> task(
            [this, &requests, &results]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                auto& plugin = ref->get();

                auto maxPerCall = static_cast<std::uint32_t>(requests.size());
                std::uint32_t queriedLimit{};
                const auto limitResult = Runtime::safe_call(plugin.internal_vertex_memory_get_bulk_request_limit, &queriedLimit);
                if (Runtime::status_ok(limitResult) && queriedLimit > 0)
                {
                    maxPerCall = std::min(maxPerCall, queriedLimit);
                }
                maxPerCall = std::max<std::uint32_t>(1, maxPerCall);

                std::size_t offset{};
                while (offset < requests.size())
                {
                    const auto remaining = requests.size() - offset;
                    const auto chunkCount = static_cast<std::uint32_t>(std::min<std::size_t>(remaining, maxPerCall));
                    const auto result = Runtime::safe_call(
                        plugin.internal_vertex_memory_read_process_bulk,
                        requests.data() + offset, results.data() + offset, chunkCount);
                    if (!Runtime::status_ok(result))
                    {
                        return Runtime::get_status(result);
                    }
                    offset += chunkCount;
                }

                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            const auto status = dispatchResult.error();
            set_bulk_read_status(outResults, status);
            return status;
        }

        const auto overallStatus = dispatchResult.value().get();
        if (overallStatus != StatusCode::STATUS_OK)
        {
            set_bulk_read_status(outResults, overallStatus);
            return overallStatus;
        }

        for (asUINT index{}; index < outResults->GetSize(); ++index)
        {
            auto* resultEntry = static_cast<ScriptBulkReadResult*>(outResults->At(index));
            if (!resultEntry)
            {
                continue;
            }

            resultEntry->status = static_cast<std::int32_t>(results[index].status);
            if (results[index].status == StatusCode::STATUS_OK)
            {
                resultEntry->data.assign(buffers[index].data(), buffers[index].size());
            }
            else
            {
                resultEntry->data.clear();
            }
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptMemory::bulk_write(CScriptArray* entries, CScriptArray*& outResults) const
    {
        if (outResults)
        {
            outResults->Release();
            outResults = nullptr;
        }

        if (!entries)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        if (entries->GetSize() > std::numeric_limits<std::uint32_t>::max())
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto* engine = entries->GetArrayObjectType() ? entries->GetArrayObjectType()->GetEngine() : nullptr;
        if (!engine)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        auto* resultArrayType = engine->GetTypeInfoByDecl("array<BulkWriteResult>");
        if (!resultArrayType)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        outResults = CScriptArray::Create(resultArrayType, entries->GetSize());
        if (!outResults)
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        if (entries->GetSize() == 0)
        {
            return StatusCode::STATUS_OK;
        }

        std::vector<ScriptBulkWriteEntry> nativeEntries(entries->GetSize());
        for (asUINT index{}; index < entries->GetSize(); ++index)
        {
            const auto* entry = static_cast<const ScriptBulkWriteEntry*>(entries->At(index));
            if (!entry || entry->data.empty())
            {
                set_bulk_write_status(outResults, StatusCode::STATUS_ERROR_INVALID_PARAMETER);
                return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
            }
            nativeEntries[index] = *entry;
        }

        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            set_bulk_write_status(outResults, StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            set_bulk_write_status(outResults, StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED);
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::vector<::BulkWriteRequest> requests(nativeEntries.size());
        std::vector<::BulkWriteResult> results(nativeEntries.size());

        for (std::size_t index{}; index < nativeEntries.size(); ++index)
        {
            requests[index] = {
                nativeEntries[index].address,
                nativeEntries[index].data.size(),
                nativeEntries[index].data.data()
            };
        }

        std::packaged_task<StatusCode()> task(
            [this, &requests, &results]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                const auto& plugin = ref->get();

                auto maxPerCall = static_cast<std::uint32_t>(requests.size());
                std::uint32_t queriedLimit{};
                const auto limitResult = Runtime::safe_call(plugin.internal_vertex_memory_get_bulk_request_limit, &queriedLimit);
                if (Runtime::status_ok(limitResult) && queriedLimit > 0)
                {
                    maxPerCall = std::min(maxPerCall, queriedLimit);
                }
                maxPerCall = std::max<std::uint32_t>(1, maxPerCall);

                std::size_t offset{};
                while (offset < requests.size())
                {
                    const auto remaining = requests.size() - offset;
                    const auto chunkCount = static_cast<std::uint32_t>(std::min<std::size_t>(remaining, maxPerCall));
                    const auto result = Runtime::safe_call(
                        plugin.internal_vertex_memory_write_process_bulk,
                        requests.data() + offset, results.data() + offset, chunkCount);
                    if (!Runtime::status_ok(result))
                    {
                        return Runtime::get_status(result);
                    }
                    offset += chunkCount;
                }

                return StatusCode::STATUS_OK;
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            const auto status = dispatchResult.error();
            set_bulk_write_status(outResults, status);
            return status;
        }

        const auto overallStatus = dispatchResult.value().get();
        if (overallStatus != StatusCode::STATUS_OK)
        {
            set_bulk_write_status(outResults, overallStatus);
            return overallStatus;
        }

        for (asUINT index{}; index < outResults->GetSize(); ++index)
        {
            auto* resultEntry = static_cast<ScriptBulkWriteResult*>(outResults->At(index));
            if (!resultEntry)
            {
                continue;
            }
            resultEntry->status = static_cast<std::int32_t>(results[index].status);
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptMemory::allocate_memory(const std::uint64_t address, const std::uint64_t size, std::uint64_t& out) const
    {
        out = 0;

        if (m_loader.get().has_plugin_loaded() != StatusCode::STATUS_OK)
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
        }

        const auto pluginRef = m_loader.get().get_active_plugin();
        if (!pluginRef || !pluginRef->get().is_loaded())
        {
            return StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED;
        }

        std::uint64_t targetAddress{};

        std::packaged_task<StatusCode()> task(
            [this, address, size, &targetAddress]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_allocate,
                                       address, size, nullptr, static_cast<std::size_t>(0), &targetAddress));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status == StatusCode::STATUS_OK)
        {
            out = targetAddress;
        }

        return status;
    }

    StatusCode ScriptMemory::free_memory(const std::uint64_t address, const std::uint64_t size) const
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
            [this, address, size]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_free, address, size));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptMemory::get_pointer_size(std::uint64_t& out) const
    {
        out = 0;

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
            [this, &out]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_get_process_pointer_size, &out));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptMemory::get_min_address(std::uint64_t& out) const
    {
        out = 0;

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
            [this, &out]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_get_min_process_address, &out));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptMemory::get_max_address(std::uint64_t& out) const
    {
        out = 0;

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
            [this, &out]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_get_max_process_address, &out));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        return dispatchResult.value().get();
    }

    StatusCode ScriptMemory::register_bulk_types(asIScriptEngine& engine)
    {
        if (engine.RegisterObjectType(
                "BulkReadEntry", sizeof(ScriptBulkReadEntry),
                asOBJ_VALUE | asGetTypeTraits<ScriptBulkReadEntry>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadEntry", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(bulk_read_entry_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadEntry", asBEHAVE_CONSTRUCT, "void f(const BulkReadEntry &in)",
                asFUNCTION(bulk_read_entry_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadEntry", asBEHAVE_CONSTRUCT, "void f(uint64, uint)",
                asFUNCTION(bulk_read_entry_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadEntry", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(bulk_read_entry_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "BulkReadEntry", "BulkReadEntry &opAssign(const BulkReadEntry &in)",
                asFUNCTION(bulk_read_entry_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkReadEntry", "uint64 address", asOFFSET(ScriptBulkReadEntry, address)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkReadEntry", "uint size", asOFFSET(ScriptBulkReadEntry, size)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectType(
                "BulkReadResult", sizeof(ScriptBulkReadResult),
                asOBJ_VALUE | asGetTypeTraits<ScriptBulkReadResult>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadResult", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(bulk_read_result_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadResult", asBEHAVE_CONSTRUCT, "void f(const BulkReadResult &in)",
                asFUNCTION(bulk_read_result_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkReadResult", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(bulk_read_result_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "BulkReadResult", "BulkReadResult &opAssign(const BulkReadResult &in)",
                asFUNCTION(bulk_read_result_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkReadResult", "int status", asOFFSET(ScriptBulkReadResult, status)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkReadResult", "string data", asOFFSET(ScriptBulkReadResult, data)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectType(
                "BulkWriteEntry", sizeof(ScriptBulkWriteEntry),
                asOBJ_VALUE | asGetTypeTraits<ScriptBulkWriteEntry>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteEntry", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(bulk_write_entry_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteEntry", asBEHAVE_CONSTRUCT, "void f(const BulkWriteEntry &in)",
                asFUNCTION(bulk_write_entry_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteEntry", asBEHAVE_CONSTRUCT, "void f(uint64, const string &in)",
                asFUNCTION(bulk_write_entry_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteEntry", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(bulk_write_entry_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "BulkWriteEntry", "BulkWriteEntry &opAssign(const BulkWriteEntry &in)",
                asFUNCTION(bulk_write_entry_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkWriteEntry", "uint64 address", asOFFSET(ScriptBulkWriteEntry, address)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkWriteEntry", "string data", asOFFSET(ScriptBulkWriteEntry, data)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectType(
                "BulkWriteResult", sizeof(ScriptBulkWriteResult),
                asOBJ_VALUE | asGetTypeTraits<ScriptBulkWriteResult>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteResult", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(bulk_write_result_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteResult", asBEHAVE_CONSTRUCT, "void f(const BulkWriteResult &in)",
                asFUNCTION(bulk_write_result_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "BulkWriteResult", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(bulk_write_result_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "BulkWriteResult", "BulkWriteResult &opAssign(const BulkWriteResult &in)",
                asFUNCTION(bulk_write_result_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("BulkWriteResult", "int status", asOFFSET(ScriptBulkWriteResult, status)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptMemory::register_memory_region_type(asIScriptEngine& engine)
    {
        if (engine.RegisterObjectType(
                "MemoryRegion", sizeof(ScriptMemoryRegion),
                asOBJ_VALUE | asGetTypeTraits<ScriptMemoryRegion>()) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "MemoryRegion", asBEHAVE_CONSTRUCT, "void f()",
                asFUNCTION(region_default_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "MemoryRegion", asBEHAVE_CONSTRUCT, "void f(const MemoryRegion &in)",
                asFUNCTION(region_copy_construct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectBehaviour(
                "MemoryRegion", asBEHAVE_DESTRUCT, "void f()",
                asFUNCTION(region_destruct), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectMethod(
                "MemoryRegion", "MemoryRegion &opAssign(const MemoryRegion &in)",
                asFUNCTION(region_assign), asCALL_CDECL_OBJLAST) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("MemoryRegion", "string moduleName", asOFFSET(ScriptMemoryRegion, moduleName)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("MemoryRegion", "uint64 baseAddress", asOFFSET(ScriptMemoryRegion, baseAddress)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        if (engine.RegisterObjectProperty("MemoryRegion", "uint64 regionSize", asOFFSET(ScriptMemoryRegion, regionSize)) < 0)
        {
            return StatusCode::STATUS_ERROR_SCRIPT_ENGINE_INIT_FAILED;
        }

        return StatusCode::STATUS_OK;
    }

    StatusCode ScriptMemory::refresh_memory_regions()
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

        MemoryRegion* rawRegions{};
        std::uint64_t regionCount{};

        std::packaged_task<StatusCode()> task(
            [this, &rawRegions, &regionCount]() -> StatusCode
            {
                const auto ref = m_loader.get().get_active_plugin();
                if (!ref)
                {
                    return StatusCode::STATUS_ERROR_PLUGIN_NOT_ACTIVE;
                }
                return Runtime::get_status(
                    Runtime::safe_call(ref->get().internal_vertex_memory_query_regions, &rawRegions, &regionCount));
            });

        auto dispatchResult = m_dispatcher.get().dispatch(Thread::ThreadChannel::Scanner, std::move(task));
        if (!dispatchResult.has_value())
        {
            return dispatchResult.error();
        }

        const auto status = dispatchResult.value().get();
        if (status != StatusCode::STATUS_OK)
        {
            return status;
        }

        std::vector<ScriptMemoryRegion> converted{};
        converted.reserve(regionCount);
        for (std::uint64_t i{}; i < regionCount; ++i)
        {
            converted.emplace_back(ScriptMemoryRegion{
                .moduleName = rawRegions[i].baseModuleName ? rawRegions[i].baseModuleName : std::string{},
                .baseAddress = rawRegions[i].baseAddress,
                .regionSize = rawRegions[i].regionSize
            });
        }

        std::free(rawRegions);

        {
            std::scoped_lock lock{m_regionsMutex};
            m_regions = std::move(converted);
        }

        return StatusCode::STATUS_OK;
    }

    std::uint32_t ScriptMemory::get_region_count() const
    {
        std::scoped_lock lock{m_regionsMutex};
        return static_cast<std::uint32_t>(m_regions.size());
    }

    ScriptMemoryRegion ScriptMemory::get_region_at(const std::uint32_t index) const
    {
        std::scoped_lock lock{m_regionsMutex};
        if (index >= m_regions.size())
        {
            return {};
        }
        return m_regions[index];
    }
}
