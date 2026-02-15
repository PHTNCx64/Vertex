//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/event/vertexevent.hh>
#include <vertex/event/eventid.hh>
#include <vertex/debugger/debuggertypes.hh>

namespace Vertex::Event
{
    class DebuggerStateChangedEvent final : public VertexEvent
    {
    public:
        explicit DebuggerStateChangedEvent(const Debugger::DebuggerState newState)
            : VertexEvent(DEBUGGER_STATE_CHANGED_EVENT), m_state(newState)
        {}

        [[nodiscard]] Debugger::DebuggerState get_state() const noexcept
        {
            return m_state;
        }

    private:
        Debugger::DebuggerState m_state {};
    };

    class DebuggerBreakpointHitEvent final : public VertexEvent
    {
    public:
        DebuggerBreakpointHitEvent(const std::uint64_t address, const std::uint32_t breakpointId)
            : VertexEvent(DEBUGGER_BREAKPOINT_HIT_EVENT),
              m_address(address), m_breakpointId(breakpointId)
        {}

        [[nodiscard]] std::uint64_t get_address() const noexcept
        {
            return m_address;
        }

        [[nodiscard]] std::uint32_t get_breakpoint_id() const noexcept
        {
            return m_breakpointId;
        }

    private:
        std::uint64_t m_address {};
        std::uint32_t m_breakpointId {};
    };

    class DebuggerStepCompleteEvent final : public VertexEvent
    {
    public:
        explicit DebuggerStepCompleteEvent(const std::uint64_t address)
            : VertexEvent(DEBUGGER_STEP_COMPLETE_EVENT), m_address(address)
        {}

        [[nodiscard]] std::uint64_t get_address() const noexcept
        {
            return m_address;
        }

    private:
        std::uint64_t m_address {};
    };

    class DebuggerMemoryChangedEvent final : public VertexEvent
    {
    public:
        DebuggerMemoryChangedEvent(const std::uint64_t address, const std::size_t size)
            : VertexEvent(DEBUGGER_MEMORY_CHANGED_EVENT),
              m_address(address), m_size(size)
        {}

        [[nodiscard]] std::uint64_t get_address() const noexcept
        {
            return m_address;
        }

        [[nodiscard]] std::size_t get_size() const noexcept
        {
            return m_size;
        }

    private:
        std::uint64_t m_address {};
        std::size_t m_size {};
    };

    class DebuggerRegisterChangedEvent final : public VertexEvent
    {
    public:
        DebuggerRegisterChangedEvent(std::string registerName, const std::uint64_t newValue)
            : VertexEvent(DEBUGGER_REGISTER_CHANGED_EVENT),
              m_registerName(std::move(registerName)), m_newValue(newValue)
        {}

        [[nodiscard]] const std::string& get_register_name() const noexcept
        {
            return m_registerName;
        }

        [[nodiscard]] std::uint64_t get_new_value() const noexcept
        {
            return m_newValue;
        }

    private:
        std::string m_registerName {};
        std::uint64_t m_newValue {};
    };

    class DebuggerModuleLoadedEvent final : public VertexEvent
    {
    public:
        DebuggerModuleLoadedEvent(std::string moduleName, const std::uint64_t baseAddress)
            : VertexEvent(DEBUGGER_MODULE_LOADED_EVENT),
              m_moduleName(std::move(moduleName)), m_baseAddress(baseAddress)
        {}

        [[nodiscard]] const std::string& get_module_name() const noexcept
        {
            return m_moduleName;
        }

        [[nodiscard]] std::uint64_t get_base_address() const noexcept
        {
            return m_baseAddress;
        }

    private:
        std::string m_moduleName {};
        std::uint64_t m_baseAddress {};
    };
}
