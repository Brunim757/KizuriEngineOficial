#pragma once









#include <cstdint>
#include <string>

namespace kizuri {
namespace scripting {

class CoreCLRHost {
public:
    
    
    
    static bool Initialize(const std::string& runtimeConfigPath,
                           const std::string& assemblyPath,
                           std::string& outError);
    static void Shutdown();
    static bool IsInitialized();

    
    static int GetScriptCount();
    static std::string GetScriptName(int index);

    
    
    static std::string GetLastInitError();

    
    static void* CreateScript(const std::string& className, uint32_t entityHandle);
    static void DestroyScript(void* handle);
    static void UpdateScript(void* handle, float deltaSeconds);
    static void CollisionScript(void* handle, uint32_t otherHandle, bool begin);

private:
    struct Impl;
    static Impl* s_Impl;
};

} 
} 
