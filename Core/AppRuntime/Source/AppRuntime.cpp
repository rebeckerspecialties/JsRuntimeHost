#include "AppRuntime.h"

#include "DelayedTaskScheduler.h"

#include <arcana/threading/cancellation.h>
#include <arcana/threading/dispatcher.h>

#include <cassert>
#include <cstring>
#include <cstdlib>
#include <atomic>
#include <optional>
#include <mutex>
#include <thread>
#if defined(__APPLE__)
#include <pthread/qos.h>
#endif
#include <type_traits>

namespace Babylon
{
    class AppRuntime::Impl
    {
    public:
        template<typename CallableT>
        void Append(CallableT callable)
        {
            if constexpr (std::is_copy_constructible<CallableT>::value)
            {
                m_dispatcher.queue([this, callable = std::move(callable)]() {
                    callable(m_env.value());
                });
            }
            else
            {
                m_dispatcher.queue([this, callablePtr = std::make_shared<CallableT>(std::move(callable))]() {
                    (*callablePtr)(m_env.value());
                });
            }
        }

        std::optional<Napi::Env> m_env{};
        std::optional<std::scoped_lock<std::mutex>> m_suspensionLock{};
        arcana::cancellation_source m_cancelSource{};
        arcana::manual_dispatcher<128> m_dispatcher{};
        std::unique_ptr<Internal::DelayedTaskScheduler> m_delayedTaskScheduler{std::make_unique<Internal::DelayedTaskScheduler>()};
        bool m_delayedTaskSchedulerRegistered{};
        std::thread m_thread;
        std::atomic_bool m_terminationRequested{false};
        std::atomic_bool m_executionTerminationRequested{false};
    };

    AppRuntime::AppRuntime() :
        AppRuntime{{}}
    {
    }

    AppRuntime::AppRuntime(Options options)
        : m_options{std::move(options)}
        , m_impl{std::make_unique<Impl>()}
    {
        m_impl->m_thread = std::thread{[this] {
#if defined(__APPLE__)
            // Diagnostic: JSRUNTIMEHOST_APPRUNTIME_QOS=background|utility runs the JavaScript thread
            // at that QoS class, which on Apple silicon schedules it on the efficiency cores while the
            // host's frame timer keeps its own QoS (a process-wide clamp would throttle that too).
            if (const char* qos = std::getenv("JSRUNTIMEHOST_APPRUNTIME_QOS"))
            {
                const qos_class_t qosClass = std::strcmp(qos, "background") == 0 ? QOS_CLASS_BACKGROUND
                    : std::strcmp(qos, "utility") == 0 ? QOS_CLASS_UTILITY
                    : QOS_CLASS_UNSPECIFIED;
                if (qosClass != QOS_CLASS_UNSPECIFIED)
                {
                    pthread_set_qos_class_self_np(qosClass, 0);
                }
            }
#endif
            RunPlatformTier();
            if (m_options.ThreadExitHandler)
            {
                m_options.ThreadExitHandler();
            }
        }};

        Dispatch([this](Napi::Env env) {
            JsRuntime::CreateForJavaScript(env, [this](auto func) { Dispatch(std::move(func)); });
            Internal::DelayedTaskScheduler::SetForJavaScript(env, GetDelayedTaskScheduler());
            m_impl->m_delayedTaskSchedulerRegistered = true;
        });
    }

    AppRuntime::~AppRuntime()
    {
        if (m_impl->m_suspensionLock.has_value())
        {
            m_impl->m_suspensionLock.reset();
        }

        Terminate();

        m_impl->m_thread.join();
    }

    void AppRuntime::Run(Napi::Env env)
    {
        m_impl->m_env = std::make_optional(env);

        m_impl->m_dispatcher.set_affinity(std::this_thread::get_id());

        while (!m_impl->m_cancelSource.cancelled())
        {
            if (m_impl->m_dispatcher.blocking_tick(m_impl->m_cancelSource))
            {
                DrainPostDispatchWork(env);
            }
        }

        Napi::HandleScope scope{env};
        ShutdownEnvironment(env);

        if (m_impl->m_delayedTaskSchedulerRegistered)
        {
            Internal::DelayedTaskScheduler::ClearFromJavaScript(env);
            m_impl->m_delayedTaskSchedulerRegistered = false;
        }
        GetDelayedTaskScheduler().Shutdown();

        // The dispatcher can be non-empty if something is dispatched after cancellation.
        m_impl->m_dispatcher.clear();
    }

    Internal::DelayedTaskScheduler& AppRuntime::GetDelayedTaskScheduler()
    {
        return *m_impl->m_delayedTaskScheduler;
    }

    void AppRuntime::Suspend()
    {
        auto suspensionMutex = std::make_shared<std::mutex>();
        m_impl->m_suspensionLock.emplace(*suspensionMutex);
        m_impl->Append([suspensionMutex{std::move(suspensionMutex)}](Napi::Env) {
            std::scoped_lock lock{*suspensionMutex};
        });
    }

    void AppRuntime::Resume()
    {
        m_impl->m_suspensionLock.reset();
    }

    void AppRuntime::Terminate()
    {
        m_impl->m_executionTerminationRequested.store(true);
        Close();
    }

    void AppRuntime::Close()
    {
        if (m_impl->m_terminationRequested.exchange(true))
        {
            return;
        }

        m_impl->m_cancelSource.cancel();

        // Queueing under the dispatcher's mutex makes the wake-up immune to
        // the missed-notification race covered by DestroyDoesNotDeadlock.
        // The cancelled run loop drops this no-op rather than executing it.
        m_impl->m_dispatcher.queue([]() {});
    }

    bool AppRuntime::IsTerminationRequested() const noexcept
    {
        return m_impl->m_terminationRequested.load();
    }

    bool AppRuntime::IsExecutionTerminationRequested() const noexcept
    {
        return m_impl->m_executionTerminationRequested.load();
    }

    void AppRuntime::Dispatch(Dispatchable<void(Napi::Env)> func)
    {
        if (IsTerminationRequested())
        {
            return;
        }

        m_impl->Append([this, func{std::move(func)}](Napi::Env env) mutable {
            Execute([this, env, func{std::move(func)}]() mutable {
                // Some engines (notably Hermes) require an open NAPI handle
                // scope before any napi_* call that materializes a value.
                // The other engines (V8/Chakra/JSC) already provide an outer
                // scope at the RunEnvironmentTier level, so this extra
                // scope is harmless there but mandatory for Hermes.
                Napi::HandleScope scope{env};

                try
                {
                    func(env);
                }
                catch (const Napi::Error& error)
                {
                    m_options.UnhandledExceptionHandler(error);
                }
                catch (...)
                {
                    assert(false);
                    std::abort();
                }

                // Drain engine-level microtasks/jobs queued during the
                // callback (Promise continuations, queueMicrotask, etc.) so
                // they run before the next top-level Dispatch.  No-op for
                // engines that drain automatically; Hermes needs an explicit
                // pump.
                DrainMicrotasks(env);
            });
        });
    }
}
