//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/io/virtualregion.hh>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <Windows.h>

#include <algorithm>

namespace Vertex::IO
{
    VirtualRegion::~VirtualRegion() noexcept
    {
        release();
    }

    VirtualRegion::VirtualRegion(VirtualRegion&& other) noexcept
        : m_baseAddr(other.m_baseAddr)
        , m_reservedBytes(other.m_reservedBytes)
        , m_committedBytes(other.m_committedBytes)
    {
        other.m_baseAddr = nullptr;
        other.m_reservedBytes = 0;
        other.m_committedBytes = 0;
    }

    VirtualRegion& VirtualRegion::operator=(VirtualRegion&& other) noexcept
    {
        if (this != &other)
        {
            release();

            m_baseAddr = other.m_baseAddr;
            m_reservedBytes = other.m_reservedBytes;
            m_committedBytes = other.m_committedBytes;

            other.m_baseAddr = nullptr;
            other.m_reservedBytes = 0;
            other.m_committedBytes = 0;
        }
        return *this;
    }

    StatusCode VirtualRegion::reserve(std::size_t reserveBytes)
    {
        if (m_baseAddr != nullptr)
        {
            release();
        }

        void* addr = VirtualAlloc(nullptr, reserveBytes, MEM_RESERVE, PAGE_NOACCESS);
        if (addr == nullptr)
        {
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_baseAddr = addr;
        m_reservedBytes = reserveBytes;
        m_committedBytes = 0;

        return StatusCode::STATUS_OK;
    }

    StatusCode VirtualRegion::ensure_committed(std::size_t neededBytes)
    {
        if (neededBytes <= m_committedBytes)
        {
            return StatusCode::STATUS_OK;
        }

        if (neededBytes > m_reservedBytes)
        {
            return StatusCode::STATUS_ERROR_MEMORY_OUT_OF_BOUNDS;
        }

        const std::size_t aligned = ((neededBytes + COMMIT_GRANULARITY - 1) / COMMIT_GRANULARITY) * COMMIT_GRANULARITY;
        const std::size_t commitTarget = std::min(aligned, m_reservedBytes);
        const std::size_t delta = commitTarget - m_committedBytes;

        void* commitAddr = static_cast<char*>(m_baseAddr) + m_committedBytes;
        void* result = VirtualAlloc(commitAddr, delta, MEM_COMMIT, PAGE_READWRITE);
        if (result == nullptr)
        {
            return StatusCode::STATUS_ERROR_MEMORY_ALLOCATION_FAILED;
        }

        m_committedBytes = commitTarget;
        return StatusCode::STATUS_OK;
    }

    void VirtualRegion::release() noexcept
    {
        if (m_baseAddr != nullptr)
        {
            VirtualFree(m_baseAddr, 0, MEM_RELEASE);
            m_baseAddr = nullptr;
            m_reservedBytes = 0;
            m_committedBytes = 0;
        }
    }

    void* VirtualRegion::base() const noexcept
    {
        return m_baseAddr;
    }

    std::size_t VirtualRegion::reserved_bytes() const noexcept
    {
        return m_reservedBytes;
    }

    std::size_t VirtualRegion::committed_bytes() const noexcept
    {
        return m_committedBytes;
    }

    bool VirtualRegion::is_reserved() const noexcept
    {
        return m_baseAddr != nullptr;
    }
} // namespace Vertex::IO
