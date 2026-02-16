//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <vertexusrrt/debugger_internal.hh>

#include <sdk/api.h>

#include <Windows.h>
#include <tlhelp32.h>

extern native_handle& get_native_handle();
extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    ThreadList* get_thread_list();
}

namespace debugger
{
    DWORD suspend_thread(HANDLE hThread);
    DWORD resume_thread(HANDLE hThread);
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_debugger_get_threads(ThreadList* threadList)
    {
        if (!threadList)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const native_handle& processHandle = get_native_handle();
        if (!processHandle)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        const DWORD processId = GetProcessId(processHandle);
        if (processId == 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_OPEN_INVALID;
        }

        ThreadList* internalList = ThreadInternal::get_thread_list();
        internalList->threadCount = 0;
        internalList->currentThreadId = 0;

        const HANDLE threadSnapshot = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
        if (threadSnapshot == INVALID_HANDLE_VALUE)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        THREADENTRY32 threadEntry{};
        threadEntry.dwSize = sizeof(THREADENTRY32);

        if (!Thread32First(threadSnapshot, &threadEntry))
        {
            CloseHandle(threadSnapshot);
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        do
        {
            if (threadEntry.th32OwnerProcessID != processId)
            {
                continue;
            }

            if (internalList->threadCount >= VERTEX_MAX_THREADS)
            {
                break;
            }
            const HANDLE hThread = OpenThread(
              THREAD_QUERY_INFORMATION | THREAD_GET_CONTEXT | THREAD_SUSPEND_RESUME, FALSE, threadEntry.th32ThreadID);

            if (!hThread)
            {
                continue;
            }

            auto& [id, name, state, instructionPointer, stackPointer, entryPoint, priority, isCurrent] = internalList->threads[internalList->threadCount];
            id = threadEntry.th32ThreadID;
            name[0] = '\0';
            priority = GetThreadPriority(hThread);
            isCurrent = 0;
            entryPoint = 0;
            instructionPointer = 0;
            stackPointer = 0;
            state = VERTEX_THREAD_RUNNING;

            const DWORD suspendCount = debugger::suspend_thread(hThread);
            if (suspendCount != static_cast<DWORD>(-1))
            {
                if (suspendCount > 0)
                {
                    state = VERTEX_THREAD_SUSPENDED;
                }

                const ProcessArchitecture arch = get_process_architecture();
                if (arch == ProcessArchitecture::X86)
                {
                    WOW64_CONTEXT ctx{};
                    ctx.ContextFlags = WOW64_CONTEXT_CONTROL;

                    if (Wow64GetThreadContext(hThread, &ctx))
                    {
                        instructionPointer = ctx.Eip;
                        stackPointer = ctx.Esp;
                    }
                }
                else if (arch == ProcessArchitecture::X86_64)
                {
                    alignas(16) CONTEXT ctx{};
                    ctx.ContextFlags = CONTEXT_CONTROL;

                    if (GetThreadContext(hThread, &ctx))
                    {
                        instructionPointer = ctx.Rip;
                        stackPointer = ctx.Rsp;
                    }
                }

                debugger::resume_thread(hThread);
            }

            CloseHandle(hThread);
            internalList->threadCount++;

        } while (Thread32Next(threadSnapshot, &threadEntry));

        CloseHandle(threadSnapshot);

        std::memcpy(threadList, internalList, sizeof(ThreadList));

        return StatusCode::STATUS_OK;
    }
}
