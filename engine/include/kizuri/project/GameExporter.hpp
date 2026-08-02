#pragma once
#include <string>

namespace kizuri {

// Empacota um jogo jogável numa pasta: KizuriGame + engine + cena +
// assets referenciados + GameModule opcional. Caminhos da cena viram
// relativos à pasta exportada (Assets/...).
struct GameExportRequest {
    std::string OutputDirectory;   // pasta de destino (criada se não existir)
    std::string ScenePath;         // .kzscene de entrada
    std::string GameModulePath;    // opcional (.dll/.so)
    std::string EngineBinDirectory; // pasta com KizuriGame + KizuriEngine (+ runtime MinGW)
    std::string GameName = "MeuJogo"; // nome do .exe final (sem extensão)
};

class GameExporter {
public:
    // Devolve true se a pasta ficou pronta pra rodar (KizuriGame Start.kzscene [GameModule]).
    static bool Export(const GameExportRequest& request, std::string& outError);
};

} // namespace kizuri
