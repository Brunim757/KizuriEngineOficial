#pragma once
#include <string>

namespace kizuri {

// Empacota um jogo jogável numa pasta: KizuriGame + engine + cena +
// assets referenciados + GameModule opcional. Caminhos da cena viram
// relativos à pasta exportada (Assets/...).
struct GameExportRequest {
    std::string OutputDirectory;   // pasta de destino (criada se não existir)
    std::string ScenePath;         // .kzscene de entrada
    std::string GameModulePath;    // opcional (.dll/.so) — fallback quando não há projeto pra publicar
    std::string EngineBinDirectory; // pasta com KizuriGame + KizuriEngine (+ runtime MinGW)
    std::string GameName = "MeuJogo"; // nome do .exe final (sem extensão)

    // Se definido, o jogo é publicado via `dotnet publish --self-contained`
    // (embute o runtime .NET — o jogador não precisa instalar nada) em vez de
    // copiar a pasta do assembly compilado. O resultado vai pra <out>/Game/.
    std::string GameProjectPath;   // caminho do .csproj do jogo (ex: <Projeto>/Source/Game.csproj)
    std::string EngineRoot;        // raiz do checkout da engine (usado como -p:EngineDir no publish)
};

class GameExporter {
public:
    // Devolve true se a pasta ficou pronta pra rodar (KizuriGame Start.kzscene [GameModule]).
    static bool Export(const GameExportRequest& request, std::string& outError);
};

} // namespace kizuri
