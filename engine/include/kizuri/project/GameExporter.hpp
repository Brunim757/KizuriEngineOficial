#pragma once
#include <string>

namespace kizuri {

struct GameExportRequest {
    std::string OutputDirectory;
    std::string ScenePath;
    std::string GameModulePath;
    std::string EngineBinDirectory;
    std::string GameName = "MeuJogo";
    std::string Version = "1.0";
    int WindowWidth = 1280;
    int WindowHeight = 720;

    std::string GameProjectPath;
    std::string EngineRoot;
};

class GameExporter {
public:

    static bool Export(const GameExportRequest& request, std::string& outError);

    static bool BuildGameModule(const std::string& csprojPath,
                                const std::string& engineRoot,
                                std::string& outDllPath,
                                std::string& outError);

    static bool FindGameModuleDll(const std::string& csprojPath,
                                  std::string& outDllPath);
};

}
