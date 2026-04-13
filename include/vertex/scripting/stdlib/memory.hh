//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//

#pragma once

#include <vertex/runtime/iloader.hh>
#include <vertex/thread/ithreaddispatcher.hh>

#include <sdk/statuscode.h>

#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <vector>

class CScriptArray;
class asIScriptEngine;

namespace Vertex::Scripting::Stdlib
{
    struct ScriptBulkReadEntry final
    {
        std::uint64_t address{};
        std::uint32_t size{};
    };

    struct ScriptBulkReadResult final
    {
        std::int32_t status{static_cast<std::int32_t>(StatusCode::STATUS_ERROR_GENERAL)};
        std::string data{};
    };

    struct ScriptBulkWriteEntry final
    {
        std::uint64_t address{};
        std::string data{};
    };

    struct ScriptBulkWriteResult final
    {
        std::int32_t status{static_cast<std::int32_t>(StatusCode::STATUS_ERROR_GENERAL)};
    };

    struct ScriptMemoryRegion final
    {
        std::string moduleName{};
        std::uint64_t baseAddress{};
        std::uint64_t regionSize{};
    };

    class ScriptMemory final
    {
    public:
        ScriptMemory(Runtime::ILoader& loader, Thread::IThreadDispatcher& dispatcher);

        [[nodiscard]] StatusCode register_api(asIScriptEngine& engine);

        [[nodiscard]] StatusCode read_memory(std::uint64_t address, std::uint32_t size, std::string& out) const;
        [[nodiscard]] StatusCode write_memory(std::uint64_t address, const std::string& data) const;

        [[nodiscard]] StatusCode bulk_read(CScriptArray* entries, CScriptArray*& outResults) const;
        [[nodiscard]] StatusCode bulk_write(CScriptArray* entries, CScriptArray*& outResults) const;

        [[nodiscard]] StatusCode allocate_memory(std::uint64_t address, std::uint64_t size, std::uint64_t& out) const;
        [[nodiscard]] StatusCode free_memory(std::uint64_t address, std::uint64_t size) const;

        [[nodiscard]] StatusCode get_pointer_size(std::uint64_t& out) const;
        [[nodiscard]] StatusCode get_min_address(std::uint64_t& out) const;
        [[nodiscard]] StatusCode get_max_address(std::uint64_t& out) const;

        [[nodiscard]] StatusCode refresh_memory_regions();
        [[nodiscard]] std::uint32_t get_region_count() const;
        [[nodiscard]] ScriptMemoryRegion get_region_at(std::uint32_t index) const;

    private:
        std::reference_wrapper<Runtime::ILoader> m_loader;
        std::reference_wrapper<Thread::IThreadDispatcher> m_dispatcher;

        mutable std::mutex m_regionsMutex{};
        std::vector<ScriptMemoryRegion> m_regions{};

        [[nodiscard]] static StatusCode register_bulk_types(asIScriptEngine& engine);
        [[nodiscard]] static StatusCode register_memory_region_type(asIScriptEngine& engine);
    };
}
