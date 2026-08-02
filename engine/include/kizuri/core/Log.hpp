#pragma once
#include <spdlog/spdlog.h>
#include <memory>

namespace kizuri {

// Wrapper fino sobre spdlog com dois canais: Core (engine) e App (jogo/cliente).
class Log {
public:
    static void Init();
    static std::shared_ptr<spdlog::logger>& Core()   { return s_CoreLogger; }
    static std::shared_ptr<spdlog::logger>& AppLog()  { return s_AppLogger; }

private:
    static std::shared_ptr<spdlog::logger> s_CoreLogger;
    static std::shared_ptr<spdlog::logger> s_AppLogger;
};

} // namespace kizuri

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

// RAII: loga "entrando em X" no construtor e "saindo de X" no destrutor.
// Como o log usa flush_on(trace) (grava em disco na hora, sem buffer), se
// o processo morrer no meio de uma função marcada com KZ_TRACE_SCOPE, a
// última linha do arquivo de log é sempre "entrando em <função que
// travou>" — sem precisar de debugger anexado, só olhando o arquivo
// depois. É o que torna prático caçar crash silencioso (sem exceção, sem
// mensagem) em ambiente onde não dá pra anexar debugger, tipo builds
// rodando em máquina/emulador de terceiros.
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

} // namespace kizuri

// KZ_TRACE_SCOPE("Classe::Metodo") no topo de uma função cobre entrada E
// saída com uma linha só — não precisa mais de um par de KZ_CORE_TRACE
// manual no início e no fim de cada função que a gente queira rastrear.
#define KZ_TRACE_SCOPE(name) ::kizuri::ScopeTracer kz_scope_tracer_##__LINE__(name)
