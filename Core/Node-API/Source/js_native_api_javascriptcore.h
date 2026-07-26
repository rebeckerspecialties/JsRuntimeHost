#pragma once

#include <napi/js_native_api.h>
#include <napi/js_native_api_types.h>
#include <JavaScriptCore/JavaScript.h>
#include <functional>
#include <unordered_map>
#include <unordered_set>
#include <list>
#include <mutex>
#include <thread>
#include <utility>
#include <vector>
#include <cassert>

struct napi_env__ {
  JSGlobalContextRef context{};
  JSValueRef last_exception{};
  napi_extended_error_info last_error{nullptr, nullptr, 0, napi_ok};
  std::unordered_map<napi_value, std::uintptr_t> active_ref_values{};
  std::list<napi_ref> strong_refs{};
  bool shutting_down{false};

  // napi_set_instance_data / napi_get_instance_data (N-API v6).
  void* instance_data{};
  napi_finalize instance_data_finalize_cb{};
  void* instance_data_finalize_hint{};

  JSValueRef constructor_info_symbol{};
  JSValueRef function_info_symbol{};
  JSValueRef reference_info_symbol{};
  JSValueRef wrapper_info_symbol{};
  JSValueRef function_prototype_call{};
  JSValueRef is_bigint_function{};
  bool bigint_supported{false};

  const std::thread::id thread_id{std::this_thread::get_id()};

  napi_env__(JSGlobalContextRef context) : context{context} {
    napi_envs[context] = this;
    JSGlobalContextRetain(context);
    init_symbol(constructor_info_symbol, "BabylonNative_ConstructorInfo");
    init_symbol(function_info_symbol, "BabylonNative_FunctionInfo");
    init_symbol(reference_info_symbol, "BabylonNative_ReferenceInfo");
    init_symbol(wrapper_info_symbol, "BabylonNative_WrapperInfo");
    init_function_prototype_call();
    init_is_bigint_function();
    init_bigint_supported();
  }

  ~napi_env__() {
    shutting_down = true;
    if (instance_data_finalize_cb != nullptr) {
      instance_data_finalize_cb(this, instance_data, instance_data_finalize_hint);
    }
    deinit_refs();
    deinit_symbol(is_bigint_function);
    deinit_symbol(function_prototype_call);
    deinit_symbol(wrapper_info_symbol);
    deinit_symbol(reference_info_symbol);
    deinit_symbol(function_info_symbol);
    deinit_symbol(constructor_info_symbol);
    JSGlobalContextRelease(context);
    delete_remaining_refs();
    napi_envs.erase(context);
  }

  static napi_env get(JSGlobalContextRef context) {
    auto it = napi_envs.find(context);
    if (it != napi_envs.end()) {
      return it->second;
    } else {
      return nullptr;
    }
  }

  void set_defer_finalizers(bool defer) {
    std::lock_guard<std::mutex> lock{deferred_finalizers_mutex};
    defer_finalizers = defer;
  }

  template<typename TCallback>
  bool defer_finalizer_if_requested(TCallback&& callback) {
    std::lock_guard<std::mutex> lock{deferred_finalizers_mutex};
    if (!defer_finalizers) {
      return false;
    }
    deferred_finalizers.emplace_back(std::forward<TCallback>(callback));
    return true;
  }

  void drain_deferred_finalizers() {
    for (;;) {
      std::vector<std::function<void()>> finalizers;
      {
        std::lock_guard<std::mutex> lock{deferred_finalizers_mutex};
        if (deferred_finalizers.empty()) {
          return;
        }
        finalizers.swap(deferred_finalizers);
      }
      for (auto& finalizer : finalizers) {
        finalizer();
      }
    }
  }

  void track_ref(napi_ref ref) {
    refs.insert(ref);
  }

  void untrack_ref(napi_ref ref) {
    refs.erase(ref);
  }

 private:
  static inline std::unordered_map<JSGlobalContextRef, napi_env> napi_envs{};
  std::unordered_set<napi_ref> refs{};
  std::mutex deferred_finalizers_mutex{};
  bool defer_finalizers{false};
  std::vector<std::function<void()>> deferred_finalizers{};

  void deinit_refs();
  void delete_remaining_refs();
  void init_symbol(JSValueRef& symbol, const char* description);
  void init_function_prototype_call();
  void init_is_bigint_function();
  void init_bigint_supported();
  void deinit_symbol(JSValueRef symbol);
};

#define RETURN_STATUS_IF_FALSE(env, condition, status) \
  do {                                                 \
    if (!(condition)) {                                \
      return napi_set_last_error((env), (status));     \
    }                                                  \
  } while (0)

#define CHECK_ENV(env)                                    \
  do {                                                    \
    if ((env) == nullptr) {                               \
      return napi_invalid_arg;                            \
    }                                                     \
    assert(env->thread_id == std::this_thread::get_id()); \
  } while (0)

#define CHECK_ARG(env, arg) \
  RETURN_STATUS_IF_FALSE((env), ((arg) != nullptr), napi_invalid_arg)

#define CHECK_JSC(env, exception)                \
  do {                                           \
    if ((exception) != nullptr) {                \
      return napi_set_exception(env, exception); \
    }                                            \
  } while (0)

// This does not call napi_set_last_error because the expression
// is assumed to be a NAPI function call that already did.
#define CHECK_NAPI(expr)                  \
  do {                                    \
    napi_status status = (expr);          \
    if (status != napi_ok) return status; \
  } while (0)
