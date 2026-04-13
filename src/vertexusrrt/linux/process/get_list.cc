//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//
#include <sdk/api.h>
#include <sdk/process.h>

#include <cerrno>
#include <pwd.h>
#include <sys/inotify.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <charconv>
#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace
{
    [[nodiscard]] bool is_pid_dir(std::string_view name) noexcept
    {
        return !name.empty() && std::ranges::all_of(name, [](const unsigned char c) { return std::isdigit(c) != 0; });
    }

    void safe_cpy(char* dst, const std::string_view src, const std::size_t max_len) noexcept
    {
        const std::size_t cpy_len = std::min(src.length(), max_len - 1);
        std::copy_n(src.data(), cpy_len, dst);
        dst[cpy_len] = '\0';
    }

    [[nodiscard]] std::optional<uid_t> uid_from_proc_dir(const uint32_t pid) noexcept
    {
        const auto path = std::format("/proc/{}", pid);
        struct stat st{};
        if (stat(path.c_str(), &st) == 0)
        {
            return st.st_uid;
        }
        return std::nullopt;
    }

    struct ProcStatus final
    {
        std::string name{};
        uint32_t ppid{};
        uid_t uid{};
        bool gotName{};
        bool gotPpid{};
        bool gotUid{};
    };

    [[nodiscard]] ProcStatus read_proc_status(const uint32_t pid) noexcept
    {
        const auto path = std::format("/proc/{}/status", pid);
        std::ifstream file{path};
        if (!file)
        {
            return {};
        }

        ProcStatus result{};
        std::string line{};

        while (std::getline(file, line))
        {
            std::string_view sv{line};

            if (sv.starts_with("Name:"))
            {
                sv.remove_prefix(5);
                while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
                {
                    sv.remove_prefix(1);
                }
                result.name = std::string{sv};
                result.gotName = true;
            }
            else if (sv.starts_with("PPid:"))
            {
                sv.remove_prefix(5);
                while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
                {
                    sv.remove_prefix(1);
                }
                if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result.ppid); ec == std::errc{})
                {
                    result.gotPpid = true;
                }
            }
            else if (sv.starts_with("Uid:"))
            {
                sv.remove_prefix(4);
                while (!sv.empty() && (sv.front() == ' ' || sv.front() == '\t'))
                {
                    sv.remove_prefix(1);
                }
                unsigned int uidVal{};
                if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), uidVal); ec == std::errc{})
                {
                    result.uid = static_cast<uid_t>(uidVal);
                    result.gotUid = true;
                }
            }

            if (result.gotName && result.gotPpid && result.gotUid)
            {
                break;
            }
        }

        return result;
    }

    struct ProcStatInfo final
    {
        std::string comm{};
        uint32_t ppid{};
        bool valid{};
    };

    [[nodiscard]] ProcStatInfo read_proc_stat(const uint32_t pid) noexcept
    {
        const auto path = std::format("/proc/{}/stat", pid);
        std::ifstream file{path};
        if (!file)
        {
            return {};
        }

        std::string content{};
        if (!std::getline(file, content))
        {
            return {};
        }

        const auto openParen = content.find('(');
        const auto closeParen = content.rfind(')');
        if (openParen == std::string::npos || closeParen == std::string::npos || closeParen <= openParen)
        {
            return {};
        }

        ProcStatInfo result{};
        result.comm = content.substr(openParen + 1, closeParen - openParen - 1);

        if (closeParen + 2 >= content.size())
        {
            return result;
        }

        std::string_view sv{content};
        sv.remove_prefix(closeParen + 2);

        if (const auto spacePos = sv.find(' '); spacePos != std::string_view::npos && spacePos + 1 < sv.size())
        {
            sv.remove_prefix(spacePos + 1);
            if (auto [ptr, ec] = std::from_chars(sv.data(), sv.data() + sv.size(), result.ppid); ec == std::errc{})
            {
                result.valid = true;
            }
        }

        return result;
    }

    [[nodiscard]] std::string read_proc_cmdline(const uint32_t pid) noexcept
    {
        const auto path = std::format("/proc/{}/cmdline", pid);
        std::ifstream file{path, std::ios::binary};
        if (!file)
        {
            return {};
        }

        std::string segment{};
        std::getline(file, segment, '\0');

        if (segment.empty())
        {
            return {};
        }

        if (const auto pos = segment.rfind('/'); pos != std::string::npos)
        {
            return segment.substr(pos + 1);
        }
        return segment;
    }

    [[nodiscard]] std::string read_exe_name(const uint32_t pid) noexcept
    {
        const auto linkPath = std::format("/proc/{}/exe", pid);

        std::array<char, VERTEX_MAX_NAME_LENGTH + 1> buf{};
        const ssize_t len = readlink(linkPath.c_str(), buf.data(), VERTEX_MAX_NAME_LENGTH);
        if (len <= 0)
        {
            return {};
        }

        std::string_view sv{buf.data(), static_cast<std::size_t>(len)};

        constexpr std::string_view DELETED_SUFFIX{" (deleted)"};
        if (sv.ends_with(DELETED_SUFFIX))
        {
            sv.remove_suffix(DELETED_SUFFIX.size());
        }

        if (const auto pos = sv.rfind('/'); pos != std::string_view::npos)
        {
            sv = sv.substr(pos + 1);
        }
        return std::string{sv};
    }

    [[nodiscard]] std::string resolve_process_name(const uint32_t pid, std::string_view statusName) noexcept
    {
        if (std::string exeName = read_exe_name(pid); !exeName.empty())
        {
            return exeName;
        }

        if (std::string cmdlineName = read_proc_cmdline(pid); !cmdlineName.empty())
        {
            return cmdlineName;
        }

        if (!statusName.empty())
        {
            return std::string{statusName};
        }

        return {};
    }

    [[nodiscard]] std::string uid_to_username(const uid_t uid) noexcept
    {
        std::array<char, 1024> buf{};
        passwd pw{};
        passwd* result{};

        if (getpwuid_r(uid, &pw, buf.data(), buf.size(), &result) == 0 && result)
        {
            return std::string{result->pw_name};
        }

        return std::to_string(uid);
    }

    [[nodiscard]] uint32_t sv_to_uint32(const std::string_view sv) noexcept
    {
        uint32_t val{};
        std::from_chars(sv.data(), sv.data() + sv.size(), val);
        return val;
    }

    constexpr uint32_t SYSTEM_INIT_PID{1};
    constexpr uint32_t SYSTEM_KTHREADD_PID{2};

    struct ProcessListCache final
    {
        std::mutex mutex{};
        std::vector<ProcessInformation> processes{};
        int inotifyFd{-1};
        int watchFd{-1};
        bool watchAttempted{};
        bool watchActive{};
        bool dirty{true};
        bool hasSnapshot{};
        std::chrono::steady_clock::time_point lastRefresh{};
    };

    [[nodiscard]] ProcessListCache& get_process_list_cache() noexcept
    {
        static ProcessListCache cache{};
        return cache;
    }

    void close_proc_watch(ProcessListCache& cache) noexcept
    {
        if (cache.inotifyFd >= 0 && cache.watchFd >= 0)
        {
            inotify_rm_watch(cache.inotifyFd, cache.watchFd);
            cache.watchFd = -1;
        }

        if (cache.inotifyFd >= 0)
        {
            close(cache.inotifyFd);
            cache.inotifyFd = -1;
        }

        cache.watchActive = false;
    }

    void initialize_proc_watch(ProcessListCache& cache) noexcept
    {
        if (cache.watchAttempted)
        {
            return;
        }
        cache.watchAttempted = true;

        cache.inotifyFd = inotify_init1(IN_NONBLOCK | IN_CLOEXEC);
        if (cache.inotifyFd < 0)
        {
            return;
        }

        constexpr uint32_t WATCH_MASK{
            IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO | IN_DELETE_SELF | IN_MOVE_SELF | IN_Q_OVERFLOW};
        cache.watchFd = inotify_add_watch(cache.inotifyFd, "/proc", WATCH_MASK);
        if (cache.watchFd < 0)
        {
            close_proc_watch(cache);
            return;
        }

        cache.watchActive = true;
        cache.dirty = true;
    }

    [[nodiscard]] bool is_process_list_event(const inotify_event& event) noexcept
    {
        constexpr uint32_t CHANGE_MASK{IN_CREATE | IN_DELETE | IN_MOVED_FROM | IN_MOVED_TO};
        if ((event.mask & CHANGE_MASK) == 0 || event.len == 0 || !event.name)
        {
            return false;
        }

        return is_pid_dir(event.name);
    }

    [[nodiscard]] bool consume_proc_events(ProcessListCache& cache) noexcept
    {
        if (!cache.watchActive || cache.inotifyFd < 0)
        {
            return false;
        }

        std::array<char, 16 * 1024> buffer{};
        bool processListChanged{};

        for (;;)
        {
            const ssize_t bytesRead = read(cache.inotifyFd, buffer.data(), buffer.size());
            if (bytesRead < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                if (errno == EAGAIN || errno == EWOULDBLOCK)
                {
                    break;
                }

                close_proc_watch(cache);
                cache.watchAttempted = false;
                return true;
            }
            if (bytesRead == 0)
            {
                break;
            }

            std::size_t offset{};
            while (offset < static_cast<std::size_t>(bytesRead))
            {
                const auto* event = reinterpret_cast<const inotify_event*>(buffer.data() + offset);

                if ((event->mask & (IN_Q_OVERFLOW | IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) != 0)
                {
                    processListChanged = true;

                    if ((event->mask & (IN_DELETE_SELF | IN_MOVE_SELF | IN_IGNORED)) != 0)
                    {
                        close_proc_watch(cache);
                        cache.watchAttempted = false;
                    }
                }
                else if (is_process_list_event(*event))
                {
                    processListChanged = true;
                }

                offset += sizeof(inotify_event) + event->len;
            }
        }

        return processListChanged;
    }

    [[nodiscard]] std::optional<std::vector<ProcessInformation>> enumerate_processes() noexcept
    {
        std::error_code ec{};
        std::filesystem::directory_iterator procIter{"/proc", ec};
        if (ec)
        {
            return std::nullopt;
        }

        std::vector<ProcessInformation> processes{};

        try
        {
            for (const auto& dirEntry : procIter)
            {
                try
                {
                    const auto filename = dirEntry.path().filename().string();
                    if (!is_pid_dir(filename))
                    {
                        continue;
                    }

                    const auto pid = sv_to_uint32(filename);

                    std::string baseName{};
                    uint32_t ppid{};
                    uid_t uid{};
                    bool gotPpid{};
                    bool gotUid{};

                    auto status = read_proc_status(pid);
                    baseName = std::move(status.name);
                    ppid = status.ppid;
                    uid = status.uid;
                    gotPpid = status.gotPpid;
                    gotUid = status.gotUid;

                    if (!gotPpid || baseName.empty())
                    {
                        if (const auto statInfo = read_proc_stat(pid); statInfo.valid)
                        {
                            if (!gotPpid)
                            {
                                ppid = statInfo.ppid;
                                gotPpid = true;
                            }
                            if (baseName.empty())
                            {
                                baseName = statInfo.comm;
                            }
                        }
                    }

                    if (!gotUid)
                    {
                        if (const auto uidOpt = uid_from_proc_dir(pid))
                        {
                            uid = *uidOpt;
                            gotUid = true;
                        }
                    }

                    const std::string processName = resolve_process_name(pid, baseName);
                    if (processName.empty())
                    {
                        continue;
                    }

                    ProcessInformation info{};
                    info.processId = pid;
                    info.parentProcessId = (ppid == SYSTEM_INIT_PID || ppid == SYSTEM_KTHREADD_PID) ? 0 : ppid;

                    safe_cpy(info.processName, processName, VERTEX_MAX_NAME_LENGTH);

                    if (gotUid)
                    {
                        safe_cpy(info.processOwner, uid_to_username(uid), VERTEX_MAX_OWNER_LENGTH);
                    }
                    else
                    {
                        safe_cpy(info.processOwner, "Unknown", VERTEX_MAX_OWNER_LENGTH);
                    }

                    processes.push_back(info);
                }
                catch (const std::filesystem::filesystem_error&)
                {
                    continue;
                }
            }
        }
        catch (const std::filesystem::filesystem_error&)
        {
        }

        return processes;
    }
}

