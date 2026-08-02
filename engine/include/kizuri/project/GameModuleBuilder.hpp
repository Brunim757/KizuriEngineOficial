#pragma once
#include <string>
#include <vector>

namespace kizuri {

// Resultado de uma compilação de GameModule (seja via CMake quer via SDK).
struct GameModuleBuildResult {
    bool Ok = false;
    std::string ModulePath; // .dll/.so gerado (vazio se falhou)
    std::string Log;        // saída do compilador / mensagem de erro
};

class GameModuleBuilder {
public:
    // Procura o checkout da engine (KIZURI_ENGINE_DIR, pastas irmãs, etc).
    static std::string FindEngineSourceDir();

    // -----------------------------------------------------------------------
    // "Compilar Scripts" estilo Unity: recompila Source/ de um projeto com o
    // compilador instalado, usando o SDK embutido (bin/sdk) — SEM checkout do
    // código-fonte e SEM rodar cmake em runtime. Encontra o SDK procurando
    // (nessa ordem): env KIZURI_SDK_DIR, <binDoEditor>/sdk, .../../sdk.
    // O módulo gerado é salvo em <projectSourceDir>/build/
    // (libGameModule.dll / libGameModule.so) e auto-carregado pelo editor.
    // -----------------------------------------------------------------------
    static std::string FindSdkDir(const std::string& editorBinDir);

    // Compila Source/*.cpp contra o SDK com o compilador detectado
    // (KIZURI_CXX env ou g++/clang++/cl no PATH). Retorna resultado.
    static GameModuleBuildResult BuildCompileScripts(
        const std::string& projectSourceDir,
        const std::string& sdkDir);

    // -----------------------------------------------------------------------
    // Caminho antigo (mantido): compila o projeto via cmake externo — usado
    // como fallback quando o usuário não tem o SDK embutido.
    // cmake -B <project>/Source/build && cmake --build ...
    // Devolve o caminho do .dll/.so se deu certo.
    // -------------------------------------------------------------------------
    static GameModuleBuildResult Build(const std::string& projectSourceDir,
                                       const std::string& engineSourceDir);

    // Candidatos comuns ao lado do editor / na pasta do projeto.
    static std::string FindBuiltModule(const std::string& projectDir,
                                       const std::string& editorBinDir);
};

} // namespace kizuri
