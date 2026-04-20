//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <chrono>
#include <condition_variable>
#include <functional>
#include <mutex>
#include <optional>
#include <utility>

namespace Vertex::Runtime
{
    template<class T>
    class ResultChannel final
    {
    public:
        using Callback = std::move_only_function<void(const T&) const>;

        ResultChannel() = default;
        ~ResultChannel() = default;

        ResultChannel(const ResultChannel&) = delete;
        ResultChannel& operator=(const ResultChannel&) = delete;
        ResultChannel(ResultChannel&&) = delete;
        ResultChannel& operator=(ResultChannel&&) = delete;

        [[nodiscard]] bool post(T result)
        {
            Callback callback {};
            std::optional<T> dispatched {};
            {
                std::scoped_lock lock {m_mutex};
                if (m_result.has_value())
                {
                    return false;
                }
                m_result = std::move(result);
                dispatched = m_result;
                callback = std::move(m_callback);
                m_callback = {};
            }
            m_cv.notify_all();
            if (callback && dispatched)
            {
                callback(*dispatched);
            }
            return true;
        }

        void on_result(Callback callback)
        {
            std::optional<T> fireNow {};
            {
                std::scoped_lock lock {m_mutex};
                if (m_result.has_value())
                {
                    fireNow = m_result;
                }
                else
                {
                    m_callback = std::move(callback);
                    return;
                }
            }
            if (callback && fireNow)
            {
                callback(*fireNow);
            }
        }

        [[nodiscard]] bool wait_for(std::chrono::milliseconds timeout)
        {
            std::unique_lock lock {m_mutex};
            return m_cv.wait_for(lock, timeout, [this]
            {
                return m_result.has_value();
            });
        }

        [[nodiscard]] bool has_result() const noexcept
        {
            std::scoped_lock lock {m_mutex};
            return m_result.has_value();
        }

        [[nodiscard]] std::optional<T> copy_result() const
        {
            std::scoped_lock lock {m_mutex};
            return m_result;
        }

    private:
        mutable std::mutex m_mutex {};
        std::condition_variable m_cv {};
        std::optional<T> m_result {};
        Callback m_callback {};
    };
}
