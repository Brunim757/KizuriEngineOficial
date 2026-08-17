#include "kizuri/core/Log.hpp"
#include "kizuri/core/LogHistory.hpp"
#include <string>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/fmt/fmt.h>
#include <vector>
#include <mutex>

#if defined(KZ_PLATFORM_ANDROID)
    #include <android/log.h>
#endif

namespace kizuri {

std::shared_ptr<spdlog::logger> Log::s_CoreLogger;
std::shared_ptr<spdlog::logger> Log::s_AppLogger;

static LogLevel ToLogLevel(spdlog::level::level_enum lvl) {
    switch (lvl) {
        case spdlog::level::trace:    return LogLevel::Trace;
        case spdlog::level::debug:    return LogLevel::Debug;
        case spdlog::level::info:     return LogLevel::Info;
        case spdlog::level::warn:     return LogLevel::Warn;
        case spdlog::level::err:      return LogLevel::Error;
        default:                      return LogLevel::Critical;
    }
}

// Sink que só existe pra alimentar o LogHistory (consumido pela aba
// Console do editor) — não escreve em disco nem em terminal, quem faz
// isso são os outros dois sinks já registrados abaixo.
#if defined(KZ_PLATFORM_ANDROID)
// Android: stdout não aparece em lugar nenhum — logcat é o console real.
class LogcatSink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        __android_log_print(ANDROID_LOG_INFO, "Kizuri",
                            "%.*s", (int)formatted.size(), formatted.data());
    }
    void flush_() override {}
};
#endif

class MemorySink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        // memory_buf_t é fmt::memory_buffer (build com fmt bundled) ou
        // std::string (SPDLOG_USE_STD_FORMAT) — nunca assumir fmt:: aqui:
        // a construção abaixo funciona com os dois (data/size).
        LogHistory::Push(ToLogLevel(msg.level),
                         std::string(formatted.data(), formatted.size()));
    }
    void flush_() override {}
};

void Log::Init() {
    std::vector<spdlog::sink_ptr> sinks;
#if defined(KZ_PLATFORM_ANDROID)
    // Em Android o stdout é perdido; logcat (adb logcat -s Kizuri) é o
    // console. O arquivo KizuriEngine.log também não faz sentido aqui (o
    // filesDir dura enquanto o app existir; logcat tem filtro por tag).
    sinks.push_back(std::make_shared<LogcatSink>());
    sinks.push_back(std::make_shared<MemorySink>());
#else
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    // v0.37.x: o arquivo KizuriEngine.log e o nível TRACE são só de
    // DESENVOLVIMENTO (testadores). Em Release (usuário final): nada de
    // arquivo no disco e nenhum TRACE — só logs úteis (info/warn/error).
#if defined(KZ_RELEASE)
    sinks.push_back(std::make_shared<MemorySink>());
#else
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("KizuriEngine.log", true));
    sinks.push_back(std::make_shared<MemorySink>());
#endif
#endif

    sinks[0]->set_pattern("%^[%T] %n: %v%$");
#if !defined(KZ_PLATFORM_ANDROID)
#if defined(KZ_RELEASE)
    // Release: 2 sinks (stdout + memória p/ o console do editor quando roda
    // debug build; em build final de jogo não há console — só o stdout).
    sinks[1]->set_pattern("[%T] [%l] %n: %v");
#else
    sinks[1]->set_pattern("[%T] [%l] %n: %v");
    sinks[2]->set_pattern("[%T] %n: %v");
#endif
#else
    sinks[1]->set_pattern("[%T] %n: %v");
#endif

    // Nível: TRACE só em Debug/editor (testador); em Release começa em INFO.
    auto level = spdlog::level::info;
#if !defined(KZ_RELEASE)
    level = spdlog::level::trace;
#endif

    s_CoreLogger = std::make_shared<spdlog::logger>("KIZURI", begin(sinks), end(sinks));
    spdlog::register_logger(s_CoreLogger);
    s_CoreLogger->set_level(level);
    s_CoreLogger->flush_on(level);

    s_AppLogger = std::make_shared<spdlog::logger>("APP", begin(sinks), end(sinks));
    spdlog::register_logger(s_AppLogger);
    s_AppLogger->set_level(level);
    s_AppLogger->flush_on(level);
}

} // namespace kizuri
