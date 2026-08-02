#pragma once
#include <string>
#include <vector>

namespace kizuri {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

struct LogEntry {
    LogLevel Level;
    std::string Message; // já formatada (nome do logger + texto)
};

// Histórico de log em memória, alimentado por um sink customizado do
// spdlog (ver core/Log.cpp) — é o que a aba Console do editor lê. Não
// depende de nenhuma UI: qualquer front-end pode consumir isso, o editor
// é só o primeiro consumidor.
class LogHistory {
public:
    static void Push(LogLevel level, const std::string& message);
    static const std::vector<LogEntry>& GetEntries();
    static void Clear();

private:
    static std::vector<LogEntry> s_Entries;
    static constexpr size_t kMaxEntries = 2000;
};

} // namespace kizuri
