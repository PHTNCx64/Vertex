//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <filesystem>
#include <shared_mutex>
#include <atomic>
#include <functional>
#include <vertex/safehandle.hh>

namespace Vertex::IO
{
    class File final
    {
      public:
        File() = default;

        File(const File&) = delete;
        File& operator=(const File&) = delete;

        File(File&& other) noexcept
            : m_sizeInBytes(other.m_sizeInBytes.exchange(0, std::memory_order_acq_rel)),
              m_mappedBaseAddr(other.m_mappedBaseAddr.exchange(0, std::memory_order_acq_rel)),
              m_usedBytes(other.m_usedBytes.exchange(0, std::memory_order_acq_rel)),
              m_path(std::move(other.m_path)),
              m_internalHandle(std::move(other.m_internalHandle)),
              m_mappedMemoryHandle(std::move(other.m_mappedMemoryHandle)),
              m_cleanUpFunc(std::move(other.m_cleanUpFunc))
        {
            other.m_cleanUpFunc = nullptr;
        }

        File& operator=(File&& other) noexcept
        {
            if (this != &other)
            {
                close();

                m_sizeInBytes.store(other.m_sizeInBytes.exchange(0, std::memory_order_acq_rel), std::memory_order_release);
                m_mappedBaseAddr.store(other.m_mappedBaseAddr.exchange(0, std::memory_order_acq_rel), std::memory_order_release);
                m_usedBytes.store(other.m_usedBytes.exchange(0, std::memory_order_acq_rel), std::memory_order_release);
                m_path = std::move(other.m_path);
                m_internalHandle = std::move(other.m_internalHandle);
                m_mappedMemoryHandle = std::move(other.m_mappedMemoryHandle);
                m_cleanUpFunc = std::move(other.m_cleanUpFunc);

                other.m_cleanUpFunc = nullptr;
            }
            return *this;
        }

        ~File() noexcept { close(); }

        [[nodiscard]] bool has_valid_handle() const noexcept { return m_internalHandle.is_valid(); }

        [[nodiscard]] bool is_mapped() const noexcept { return m_mappedBaseAddr.load(std::memory_order_acquire) != 0; }

        [[nodiscard]] bool is_valid() const noexcept { return has_valid_handle() && is_mapped(); }

        [[nodiscard]] explicit operator bool() const noexcept { return is_valid(); }

        [[nodiscard]] std::uintptr_t get_mapped_addr() const noexcept { return m_mappedBaseAddr.load(std::memory_order_acquire); }

        [[nodiscard]] std::size_t get_size() const noexcept { return m_sizeInBytes.load(std::memory_order_acquire); }

        [[nodiscard]] const std::filesystem::path& get_path() const noexcept { return m_path; }

        [[nodiscard]] const SafeHandle& get_file_handle() const noexcept { return m_internalHandle; }

        [[nodiscard]] const SafeHandle& get_mapping_handle() const noexcept { return m_mappedMemoryHandle; }

        [[nodiscard]] std::size_t get_used_bytes() const noexcept { return m_usedBytes.load(std::memory_order_acquire); }

        [[nodiscard]] double get_usage_ratio() const noexcept
        {
            const auto size = m_sizeInBytes.load(std::memory_order_acquire);
            if (size == 0)
            {
                return 0.0;
            }
            return static_cast<double>(m_usedBytes.load(std::memory_order_acquire)) / static_cast<double>(size);
        }

        void set_mapped_addr(const std::uintptr_t addr) noexcept { m_mappedBaseAddr.store(addr, std::memory_order_release); }

        void set_size(const std::size_t size) noexcept { m_sizeInBytes.store(size, std::memory_order_release); }

        void set_used_bytes(const std::size_t used) noexcept { m_usedBytes.store(used, std::memory_order_release); }

        [[nodiscard]] std::size_t add_used_bytes(const std::size_t delta) noexcept { return m_usedBytes.fetch_add(delta, std::memory_order_acq_rel) + delta; }

        void set_path(const std::filesystem::path& path) { m_path = path; }

        void set_file_handle(SafeHandle&& handle) noexcept { m_internalHandle = std::move(handle); }

        void set_mapping_handle(SafeHandle&& handle) noexcept { m_mappedMemoryHandle = std::move(handle); }

        void set_file_map(SafeHandle& handle) noexcept { m_internalHandle = std::move(handle); }

        void set_mapping_handle(SafeHandle& handle) noexcept { m_mappedMemoryHandle = std::move(handle); }

        void close() noexcept
        {
            if (m_cleanUpFunc)
            {
                m_cleanUpFunc();
            }

            m_mappedBaseAddr.store(0, std::memory_order_release);

            m_sizeInBytes.store(0, std::memory_order_release);
            m_usedBytes.store(0, std::memory_order_release);
        }

        void reset() noexcept
        {
            m_sizeInBytes.store(0, std::memory_order_release);
            m_mappedBaseAddr.store(0, std::memory_order_release);
            m_usedBytes.store(0, std::memory_order_release);
            m_path.clear();
            m_internalHandle = SafeHandle{};
            m_mappedMemoryHandle = SafeHandle{};
        }

        [[nodiscard]] std::shared_mutex& get_shared_mutex() const noexcept { return m_sharedMutex; }

      private:
        std::atomic<std::size_t> m_sizeInBytes{};
        std::atomic<std::uintptr_t> m_mappedBaseAddr{};
        std::atomic<std::size_t> m_usedBytes{};
        std::filesystem::path m_path{};
        SafeHandle m_internalHandle{};
        SafeHandle m_mappedMemoryHandle{};

      public:
        std::function<void()> m_cleanUpFunc{};

      private:
        mutable std::shared_mutex m_sharedMutex{};
    };
} // namespace Vertex::IO
