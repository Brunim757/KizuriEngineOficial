#pragma once
#include "kizuri/scripting/ScriptRegistry.hpp"
#include <string>

namespace kizuri {

// Carrega o módulo do jogo — a partir da v2, um assembly .NET (C#) com a
// API Kizuri.Scripting, compilado com `dotnet build` a partir da pasta
// Source/ do projeto (ver docs/NOTAS_INTERNAS.md) — e expõe o ScriptRegistry
// que ele preencheu. O runtime CoreCLR é embutido via hostfxr (CoreCLRHost).
// Tanto o editor (pra listar classes no Inspetor e rodar o Play) quanto o
// executável final do jogo usam esta mesma classe pra carregar o módulo.
class ScriptEngine {
public:
    // Carrega (ou recarrega, descarregando o anterior primeiro) o assembly
    // do jogo em 'path'. Retorna false se o arquivo não existir, não for um
    // assembly .NET válido, ou o runtime .NET não puder ser iniciado.
    static bool LoadModule(const std::string& path);
    static void UnloadModule();
    static bool IsModuleLoaded();

    // Pra UI mostrar o que aconteceu de verdade (antes só ia pro log, que ninguém olha durante
    // o uso normal). GetLastError() some assim que um LoadModule() seguinte tem sucesso.
    static const std::string& GetLastError();
    static const std::string& GetLoadedPath(); // vazio se nada carregado

    static ScriptRegistry& GetRegistry();

private:
    static ScriptRegistry s_Registry;
    static std::string s_LastError;
    static std::string s_LoadedPath;
};

} // namespace kizuri
