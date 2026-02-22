//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <sdk/ui.h>
#include <sdk/macro.h>
#include <vertex/runtime/iuiregistry.hh>

#include <atomic>

namespace
{
    std::atomic<Vertex::Runtime::IUIRegistry*> g_uiRegistryInstance{};

    [[nodiscard]] inline Vertex::Runtime::IUIRegistry* get_ui_registry_instance()
    {
        return g_uiRegistryInstance.load(std::memory_order_acquire);
    }
}

extern "C"
{
    StatusCode VERTEX_API vertex_ui_registry_set_instance(void* handle)
    {
        auto* registry = static_cast<Vertex::Runtime::IUIRegistry*>(handle);
        g_uiRegistryInstance.store(registry, std::memory_order_release);
        return StatusCode::STATUS_OK;
    }

    void* VERTEX_API vertex_ui_registry_get_instance()
    {
        return static_cast<void*>(get_ui_registry_instance());
    }

    StatusCode VERTEX_API vertex_register_ui_panel(const UIPanel* panel)
    {
        auto* registry = get_ui_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!panel)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        return registry->register_panel(*panel);
    }

    StatusCode VERTEX_API vertex_get_ui_value(const char* panelId, const char* fieldId, UIValue* outValue)
    {
        auto* registry = get_ui_registry_instance();
        if (!registry)
        {
            return StatusCode::STATUS_ERROR_INVALID_HANDLE;
        }

        if (!panelId || !fieldId || !outValue)
        {
            return StatusCode::STATUS_ERROR_INVALID_PARAMETER;
        }

        auto value = registry->get_value(panelId, fieldId);
        if (!value.has_value())
        {
            return StatusCode::STATUS_ERROR_GENERAL_NOT_FOUND;
        }

        *outValue = *value;
        return StatusCode::STATUS_OK;
    }
}
