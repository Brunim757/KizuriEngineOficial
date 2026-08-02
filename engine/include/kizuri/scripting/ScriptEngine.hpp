#pragma once
#include "kizuri/scripting/ScriptRegistry.hpp"
#include <string>

namespace kizuri {

// Carrega o GameModule — a biblioteca dinâmica com o código C++ do jogo,
// compilada a partir da pasta Source/ dentro do projeto (ver
// docs/NOTAS_INTERNAS.md) — e expõe o ScriptRegistry que ele preencheu.
// Tanto o editor (pra listar classes no Inspetor e rodar o Play) quanto o
// executável final do jogo usam esta mesma classe pra carregar o módulo;
// a diferença entre os dois fica em como cada um resolve o caminho da
// biblioteca, não em como ela é carregada.
class ScriptEngine {
public:
    // Carrega (ou recarrega, descarregando o anterior primeiro) a
    // biblioteca em 'path'. Retorna false se o arquivo não existir ou não
    // exportar uma função 'RegisterScripts(ScriptRegistry&)' com linkage C.
    static bool LoadModule(const std::string& path);
    static void UnloadModule();
    static bool IsModuleLoaded();

    // Pra UI mostrar o que aconteceu de verdade (antes só ia pro log, que ninguém olha durante
    // o uso normal). GetLastError() some assim que um LoadModule() seguinte tem sucesso.
    static const std::string& GetLastError();
    static const std::string& GetLoadedPath(); // vazio se nada carregado

    static ScriptRegistry& GetRegistry();

private:
    static void* s_ModuleHandle;
    static ScriptRegistry s_Registry;
    static std::string s_LastError;
    static std::string s_LoadedPath;
};

} // namespace kizuri
