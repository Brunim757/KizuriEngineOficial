#pragma once

#include <string>
#include <vector>

namespace kizuri {

// Argumentos da linha de comando, capturados pelo main() do EntryPoint e
// disponíveis pro CreateApplication() (que não recebe parâmetros). Usado
// pelo KizuriGame pra saber qual .kzscene abrir e qual GameModule carregar;
// editor e sandbox simplesmente ignoram.
//
// Vive num header próprio (e não dentro de EntryPoint.hpp) pra que qualquer
// arquivo possa usá-la com fornecer o main() zada na TU.
inline std::vector<std::string>& GetCommandLineArgs() {
    static std::vector<std::string> s_Args;
    return s_Args;
}

} // namespace kizuri