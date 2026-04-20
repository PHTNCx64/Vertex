//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <vertexusrrt/native_handle.hh>
#include <sdk/api.h>

#include <sys/ptrace.h>
#include <sys/user.h>
#include <dirent.h>

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <string>
#include <string_view>

extern native_handle& get_native_handle();
extern ProcessArchitecture get_process_architecture();

namespace ThreadInternal
{
    ThreadList* get_thread_list();
}

namespace
{
    ThreadState map_proc_state(const char stateChar)
    {
        switch (stateChar)
        {
        case 'R':
            return VERTEX_THREAD_RUNNING;
        case 'S':
        case 'D':
            return VERTEX_THREAD_WAITING;
        case 'T':
        case 't':
            return VERTEX_THREAD_SUSPENDED;
        case 'Z':
        case 'X':
        case 'x':
            return VERTEX_THREAD_TERMINATED;
        default:
            return VERTEX_THREAD_RUNNING;
        }
    }

    struct ProcStatInfo final
    {
        char state{'R'};
        std::int32_t nice{};
        bool valid{false};
    };

    [[nodiscard]] ProcStatInfo parse_proc_stat(const pid_t pid, const pid_t tid)
    {
        const auto path = std::string{"/proc/"} + std::to_string(pid) + "/task/" + std::to_string(tid) + "/stat";
        std::ifstream file{path};
        if (!file.is_open())
        {
            return {};
        }

        std::string line{};
        if (!std::getline(file, line))
        {
            return {};
        }

        const auto closeParen = line.rfind(')');
        if (closeParen == std::string::npos || closeParen + 2 >= line.size())
        {
            return {};
        }

        ProcStatInfo info{};
        info.state = line[closeParen + 2];

        std::string_view remainder{line.data() + closeParen + 2, line.size() - closeParen - 2};

        std::uint32_t fieldIndex{};
        std::size_t pos{};
        while (pos < remainder.size() && fieldIndex < 16)
        {
            while (pos < remainder.size() && remainder[pos] == ' ')
            {
                ++pos;
            }
            const auto end = remainder.find(' ', pos);
            const auto fieldEnd = (end == std::string_view::npos) ? remainder.size() : end;

            if (fieldIndex == 15)
            {
                const auto fieldStr = remainder.substr(pos, fieldEnd - pos);
                std::from_chars(fieldStr.data(), fieldStr.data() + fieldStr.size(), info.nice);
            }

            pos = fieldEnd;
            ++fieldIndex;
        }

        info.valid = true;
        return info;
    }

    void read_thread_name(const pid_t pid, const pid_t tid, char* name, const std::size_t nameSize)
    {
        const auto path = std::string{"/proc/"} + std::to_string(pid) + "/task/" + std::to_string(tid) + "/comm";
        std::ifstream file{path};
        if (!file.is_open())
        {
            name[0] = '\0';
            return;
        }

        std::string comm{};
        if (!std::getline(file, comm))
        {
            name[0] = '\0';
            return;
        }

        const auto copyLen = std::min(comm.size(), nameSize - 1);
        std::copy_n(comm.data(), copyLen, name);
        name[copyLen] = '\0';
    }
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
        if (processHandle <= 0)
        {
            return StatusCode::STATUS_ERROR_PROCESS_INVALID;
        }

        const pid_t pid = processHandle;

        ThreadList* internalList = ThreadInternal::get_thread_list();
        internalList->threadCount = 0;
        internalList->currentThreadId = 0;

        const auto taskPath = std::string{"/proc/"} + std::to_string(pid) + "/task";
        DIR* taskDir = opendir(taskPath.c_str());
        if (taskDir == nullptr)
        {
            return StatusCode::STATUS_ERROR_THREAD_INVALID_TASK;
        }

        const ProcessArchitecture arch = get_process_architecture();

        while (const dirent* entry = readdir(taskDir))
        {
            if (entry->d_name[0] == '.')
            {
                continue;
            }

            if (internalList->threadCount >= VERTEX_MAX_THREADS)
            {
                break;
            }

            pid_t tid{};
            const auto* nameEnd = entry->d_name + std::strlen(entry->d_name);
            const auto [ptr, ec] = std::from_chars(entry->d_name, nameEnd, tid);
            if (ec != std::errc{} || ptr != nameEnd)
            {
                continue;
            }

            auto& [id, name, state, instructionPointer, stackPointer, entryPoint, priority, isCurrent] =
                internalList->threads[internalList->threadCount];

            id = static_cast<std::uint32_t>(tid);
            isCurrent = 0;
            entryPoint = 0;
            instructionPointer = 0;
            stackPointer = 0;

            read_thread_name(pid, tid, name, sizeof(ThreadInfo::name));

            const auto statInfo = parse_proc_stat(pid, tid);
            if (statInfo.valid)
            {
                state = map_proc_state(statInfo.state);
                priority = statInfo.nice;
            }
            else
            {
                state = VERTEX_THREAD_RUNNING;
                priority = 0;
            }

            user_regs_struct regs{};
            if (ptrace(PTRACE_GETREGS, tid, nullptr, &regs) == 0)
            {
                if (arch == ProcessArchitecture::X86)
                {
                    instructionPointer = static_cast<std::uint32_t>(regs.rip);
                    stackPointer = static_cast<std::uint32_t>(regs.rsp);
                }
                else
                {
                    instructionPointer = regs.rip;
                    stackPointer = regs.rsp;
                }
            }

            internalList->threadCount++;
        }

        closedir(taskDir);

        std::memcpy(threadList, internalList, sizeof(ThreadList));

        return StatusCode::STATUS_OK;
    }
}
