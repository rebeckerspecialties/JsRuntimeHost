#pragma once

#include "JsRuntime.h"

#include <type_traits>
#include <utility>

namespace Babylon
{
    /**
     * Scheduler that invokes continuations via JsRuntime::Dispatch.
     * Intended to be consumed by arcana.cpp tasks.
     */
    class JsRuntimeScheduler
    {
    public:
        explicit JsRuntimeScheduler(JsRuntime& runtime)
            : m_runtimeState{runtime.m_state}
        {
        }

        template<typename CallableT>
        void operator()(CallableT&& callable) const
        {
            JsRuntime::Dispatch(m_runtimeState, [callable{std::forward<CallableT>(callable)}](Napi::Env env) mutable {
                if constexpr (std::is_invocable_v<decltype(callable)&, Napi::Env>)
                {
                    callable(env);
                }
                else
                {
                    callable();
                }
            });
        }

    private:
        std::shared_ptr<JsRuntime::InternalState> m_runtimeState;
    };
}
