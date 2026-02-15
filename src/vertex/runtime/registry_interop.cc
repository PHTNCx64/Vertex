//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <sdk/registry.h>
#include <sdk/macro.h>
#include <vertex/runtime/iregistry.hh>

#include <atomic>

namespace
{
    std::atomic<Vertex::Runtime::IRegistry*> g_registryInstance{};

    [[nodiscard]] inline Vertex::Runtime::IRegistry* get_registry_instance()
    {
        return g_registryInstance.load(std::memory_order_acquire);
    }
}

extern "C"
{
    StatusCode VERTEX_API vertex_registry_set_instance(void* handle)
    {
        auto* registry = static_cast<Vertex::Runtime::IRegistry*>(handle);
        g_registryInstance.store(registry, std::memory_order_release);
        return StatusCode::STATUS_OK;
    }

    void* VERTEX_API vertex_registry_get_instance()
    {
        return static_cast<void*>(get_registry_instance());
    }

    StatusCode VERTEX_API vertex_register_architecture(const ArchitectureInfo* archInfo)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!archInfo)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_architecture(*archInfo);
    }

    StatusCode VERTEX_API vertex_register_category(const RegisterCategoryDef* category)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!category)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_category(*category);
    }

    StatusCode VERTEX_API vertex_unregister_category(const char* categoryId)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!categoryId)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->unregister_category(categoryId);
    }

    StatusCode VERTEX_API vertex_register_register(const RegisterDef* reg)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!reg)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_register(*reg);
    }

    StatusCode VERTEX_API vertex_unregister_register(const char* registerName)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!registerName)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->unregister_register(registerName);
    }

    StatusCode VERTEX_API vertex_register_flag_bit(const FlagBitDef* flagBit)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!flagBit)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_flag_bit(*flagBit);
    }

    StatusCode VERTEX_API vertex_register_exception_type(const ExceptionTypeDef* exceptionType)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!exceptionType)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_exception_type(*exceptionType);
    }

    StatusCode VERTEX_API vertex_register_calling_convention(const CallingConventionDef* callingConv)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!callingConv)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_calling_convention(*callingConv);
    }

    StatusCode VERTEX_API vertex_register_snapshot(const RegistrySnapshot* snapshot)
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!snapshot)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_snapshot(*snapshot);
    }

    StatusCode VERTEX_API vertex_clear_registry()
    {
        auto* registry = get_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        registry->clear();
        return StatusCode::STATUS_OK;
    }
}
