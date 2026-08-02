#include "kizuri/core/Log.hpp"
#include "kizuri/core/LogHistory.hpp"
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/fmt/fmt.h>
#include <vector>
#include <mutex>

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
class MemorySink : public spdlog::sinks::base_sink<std::mutex> {
protected:
    void sink_it_(const spdlog::details::log_msg& msg) override {
        spdlog::memory_buf_t formatted;
        formatter_->format(msg, formatted);
        LogHistory::Push(ToLogLevel(msg.level), fmt::to_string(formatted));
    }
    void flush_() override {}
};

void Log::Init() {
    std::vector<spdlog::sink_ptr> sinks;
    sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());
    sinks.push_back(std::make_shared<spdlog::sinks::basic_file_sink_mt>("KizuriEngine.log", true));
    sinks.push_back(std::make_shared<MemorySink>());

    sinks[0]->set_pattern("%^[%T] %n: %v%$");
    sinks[1]->set_pattern("[%T] [%l] %n: %v");
    sinks[2]->set_pattern("[%T] %n: %v");

    s_CoreLogger = std::make_shared<spdlog::logger>("KIZURI", begin(sinks), end(sinks));
    spdlog::register_logger(s_CoreLogger);
    s_CoreLogger->set_level(spdlog::level::trace);
    s_CoreLogger->flush_on(spdlog::level::trace);

    s_AppLogger = std::make_shared<spdlog::logger>("APP", begin(sinks), end(sinks));
    spdlog::register_logger(s_AppLogger);
    s_AppLogger->set_level(spdlog::level::trace);
    s_AppLogger->flush_on(spdlog::level::trace);
}

} // namespace kizuri
