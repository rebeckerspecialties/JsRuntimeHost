#pragma once

#include "JsRuntime.h"

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
            JsRuntime::Dispatch(m_runtimeState, [callable{std::forward<CallableT>(callable)}](Napi::Env) {
                callable();
            });
        }

    private:
        std::shared_ptr<JsRuntime::InternalState> m_runtimeState;
    };
}
