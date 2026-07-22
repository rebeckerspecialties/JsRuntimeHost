#include "AppRuntime.h"
#include <napi/env.h>

#if __APPLE__
#include <CoreFoundation/CoreFoundation.h>
#endif

namespace Babylon
{
    void AppRuntime::RunEnvironmentTier(const char*)
    {
        auto globalContext = JSGlobalContextCreateInGroup(nullptr, nullptr);

#if __APPLE__
        if (__builtin_available(iOS 16.4, macOS 13.3, *))
        {
            JSGlobalContextSetInspectable(globalContext, m_options.EnableDebugger);
        }
#endif

        Napi::Env env = Napi::Attach(globalContext);

        Run(env);

        JSGlobalContextRelease(globalContext);

        // Detach must come after JSGlobalContextRelease since it triggers finalizers which require env.
        Napi::Detach(env);
    }

    void AppRuntime::DrainMicrotasks(Napi::Env)
    {
        // JavaScriptCore drains Promise jobs at script boundaries, but Apple
        // platform work such as asynchronous WebAssembly compilation completes
        // through the current thread's run loop.
#if __APPLE__
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0.0, true);
#endif
    }
}
