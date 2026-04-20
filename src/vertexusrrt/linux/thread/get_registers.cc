//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/linux/lldb_backend.hh>
#include <sdk/api.h>

#include <lldb/API/LLDB.h>

#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string_view>

namespace
{
    [[nodiscard]] RegisterCategory classify_register_set(std::string_view setName) noexcept
    {
        if (setName.find("Flags") != std::string_view::npos
         || setName.find("flags") != std::string_view::npos)
        {
            return VERTEX_REG_FLAGS;
        }
        if (setName.find("Segment") != std::string_view::npos
         || setName.find("segment") != std::string_view::npos)
        {
            return VERTEX_REG_SEGMENT;
        }
        if (setName.find("Floating") != std::string_view::npos
         || setName.find("FPU") != std::string_view::npos
         || setName.find("fpu") != std::string_view::npos)
        {
            return VERTEX_REG_FLOATING_POINT;
        }
        if (setName.find("Vector") != std::string_view::npos
         || setName.find("SSE") != std::string_view::npos
         || setName.find("AVX") != std::string_view::npos
         || setName.find("XMM") != std::string_view::npos
         || setName.find("YMM") != std::string_view::npos
         || setName.find("ZMM") != std::string_view::npos)
        {
            return VERTEX_REG_VECTOR;
        }
        if (setName.find("Debug") != std::string_view::npos
         || setName.find("debug") != std::string_view::npos)
        {
            return VERTEX_REG_DEBUG;
        }
        if (setName.find("Control") != std::string_view::npos
         || setName.find("control") != std::string_view::npos)
        {
            return VERTEX_REG_CONTROL;
        }
        return VERTEX_REG_GENERAL;
    }

    void copy_name(char* dest, std::size_t capacity, std::string_view src) noexcept
    {
        if (capacity == 0)
        {
            return;
        }
        const auto copyLen = std::min(src.size(), capacity - 1);
        std::copy_n(src.data(), copyLen, dest);
        dest[copyLen] = '\0';
    }

    [[nodiscard]] std::uint8_t byte_size_to_bit_width(std::uint32_t byteSize) noexcept
    {
        constexpr std::uint32_t MAX_BYTES = 32;
        const auto clamped = std::min(byteSize, MAX_BYTES);
        return static_cast<std::uint8_t>(clamped * 8U);
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_registers(const uint32_t threadId, RegisterSet* registers)
    {
        if (registers == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::memset(registers, 0, sizeof(RegisterSet));

        auto& state = Debugger::get_backend_state();
        if (!state.process.IsValid())
        {
            return StatusCode::STATUS_ERROR_DEBUGGER_NOT_ATTACHED;
        }

        lldb::SBThread thread = state.process.GetThreadByID(threadId);
        if (!thread.IsValid())
        {
            const auto numThreads = state.process.GetNumThreads();
            for (std::uint32_t i = 0; i < numThreads; ++i)
            {
                auto candidate = state.process.GetThreadAtIndex(i);
                if (candidate.IsValid() && static_cast<std::uint32_t>(candidate.GetThreadID()) == threadId)
                {
                    thread = candidate;
                    break;
                }
            }
        }

        if (!thread.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_ID;
        }

        auto frame = thread.GetFrameAtIndex(0);
        if (!frame.IsValid())
        {
            return StatusCode::STATUS_ERROR_THREAD_CONTEXT_FAILED;
        }

        registers->instructionPointer = frame.GetPC();
        registers->stackPointer = frame.GetSP();
        registers->basePointer = frame.GetFP();

        auto registerSets = frame.GetRegisters();
        const auto setCount = registerSets.GetSize();
        std::uint32_t outIndex = 0;

        for (std::uint32_t i = 0; i < setCount && outIndex < VERTEX_MAX_REGISTERS; ++i)
        {
            auto regSet = registerSets.GetValueAtIndex(i);
            if (!regSet.IsValid())
            {
                continue;
            }

            const char* setNameCStr = regSet.GetName();
            const std::string_view setName{setNameCStr != nullptr ? setNameCStr : ""};
            const auto category = classify_register_set(setName);

            const auto childCount = regSet.GetNumChildren();
            for (std::uint32_t c = 0; c < childCount && outIndex < VERTEX_MAX_REGISTERS; ++c)
            {
                auto regValue = regSet.GetChildAtIndex(c);
                if (!regValue.IsValid())
                {
                    continue;
                }

                const char* regNameCStr = regValue.GetName();
                if (regNameCStr == nullptr)
                {
                    continue;
                }

                Register& out = registers->registers[outIndex];
                copy_name(out.name, VERTEX_MAX_REGISTER_NAME_LENGTH, regNameCStr);
                out.category = category;
                out.value = regValue.GetValueAsUnsigned(0);
                out.previousValue = 0;
                out.bitWidth = byte_size_to_bit_width(regValue.GetByteSize());
                out.modified = 0;

                if (category == VERTEX_REG_FLAGS && registers->flagsRegister == 0)
                {
                    registers->flagsRegister = out.value;
                }

                ++outIndex;
            }
        }

        registers->registerCount = outIndex;
        return StatusCode::STATUS_OK;
    }
}
