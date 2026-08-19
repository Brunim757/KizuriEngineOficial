#pragma once
#include "kizuri/scripting/ScriptRegistry.hpp"
#include <string>

namespace kizuri {

class ScriptEngine {
public:

    static bool LoadModule(const std::string& path);
    static void UnloadModule();
    static bool IsModuleLoaded();

    static const std::string& GetLastError();
    static const std::string& GetLoadedPath();

    static ScriptRegistry& GetRegistry();

private:
    static ScriptRegistry s_Registry;
    static std::string s_LastError;
    static std::string s_LoadedPath;
};

}
