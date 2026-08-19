#include "kizuri/core/LogHistory.hpp"

namespace kizuri {

std::vector<LogEntry> LogHistory::s_Entries;

void LogHistory::Push(LogLevel level, const std::string& message) {
    s_Entries.push_back({ level, message });
    if (s_Entries.size() > kMaxEntries)
        s_Entries.erase(s_Entries.begin(), s_Entries.begin() + (s_Entries.size() - kMaxEntries));
}

const std::vector<LogEntry>& LogHistory::GetEntries() { return s_Entries; }

void LogHistory::Clear() { s_Entries.clear(); }

}
