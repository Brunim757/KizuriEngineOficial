#pragma once
#include <string>

namespace kizuri {

// Compila o GameModule do projeto (pasta Source/) via CMake — o equivalente
// prático do "compile scripts" da Unity nesta v1 (ainda é C++, então precisa
// de um compilador instalado + código-fonte da engine pra linkar).
struct GameModuleBuildResult {
    bool Ok = false;
    std::string ModulePath; // .dll/.so gerado (vazio se falhou)
    std::string Log;        // saída do cmake / mensagem de erro
};

class GameModuleBuilder {
public:
    // Procura o checkout da engine (KIZURI_ENGINE_DIR, pastas irmãs, etc).
    static std::string FindEngineSourceDir();

    // cmake -B <project>/Source/build && cmake --build ...
    // Devolve o caminho do .dll/.so se deu certo.
    static GameModuleBuildResult Build(const std::string& projectSourceDir,
                                       const std::string& engineSourceDir);

    // Candidatos comuns ao lado do editor / na pasta do projeto.
    static std::string FindBuiltModule(const std::string& projectDir,
                                       const std::string& editorBinDir);
};

} // namespace kizuri
