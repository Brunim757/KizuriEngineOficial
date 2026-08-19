#pragma once
#include <spdlog/spdlog.h>
#include <memory>

namespace kizuri {

class Log {
public:
    static void Init();
    static std::shared_ptr<spdlog::logger>& Core()   { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger>& AppLog()  { return s_AppLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_AppLogger;
};

}

#define KZ_CORE_TRACE(...)    ::kizuri::Log::Core()->trace(__VA_ARGS__)
#define KZ_CORE_INFO(...)     ::kizuri::Log::Core()->info(__VA_ARGS__)
#define KZ_CORE_WARN(...)     ::kizuri::Log::Core()->warn(__VA_ARGS__)
#define KZ_CORE_ERROR(...)    ::kizuri::Log::Core()->error(__VA_ARGS__)
#define KZ_CORE_CRITICAL(...) ::kizuri::Log::Core()->critical(__VA_ARGS__)

#define KZ_TRACE(...)    ::kizuri::Log::AppLog()->trace(__VA_ARGS__)
#define KZ_INFO(...)     ::kizuri::Log::AppLog()->info(__VA_ARGS__)
#define KZ_WARN(...)     ::kizuri::Log::AppLog()->warn(__VA_ARGS__)
#define KZ_ERROR(...)    ::kizuri::Log::AppLog()->error(__VA_ARGS__)
#define KZ_CRITICAL(...) ::kizuri::Log::AppLog()->critical(__VA_ARGS__)

namespace kizuri {

class ScopeTracer {
public:
    explicit ScopeTracer(const char* name) : m_Name(name) {
        Log::Core()->trace("--> {0}", m_Name);
    }
    ~ScopeTracer() {
        Log::Core()->trace("<-- {0}", m_Name);
    }

private:
    const char* m_Name;
};

}

#define KZ_TRACE_SCOPE(name) ::kizuri::ScopeTracer kz_scope_tracer_##__LINE__(name)
