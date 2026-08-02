#include "kizuri/project/GameModuleBuilder.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/EntryPoint.hpp"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <cstdio>
#include <vector>
#include <string>

namespace fs = std::filesystem;

namespace kizuri {

static bool DirLooksLikeEngineRoot(const fs::path& p) {
    return fs::exists(p / "engine" / "CMakeLists.txt") && fs::exists(p / "CMakeLists.txt");
}

std::string GameModuleBuilder::FindEngineSourceDir() {
    if (const char* env = std::getenv("KIZURI_ENGINE_DIR")) {
        if (DirLooksLikeEngineRoot(env)) return fs::absolute(env).string();
    }

    std::vector<fs::path> candidates;

    // Pasta do executável (bin/) e pais.
    const auto& args = GetCommandLineArgs();
    fs::path exeDir = fs::current_path();
    if (!args.empty()) {
        fs::path exe = args[0];
        if (exe.has_parent_path()) exeDir = fs::absolute(exe.parent_path());
    }
    candidates.push_back(exeDir / ".." / "Kizuri-Engine-main");
    candidates.push_back(exeDir / ".." / ".." / "Kizuri-Engine-main");
    candidates.push_back(exeDir / "Kizuri-Engine-main");
    candidates.push_back(fs::current_path() / "Kizuri-Engine-main");
    candidates.push_back(fs::path("/storage/emulated/0/Download/Kizuri-Engine-main"));

    for (auto& c : candidates) {
        std::error_code ec;
        fs::path canon = fs::weakly_canonical(c, ec);
        if (!ec && DirLooksLikeEngineRoot(canon)) return canon.string();
        if (DirLooksLikeEngineRoot(c)) return fs::absolute(c).string();
    }
    return {};
}

static std::string RunCommandCapture(const std::string& cmd, int& outExit) {
    std::string output;
#if defined(_WIN32)
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        outExit = -1;
        return "Falha ao iniciar o processo.";
    }
    std::array<char, 512> buf{};
    while (fgets(buf.data(), (int)buf.size(), pipe))
        output += buf.data();
#if defined(_WIN32)
    outExit = _pclose(pipe);
#else
    outExit = pclose(pipe);
#endif
    return output;
}

GameModuleBuildResult GameModuleBuilder::Build(const std::string& projectSourceDir,
                                               const std::string& engineSourceDir) {
    GameModuleBuildResult result;
    if (projectSourceDir.empty() || !fs::exists(projectSourceDir)) {
        result.Log = "Pasta Source/ do projeto não encontrada.";
        return result;
    }
    if (engineSourceDir.empty() || !DirLooksLikeEngineRoot(engineSourceDir)) {
        result.Log =
            "Código-fonte da Kizuri Engine não encontrado.\n"
            "Defina a variável de ambiente KIZURI_ENGINE_DIR apontando pra pasta\n"
            "Kizuri-Engine-main (a que tem engine/, editor/, CMakeLists.txt),\n"
            "ou coloque esse checkout ao lado da pasta do editor.\n\n"
            "Enquanto isso, use o libGameModule.dll / GameModule.dll que já vem\n"
            "junto do editor (Arquivo > Carregar GameModule).";
        return result;
    }

    fs::path sourceDir = projectSourceDir;
    fs::path buildDir = sourceDir / "build";
    std::error_code ec;
    fs::create_directories(buildDir, ec);

    // Aspas pra caminhos com espaço.
    auto q = [](const std::string& s) { return "\"" + s + "\""; };

    std::string configure =
        "cmake -S " + q(sourceDir.string()) +
        " -B " + q(buildDir.string()) +
        " -DKIZURI_ENGINE_DIR=" + q(engineSourceDir);

    int exitCode = 0;
    result.Log += "$ " + configure + "\n";
    result.Log += RunCommandCapture(configure, exitCode);
    if (exitCode != 0) {
        result.Log += "\nConfigure falhou (código " + std::to_string(exitCode) + ").";
        return result;
    }

    std::string build = "cmake --build " + q(buildDir.string()) + " --config Release";
    result.Log += "\n$ " + build + "\n";
    result.Log += RunCommandCapture(build, exitCode);
    if (exitCode != 0) {
        result.Log += "\nBuild falhou (código " + std::to_string(exitCode) + ").";
        return result;
    }

    // Procura o .dll/.so gerado.
    std::vector<fs::path> searchRoots = {
        buildDir / "bin",
        buildDir,
        buildDir / "Release",
        buildDir / "Debug",
    };
    for (auto& root : searchRoots) {
        if (!fs::exists(root)) continue;
        for (auto& entry : fs::directory_iterator(root, ec)) {
            if (!entry.is_regular_file()) continue;
            auto name = entry.path().filename().string();
            if (name == "GameModule.dll" || name == "libGameModule.dll" ||
                name == "libGameModule.so" || name == "GameModule.so") {
                result.Ok = true;
                result.ModulePath = entry.path().string();
                result.Log += "\nMódulo gerado: " + result.ModulePath;
                KZ_CORE_INFO("GameModule compilado: {0}", result.ModulePath);
                return result;
            }
        }
    }

    result.Log += "\nBuild terminou, mas GameModule.dll/.so não foi encontrado em Source/build.";
    return result;
}

std::string GameModuleBuilder::FindBuiltModule(const std::string& projectDir,
                                               const std::string& editorBinDir) {
    std::vector<fs::path> candidates = {
        fs::path(projectDir) / "Source" / "build" / "bin" / "GameModule.dll",
        fs::path(projectDir) / "Source" / "build" / "bin" / "libGameModule.dll",
        fs::path(projectDir) / "Source" / "build" / "bin" / "libGameModule.so",
        fs::path(projectDir) / "Source" / "build" / "GameModule.dll",
        fs::path(projectDir) / "Source" / "build" / "libGameModule.dll",
        fs::path(editorBinDir) / "libGameModule.dll",
        fs::path(editorBinDir) / "GameModule.dll",
        fs::path(editorBinDir) / "libGameModule.so",
        fs::path(editorBinDir) / "GameModule.so",
    };
    for (auto& c : candidates) {
        if (fs::exists(c)) return fs::absolute(c).string();
    }
    return {};
}

} // namespace kizuri
