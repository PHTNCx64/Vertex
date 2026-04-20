//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/runtime/session_epoch.hh>

namespace Vertex::Runtime
{
    std::uint64_t SessionEpoch::load() const noexcept
    {
        return m_value.load(std::memory_order_acquire);
    }

    std::uint64_t SessionEpoch::bump() noexcept
    {
        return m_value.fetch_add(1, std::memory_order_acq_rel) + 1;
    }
}
