#pragma once
#include <string>
#include <vector>

namespace kizuri {

enum class LogLevel { Trace, Debug, Info, Warn, Error, Critical };

struct LogEntry {
    LogLevel Level;
    std::string Message;
};

class LogHistory {
public:
    static void Push(LogLevel level, const std::string& message);
    static const std::vector<LogEntry>& GetEntries();
    static void Clear();

private:
    static std::vector<LogEntry> s_Entries;
    static constexpr size_t kMaxEntries = 2000;
};

}
