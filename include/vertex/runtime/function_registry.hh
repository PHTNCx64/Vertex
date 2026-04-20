//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <vertex/runtime/libraryloader.hh>
#include <functional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <expected>
#include <filesystem>
#include <memory>
#include <vector>
#include <fmt/format.h>

namespace Vertex::Runtime
{

    class LibraryError final : public std::runtime_error
    {
      public:
        explicit LibraryError(const std::string& message)
            : std::runtime_error(message)
        {
        }
    };

    class LibraryHandle final
    {
      public:
        explicit LibraryHandle(void* handle) noexcept
            : m_handle(handle)
        {
        }

        ~LibraryHandle()
        {
            if (m_handle != nullptr)
            {
                (void) LibraryLoader::unload_library(m_handle);
                m_handle = nullptr;
            }
        }

        LibraryHandle(const LibraryHandle&) = delete;
        LibraryHandle& operator=(const LibraryHandle&) = delete;
        LibraryHandle(LibraryHandle&&) = delete;
        LibraryHandle& operator=(LibraryHandle&&) = delete;

        [[nodiscard]] void* get() const noexcept
        {
            return m_handle;
        }

        [[nodiscard]] bool is_loaded() const noexcept
        {
            return m_handle != nullptr;
        }

      private:
        void* m_handle{};
    };

    class Library final
    {
      public:
        explicit Library(const std::filesystem::path& path)
        {
            void* handle = LibraryLoader::load_library(path.string());
            if (handle == nullptr)
            {
                const auto reason = LibraryLoader::last_error();
                throw LibraryError(reason.empty()
                    ? fmt::format("Failed to load library '{}'", path.string())
                    : fmt::format("Failed to load library '{}': {}", path.string(), reason));
            }
            m_handle = std::make_shared<LibraryHandle>(handle);
        }

        ~Library() = default;

        template <class FunctionType> [[nodiscard]] std::expected<FunctionType, std::string> get_function(std::string_view name) const
        {
            static_assert(std::is_pointer_v<FunctionType> && std::is_function_v<std::remove_pointer_t<FunctionType>>, "FunctionType must be a function pointer");

            if (!m_handle || !m_handle->is_loaded())
            {
                return std::unexpected("Library handle is null");
            }

            void* proc = LibraryLoader::resolve_address(m_handle->get(), name);
            if (!proc)
            {
                return std::unexpected(fmt::format("Function '{}' not found", name));
            }

            return reinterpret_cast<FunctionType>(proc);
        }

        [[nodiscard]] void* handle() const noexcept { return m_handle ? m_handle->get() : nullptr; }

        [[nodiscard]] bool is_loaded() const noexcept { return m_handle && m_handle->is_loaded(); }

        [[nodiscard]] static Library create_stub() noexcept { return Library{}; }

        [[nodiscard]] std::shared_ptr<LibraryHandle> keepalive() const noexcept
        {
            return m_handle;
        }

      private:
        Library() noexcept = default;

        std::shared_ptr<LibraryHandle> m_handle{};
    };

    enum class FunctionRequirement
    {
        Required,
        Optional
    };

    template <class FunctionType> struct FunctionDescriptor
    {
        std::string name;
        FunctionRequirement requirement;
        FunctionType* target_ptr;

        FunctionDescriptor(std::string fn_name, const FunctionRequirement req, FunctionType* target)
            : name(std::move(fn_name)),
              requirement(req),
              target_ptr(target)
        {
        }
    };

    class FunctionRegistry final
    {
      public:
        template <class FunctionType> void register_function(const std::string_view name, const FunctionRequirement requirement, FunctionType* target)
        {
            const std::string nameStr{name};
            m_resolvers.emplace_back(
              [=](const Library& lib) -> std::expected<void, std::string>
              {
                  auto result = lib.get_function<FunctionType>(nameStr);
                  if (!result)
                  {
                      if (requirement == FunctionRequirement::Required)
                      {
                          return std::unexpected(fmt::format("Required function '{}' failed to resolve: {}", nameStr, result.error()));
                      }
                      *target = nullptr;
                      return {};
                  }
                  *target = result.value();
                  return {};
              });
        }

        [[nodiscard]] std::expected<std::vector<std::string>, std::string> resolve_all(const Library& library) const
        {
            std::vector<std::string> warnings;

            for (const auto& resolver : m_resolvers)
            {
                const auto result = resolver(library);
                if (!result)
                {
                    if (result.error().find("Required") == 0)
                    {
                        return std::unexpected(result.error());
                    }
                    warnings.push_back(result.error());
                }
            }

            return warnings;
        }

        void clear() { m_resolvers.clear(); }

        [[nodiscard]] std::size_t size() const noexcept { return m_resolvers.size(); }

      private:
        std::vector<std::function<std::expected<void, std::string>(const Library&)>> m_resolvers;
    };

} // namespace Vertex::Runtime
