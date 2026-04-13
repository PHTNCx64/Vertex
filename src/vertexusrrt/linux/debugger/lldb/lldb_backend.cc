//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexusrrt/linux/lldb_backend.hh>

#include <array>
#include <cstdio>
#include <cstdlib>
#include <format>
#include <string>

namespace Debugger
{
    namespace
    {
        LldbBackendState g_backendState{};
        bool g_lldbInitialized{false};

        [[nodiscard]] std::string resolve_lldb_server_path()
        {
            const std::unique_ptr<FILE, decltype(&pclose)> pipe{popen("which lldb-server", "r"), pclose};
            if (!pipe)
            {
                return {};
            }

            std::array<char, 512> buffer{};
            std::string result{};
            while (fgets(buffer.data(), buffer.size(), pipe.get()) != nullptr)
            {
                result += buffer.data();
            }

            while (!result.empty() && (result.back() == '\n' || result.back() == '\r'))
            {
                result.pop_back();
            }

            return result; // NOLINT
        }
    }

    void initialize_lldb()
    {
        if (g_lldbInitialized)
        {
            return;
        }

        const auto serverPath = resolve_lldb_server_path();
        if (!serverPath.empty())
        {
            setenv("LLDB_DEBUGSERVER_PATH", serverPath.c_str(), 0);
        }

        lldb::SBDebugger::Initialize();
        g_backendState.debugger = lldb::SBDebugger::Create(false);
        g_backendState.debugger.SetAsync(true);

        g_backendState.listener = g_backendState.debugger.GetListener();

        g_lldbInitialized = true;
    }

    void terminate_lldb()
    {
        if (!g_lldbInitialized)
        {
            return;
        }

        if (g_backendState.process.IsValid())
        {
            g_backendState.process.Detach();
        }

        if (g_backendState.target.IsValid())
        {
            g_backendState.debugger.DeleteTarget(g_backendState.target);
        }

        lldb::SBDebugger::Destroy(g_backendState.debugger);
        lldb::SBDebugger::Terminate();

        g_backendState.callbacks.reset();
        g_backendState.debugger = lldb::SBDebugger{};
        g_backendState.target = lldb::SBTarget{};
        g_backendState.process = lldb::SBProcess{};
        g_backendState.listener = lldb::SBListener{};
        g_lldbInitialized = false;
    }

    LldbBackendState& get_backend_state()
    {
        return g_backendState;
    }
}