extern "C"
{
    VERTEX_EXPORT StatusCode VERTEX_API vertex_process_get_list(ProcessInformation** list, uint32_t* count)
    {
        if (!count)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto& cache = get_process_list_cache();
        std::scoped_lock lock(cache.mutex);

        initialize_proc_watch(cache);
        if (!cache.watchActive)
        {
            constexpr auto FALLBACK_REFRESH_WINDOW = std::chrono::milliseconds{100};
            const auto now = std::chrono::steady_clock::now();
            if (!cache.hasSnapshot || now - cache.lastRefresh >= FALLBACK_REFRESH_WINDOW)
            {
                cache.dirty = true;
            }
        }
        if (consume_proc_events(cache))
        {
            cache.dirty = true;
        }

        if (cache.dirty)
        {
            if (auto refreshed = enumerate_processes())
            {
                cache.processes = std::move(*refreshed);
                cache.dirty = false;
                cache.hasSnapshot = true;
                cache.lastRefresh = std::chrono::steady_clock::now();
            }
            else if (cache.processes.empty())
            {
                return StatusCode::STATUS_ERROR_PROCESS_ACCESS_DENIED;
            }
        }

        const auto actualCount = static_cast<uint32_t>(cache.processes.size());

        if (!list)
        {
            *count = actualCount;
            return StatusCode::STATUS_OK;
        }

        if (!*list || *count == 0)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        const std::uint32_t bufferSize = *count;
        const uint32_t copyCount = std::min(bufferSize, actualCount);

        std::copy_n(cache.processes.begin(), copyCount, *list);

        if (actualCount > bufferSize)
        {
            *count = actualCount;
            return StatusCode::STATUS_ERROR_MEMORY_BUFFER_TOO_SMALL;
        }

        *count = copyCount;

        return StatusCode::STATUS_OK;
    }
}
