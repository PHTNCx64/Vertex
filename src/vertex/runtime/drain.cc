//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/drain.hh>

namespace Vertex::Runtime
{
    void BoundedDrain::acquire() noexcept
    {
        std::scoped_lock lock {m_mutex};
        ++m_inFlight;
    }

    void BoundedDrain::release() noexcept
    {
        {
            std::scoped_lock lock {m_mutex};
            if (m_inFlight > 0)
            {
                --m_inFlight;
            }
        }
        m_cv.notify_all();
    }

    bool BoundedDrain::wait(std::chrono::milliseconds timeout) noexcept
    {
        std::unique_lock lock {m_mutex};
        return m_cv.wait_for(lock, timeout, [this]
        {
            return m_inFlight == 0;
        });
    }

    std::uint32_t BoundedDrain::in_flight() const noexcept
    {
        std::scoped_lock lock {m_mutex};
        return m_inFlight;
    }

    BoundedDrainGuard::BoundedDrainGuard(BoundedDrain& drain) noexcept
        : m_drain {drain}
    {
        m_drain.acquire();
    }

    BoundedDrainGuard::~BoundedDrainGuard()
    {
        m_drain.release();
    }
}
