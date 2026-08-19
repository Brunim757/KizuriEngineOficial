#pragma once

#include <string>
#include <vector>

namespace kizuri {

inline std::vector<std::string>& GetCommandLineArgs() {
    static std::vector<std::string> s_Args;
    return s_Args;
}

}