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

    class Library final
    {
      public:
        explicit Library(const std::filesystem::path& path)
        {
            m_handle = LibraryLoader::load_library(path.string());
            if (!m_handle)
            {
                const auto reason = LibraryLoader::last_error();
                throw LibraryError(reason.empty()
                    ? fmt::format("Failed to load library '{}'", path.string())
                    : fmt::format("Failed to load library '{}': {}", path.string(), reason));
            }
        }

        ~Library()
        {
            unload();
        }

        Library(const Library&) = delete;
        Library& operator=(const Library&) = delete;

        Library(Library&& other) noexcept
            : m_handle(other.m_handle)
        {
            other.m_handle = nullptr;
        }

        Library& operator=(Library&& other) noexcept
        {
            if (this != &other)
            {
                unload();
                m_handle = other.m_handle;
                other.m_handle = nullptr;
            }
            return *this;
        }

        bool unload() noexcept
        {
            if (m_handle)
            {
                const bool result = LibraryLoader::unload_library(m_handle);
                m_handle = nullptr;
                return result;
            }
            return true;
        }

        template <class FunctionType> [[nodiscard]] std::expected<FunctionType, std::string> get_function(std::string_view name) const
        {
            static_assert(std::is_pointer_v<FunctionType> && std::is_function_v<std::remove_pointer_t<FunctionType>>, "FunctionType must be a function pointer");

            if (!m_handle)
            {
                return std::unexpected("Library handle is null");
            }

            void* proc = LibraryLoader::resolve_address(m_handle, name);
            if (!proc)
            {
                return std::unexpected(fmt::format("Function '{}' not found", name));
            }

            return reinterpret_cast<FunctionType>(proc);
        }

        [[nodiscard]] void* handle() const noexcept { return m_handle; }

        [[nodiscard]] bool is_loaded() const noexcept { return m_handle != nullptr; }

        [[nodiscard]] static Library create_stub() noexcept { return Library{}; }

      private:
        Library() noexcept = default;

        void* m_handle{};
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
