//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#include <vertex/debugger/debuggerworker.hh>

namespace Vertex::Debugger
{
    CallbackContextRegistry& CallbackContextRegistry::instance()
    {
        static CallbackContextRegistry registry{};
        return registry;
    }

    void CallbackContextRegistry::register_context(void* key, std::weak_ptr<CallbackContext> context)
    {
        std::scoped_lock lock{m_mutex};
        m_registry[key] = std::move(context);
    }

    void CallbackContextRegistry::unregister_context(void* key)
    {
        std::scoped_lock lock{m_mutex};
        m_registry.erase(key);
    }

    std::shared_ptr<CallbackContext> CallbackContextRegistry::lookup(void* key)
    {
        std::shared_lock lock{m_mutex};
        if (const auto it = m_registry.find(key); it != m_registry.end())
        {
            return it->second.lock();
        }
        return nullptr;
    }
}
