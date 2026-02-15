//
// Copyright (C) 2026 PHTNC<>.
// Licensed under GPLv3.0 with Plugin Interface exceptions.
//
#pragma once

#include <utility>

namespace Vertex
{
    // not thread safe.
    // TODO: Evaluate whether this will be used in the future? This was used in past private implementations.
    class ResettableCallOnce
    {
    public:
        template<class Callable, class... Args>
        void call(Callable&& func, Args&&... args)
        {
            if (!m_resetFlag)
            {
                std::forward<Callable>(func)(std::forward<Args>(args)...);
                m_resetFlag = true;
            }
        }

        void reset()
        {
            m_resetFlag = false;
        }

    private:
        bool m_resetFlag{};
    };
}
