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

    // Compila o assembly C# do jogo com `dotnet build -c Debug` e devolve o
    // caminho da .dll resultante (a que tem .runtimeconfig.json do lado, a
    // mais recente em <projeto>/bin). É o que o editor usa pra "compilar no
    // Play", estilo Unity. 'engineRoot' alimenta o -p:EngineDir (raiz do
    // checkout da engine, opcional se o csproj já resolve por outra via).
    static bool BuildGameModule(const std::string& csprojPath,
                                const std::string& engineRoot,
                                std::string& outDllPath,
                                std::string& outError);

    // Acha a .dll do jogo JÁ COMPILADA (a mais recente com .runtimeconfig
    // em <projeto>/bin) sem compilar nada — caminho rápido pra abrir projeto
    // sem travar o editor (o build real fica pro Play, estilo Unity).
    static bool FindGameModuleDll(const std::string& csprojPath,
                                  std::string& outDllPath);
};

} // namespace kizuri
