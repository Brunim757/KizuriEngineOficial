#pragma once
// CoreCLRHost.hpp — host embutido do runtime .NET (CoreCLR) via hostfxr.
// É o que permite o jogo ser um assembly C# (Kizuri.Scripting.dll) em vez
// de uma DLL C++: inicializa o runtime, carrega o assembly do jogo, dispara
// os pontos de entrada [GameEntryPoint] (que registram scripts) e expõe as
// funções managed de ciclo de vida dos scripts.
//
// O hostfxr é carregado DINAMICAMENTE em runtime (LoadLibrary/dlopen) — só
// os headers do hosting .NET são necessários no build (pasta dotnet/), nada
// de linkar o SDK.
#include <cstdint>
#include <string>

namespace kizuri {
namespace scripting {

class CoreCLRHost {
public:
    // 'runtimeConfigPath' aponta pro .runtimeconfig.json do jogo (fica do
    // lado do assembly). 'assemblyPath' é o caminho do assembly do jogo
    // (usado como fallback de resolução da Kizuri.Scripting.dll).
    static bool Initialize(const std::string& runtimeConfigPath,
                           const std::string& assemblyPath,
                           std::string& outError);
    static void Shutdown();
    static bool IsInitialized();

    // Lista os scripts registrados pelo jogo (consulta o managed).
    static int GetScriptCount();
    static std::string GetScriptName(int index);

    // Texto do último erro de inicialização do módulo (o lado managed guarda
    // a exceção real em vez de engoli-la — ver Host.InitializeGameModule).
    static std::string GetLastInitError();

    // Ciclo de vida de um script C#. 'handle' é opaco (GCHandle).
    static void* CreateScript(const std::string& className, uint32_t entityHandle);
    static void DestroyScript(void* handle);
    static void UpdateScript(void* handle, float deltaSeconds);
    static void CollisionScript(void* handle, uint32_t otherHandle, bool begin);

private:
    struct Impl;
    static Impl* s_Impl;
};

} // namespace scripting
} // namespace kizuri
