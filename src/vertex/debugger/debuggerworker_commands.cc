//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerworker.hh>
#include <vertex/runtime/plugin.hh>
#include <vertex/runtime/caller.hh>

namespace Vertex::Debugger
{
    void DebuggerWorker::send_command(DebuggerCommand cmd)
    {
        if (!m_isRunning.load(std::memory_order_acquire))
        {
            post_error(StatusCode::STATUS_ERROR_THREAD_IS_NOT_RUNNING, "Debugger worker not running");
            return;
        }

        if (m_stopping.load(std::memory_order_acquire))
        {
            return;
        }

        if (!is_valid_command_for_state(cmd))
        {
            post_error(StatusCode::STATUS_ERROR_DEBUGGER_INVALID_STATE, "Command not valid for current state");
            return;
        }

        Runtime::Plugin* plugin = get_plugin();
        if (plugin == nullptr)
        {
            post_error(StatusCode::STATUS_ERROR_PLUGIN_NOT_LOADED, "No plugin loaded");
            return;
        }

        std::visit(
          [this, plugin]<class T0>(T0&& arg)
          {
              using T = std::decay_t<T0>;

              if constexpr (std::is_same_v<T, CmdAttach>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_attach);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Attach failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdDetach>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_detach);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Detach failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdContinue>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_continue, arg.passException);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Continue failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdPause>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_pause);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Pause failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdStepInto>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_step, VERTEX_STEP_INTO);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Step into failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdStepOver>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_step, VERTEX_STEP_OVER);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Step over failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdStepOut>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_step, VERTEX_STEP_OUT);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Step out failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdRunToAddress>)
              {
                  const auto result = Runtime::safe_call(plugin->internal_vertex_debugger_run_to_address, arg.address);
                  if (!Runtime::status_ok(result))
                  {
                      post_error(Runtime::get_status(result), "Run to address failed");
                  }
              }
              else if constexpr (std::is_same_v<T, CmdShutdown>)
              {
                  std::ignore = stop();
              }
          },
          cmd);
    }

    bool DebuggerWorker::is_valid_command_for_state(const DebuggerCommand& cmd) const
    {
        const auto state = m_state.load(std::memory_order_acquire);
        const bool isAttached = m_attached.load(std::memory_order_acquire);

        return std::visit(
          [state, isAttached]<class T0>([[maybe_unused]] T0&& arg) -> bool
          {
              using T = std::decay_t<T0>;

              if constexpr (std::is_same_v<T, CmdAttach>)
              {
                  return state == DebuggerState::Detached && !isAttached;
              }
              else if constexpr (std::is_same_v<T, CmdDetach>)
              {
                  return state != DebuggerState::Detached && isAttached;
              }
              else if constexpr (std::is_same_v<T, CmdContinue>)
              {
                  return isAttached && (state == DebuggerState::Paused || state == DebuggerState::BreakpointHit || state == DebuggerState::Exception || state == DebuggerState::Stepping);
              }
              else if constexpr (std::is_same_v<T, CmdPause>)
              {
                  return isAttached && state == DebuggerState::Running;
              }
              else if constexpr (std::is_same_v<T, CmdStepInto> ||
                                 std::is_same_v<T, CmdStepOver> ||
                                 std::is_same_v<T, CmdStepOut> ||
                                 std::is_same_v<T, CmdRunToAddress>)
              {
                  return isAttached && (state == DebuggerState::Paused || state == DebuggerState::BreakpointHit || state == DebuggerState::Exception || state == DebuggerState::Stepping);
              }
              else if constexpr (std::is_same_v<T, CmdShutdown>)
              {
                  return true;
              }

              return false;
          },
          cmd);
    }
}
