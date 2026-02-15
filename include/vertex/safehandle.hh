//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__APPLE__) || defined(__MACH__)
#include <unistd.h>
#elif defined(_WIN32) || defined(_WIN64)
#include <Windows.h>
#endif

#include <utility>

namespace Vertex
{
#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__APPLE__) || defined(__MACH__)
    using NativeHandle = int;
    constexpr int INVALID_NATIVE_HANDLE = -1;
#elif defined(_WIN32) || defined(_WIN64)
    using NativeHandle = HANDLE;
    inline const HANDLE INVALID_NATIVE_HANDLE = INVALID_HANDLE_VALUE; // NOLINT
#endif

    class SafeHandle final
    {
      public:
        SafeHandle() noexcept = default;

        SafeHandle(const NativeHandle Handle) noexcept
            : m_internalHandle(Handle)
        {
        }

        ~SafeHandle() noexcept
        {
            close();
        }

        SafeHandle(const SafeHandle&) = delete;
        SafeHandle& operator=(const SafeHandle&) = delete;

        SafeHandle(SafeHandle&& other) noexcept
            : m_internalHandle(std::exchange(other.m_internalHandle, INVALID_NATIVE_HANDLE))
        {
        }

        SafeHandle& operator=(SafeHandle&& other) noexcept
        {
            if (this != &other)
            {
                close();
                m_internalHandle = std::exchange(other.m_internalHandle, INVALID_NATIVE_HANDLE);
            }
            return *this;
        }

        SafeHandle& operator=(const NativeHandle otherHandle) noexcept
        {
            if (m_internalHandle != otherHandle)
            {
                close();
                m_internalHandle = otherHandle;
            }
            return *this;
        }

        // IDE suggests explicit conversions, but implicit is fine in this context.
        [[nodiscard]] operator NativeHandle() const noexcept // NOLINT
        {
            return m_internalHandle;
        }

        [[nodiscard]] explicit operator bool() const noexcept
        {
            return is_valid();
        }

        [[nodiscard]] bool is_valid() const noexcept
        {
            return m_internalHandle != INVALID_NATIVE_HANDLE;
        }

        [[nodiscard]] NativeHandle get() const noexcept
        {
            return m_internalHandle;
        }

        [[nodiscard]] NativeHandle* get_address_of() noexcept
        {
            return &m_internalHandle;
        }

        [[nodiscard]] NativeHandle release() noexcept
        {
            return std::exchange(m_internalHandle, INVALID_NATIVE_HANDLE);
        }

        void reset(NativeHandle handle = INVALID_NATIVE_HANDLE) noexcept
        {
            if (m_internalHandle != handle)
            {
                close();
                m_internalHandle = handle;
            }
        }

        void close() noexcept
        {
            if (m_internalHandle != INVALID_NATIVE_HANDLE)
            {
#if defined(__linux) || defined(__linux__) || defined(linux) || defined(__APPLE__) || defined(__MACH__)
                const int result = ::close(m_internalHandle);
                (void)result;
#elif defined(_WIN32) || defined(_WIN64)
                const BOOL result = CloseHandle(m_internalHandle);
                (void)result;
#endif
                m_internalHandle = INVALID_NATIVE_HANDLE;
            }
        }

        void swap(SafeHandle& other) noexcept
        {
            std::swap(m_internalHandle, other.m_internalHandle);
        }

        [[nodiscard]] bool operator==(const SafeHandle& other) const noexcept
        {
            return m_internalHandle == other.m_internalHandle;
        }

        [[nodiscard]] bool operator!=(const SafeHandle& other) const noexcept
        {
            return m_internalHandle != other.m_internalHandle;
        }

        [[nodiscard]] bool operator==(NativeHandle handle) const noexcept
        {
            return m_internalHandle == handle;
        }

        [[nodiscard]] bool operator!=(NativeHandle handle) const noexcept
        {
            return m_internalHandle != handle;
        }

      private:
        NativeHandle m_internalHandle{INVALID_NATIVE_HANDLE};
    };

    inline void swap(SafeHandle& lhs, SafeHandle& rhs) noexcept
    {
        lhs.swap(rhs);
    }

}
