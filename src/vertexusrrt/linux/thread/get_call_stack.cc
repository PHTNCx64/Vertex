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
#include <string>
#include <string_view>

namespace
{
    void copy_string(char* dest, std::size_t capacity, std::string_view src) noexcept
    {
        if (capacity == 0)
        {
            return;
        }
        const auto copyLen = std::min(src.size(), capacity - 1);
        std::copy_n(src.data(), copyLen, dest);
        dest[copyLen] = '\0';
    }

    [[nodiscard]] std::string resolve_function_name(lldb::SBFrame& frame)
    {
        if (auto fn = frame.GetFunction(); fn.IsValid())
        {
            if (const char* name = fn.GetDisplayName(); name != nullptr)
            {
                return name;
            }
            if (const char* name = fn.GetName(); name != nullptr)
            {
                return name;
            }
        }
        if (auto sym = frame.GetSymbol(); sym.IsValid())
        {
            if (const char* name = sym.GetDisplayName(); name != nullptr)
            {
                return name;
            }
            if (const char* name = sym.GetName(); name != nullptr)
            {
                return name;
            }
        }
        if (const char* name = frame.GetFunctionName(); name != nullptr)
        {
            return name;
        }
        return {};
    }

    [[nodiscard]] std::string resolve_module_name(lldb::SBFrame& frame)
    {
        auto mod = frame.GetModule();
        if (!mod.IsValid())
        {
            return {};
        }
        const auto fileSpec = mod.GetFileSpec();
        if (!fileSpec.IsValid())
        {
            return {};
        }
        if (const char* file = fileSpec.GetFilename(); file != nullptr)
        {
            return file;
        }
        return {};
    }

    [[nodiscard]] std::string resolve_source_file(lldb::SBFrame& frame)
    {
        auto lineEntry = frame.GetLineEntry();
        if (!lineEntry.IsValid())
        {
            return {};
        }
        const auto fileSpec = lineEntry.GetFileSpec();
        if (!fileSpec.IsValid())
        {
            return {};
        }
        const char* dir = fileSpec.GetDirectory();
        const char* file = fileSpec.GetFilename();
        if (dir != nullptr && file != nullptr)
        {
            std::string result{dir};
            result.append("/");
            result.append(file);
            return result;
        }
        if (file != nullptr)
        {
            return file;
        }
        return {};
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_call_stack(const uint32_t threadId, CallStack* callStack)
    {
        if (callStack == nullptr)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        std::memset(callStack, 0, sizeof(CallStack));

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

        const auto rawFrameCount = thread.GetNumFrames();
        const auto frameCount = std::min<std::uint32_t>(rawFrameCount, VERTEX_MAX_STACK_FRAMES);

        for (std::uint32_t i = 0; i < frameCount; ++i)
        {
            auto frame = thread.GetFrameAtIndex(i);
            if (!frame.IsValid())
            {
                continue;
            }

            StackFrame& out = callStack->frames[i];
            out.frameIndex = i;
            out.returnAddress = frame.GetPC();
            out.framePointer = frame.GetFP();
            out.stackPointer = frame.GetSP();

            const auto functionName = resolve_function_name(frame);
            const auto moduleName = resolve_module_name(frame);
            const auto sourceFile = resolve_source_file(frame);

            copy_string(out.functionName, VERTEX_MAX_FUNCTION_NAME_LENGTH, functionName);
            copy_string(out.moduleName, VERTEX_MAX_NAME_LENGTH, moduleName);
            copy_string(out.sourceFile, VERTEX_MAX_SOURCE_FILE_LENGTH, sourceFile);

            auto lineEntry = frame.GetLineEntry();
            out.sourceLine = lineEntry.IsValid() ? lineEntry.GetLine() : 0;
        }

        callStack->frameCount = frameCount;
        callStack->currentFrameIndex = 0;

        return StatusCode::STATUS_OK;
    }
}
