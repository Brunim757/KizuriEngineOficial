#pragma once




#include <memory>
#include <string>
#include <cstdint>

#if defined(_MSC_VER)
    #define KZ_DEBUGBREAK() __debugbreak()
#else
    #include <csignal>
    #define KZ_DEBUGBREAK() raise(SIGTRAP)
#endif

#define KZ_EXPAND_MACRO(x) x
#define KZ_STRINGIFY(x) #x

namespace kizuri {

template<typename T>
using Scope = std::unique_ptr<T>;
template<typename T, typename... Args>
constexpr Scope<T> CreateScope(Args&&... args) { return std::make_unique<T>(std::forward<Args>(args)...); }

template<typename T>
using Ref = std::shared_ptr<T>;
template<typename T, typename... Args>
constexpr Ref<T> CreateRef(Args&&... args) { return std::make_shared<T>(std::forward<Args>(args)...); }

using EntityHandle = std::uint32_t;
constexpr EntityHandle kInvalidEntity = 0;

} 

#include "kizuri/core/Log.hpp"

#ifdef KZ_DEBUG
    #define KZ_ASSERT(cond, ...) { if (!(cond)) { kizuri::Log::Core().error("Assertion falhou: {0}", __VA_ARGS__); KZ_DEBUGBREAK(); } }
#else
    #define KZ_ASSERT(cond, ...)
#endif
