#include "kizuri/project/GameModuleBuilder.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/CommandLineArgs.hpp"

#include <filesystem>
#include <fstream>
#include <cstdlib>
#include <array>
#include <cstdio>
#include <vector>
#include <string>

namespace fs = std::filesystem;

namespace kizuri {

// Executa um comando via popen e captura a saída. Definida mais abaixo;
// declarada aqui porque FindCompiler/BuildCompileScripts usam ela.
static std::string RunCommandCapture(const std::string& cmd, int& outExit);

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

// ---------------------------------------------------------------------------
// "Compilar Scripts" estilo Unity (SDK embutido). Ver
// cmake/StageSDK.cmake pra como o bin/sdk é montado no build da engine.
// ---------------------------------------------------------------------------

std::string GameModuleBuilder::FindSdkDir(const std::string& editorBinDir) {
    if (const char* env = std::getenv("KIZURI_SDK_DIR")) {
        if (fs::exists(fs::path(env) / "include")) return fs::absolute(env).string();
    }

    std::vector<fs::path> candidates;
    if (!editorBinDir.empty()) {
        candidates.push_back(fs::path(editorBinDir) / "sdk");
        candidates.push_back(fs::path(editorBinDir) / ".." / "sdk");
        candidates.push_back(fs::path(editorBinDir) / ".." / ".." / "sdk");
    }
    candidates.push_back(fs::current_path() / "sdk");
    candidates.push_back(fs::path("/storage/emulated/0/Download/Kizuri-Engine-main") / "build" / "bin" / "sdk");

    for (auto& c : candidates) {
        std::error_code ec;
        fs::path canon = fs::weakly_canonical(c, ec);
        if (!ec && fs::exists(canon / "include")) return canon.string();
        if (fs::exists(c / "include")) return fs::absolute(c).string();
    }
    return {};
}

static std::string FindCompiler() {
    if (const char* env = std::getenv("KIZURI_CXX")) {
        // Env apontando pra um caminho existente (g++/clang++/cl do usuário).
        std::error_code ec;
        if (fs::exists(env, ec)) return env;
        // Não é um caminho — pode ser um nome de binário no PATH; o popen
        // resolve quando rodarmos.
        return env;
    }

#if defined(_WIN32)
    const char* names[] = { "g++", "g++.exe", "clang++", "clang++.exe", "cl", "cl.exe", nullptr };
#else
    const char* names[] = { "c++", "g++", "clang++", "cc", "gcc", nullptr };
#endif
    for (int i = 0; names[i]; ++i) {
        int out;
        std::string cmd = std::string(names[i]) + " --version";
        std::string res = RunCommandCapture(cmd, out);
        if (!res.empty() && out == 0)
            return names[i];
    }
    return {};
}

static std::string ShellQuote(const std::string& s) {
    // Aspas simples quando possível (como em popen não passa por shell num
    // parser complexo no Windows... usamos dupla, escapando) — simples:
    if (s.find_first_of(" \t") == std::string::npos) return s;
    std::string quoted = "\"";
    for (char c : s) {
        if (c == '"') quoted += "\\\"";
        else quoted += c;
    }
    quoted += "\"";
    return quoted;
}

static std::string MakeModuleName() {
#if defined(_WIN32)
    return "GameModule.dll";
#else
    return "libGameModule.so";
#endif
}

GameModuleBuildResult GameModuleBuilder::BuildCompileScripts(
    const std::string& projectSourceDir,
    const std::string& sdkDir) {

    GameModuleBuildResult result;
    if (projectSourceDir.empty() || !fs::exists(projectSourceDir)) {
        result.Log = "Pasta Source/ do projeto não encontrada.";
        return result;
    }
    if (sdkDir.empty() || !fs::exists(fs::path(sdkDir) / "include")) {
        result.Log =
            "SDK embutido não encontrado (procurei em bin/sdk e KIZURI_SDK_DIR).\n"
            "Rode o build da engine uma vez (gera bin/sdk) ou baixe o release.";
        return result;
    }

    std::string compiler = FindCompiler();
    if (compiler.empty()) {
        result.Log =
            "Nenhum compilador C++20 encontrado. Instale MinGW/GCC/Clang\n"
            "(ou defina a variável de ambiente KIZURI_CXX apontando pra ele).";
        return result;
    }

    // Coleta os .cpp do projeto (e subpastas).
    std::vector<std::string> sources;
    for (fs::recursive_directory_iterator it(projectSourceDir), end; it != end; ++it) {
        if (!it->is_regular_file()) continue;
        auto ext = it->path().extension();
        if (ext == ".cpp" || ext == ".cc" || ext == ".cxx")
            sources.push_back(it->path().string());
    }
    if (sources.empty()) {
        result.Log = "Nenhum arquivo .cpp encontrado em Source/.";
        return result;
    }

    fs::path buildDir = fs::path(projectSourceDir) / "build";
    fs::create_directories(buildDir);
    fs::path outModule = buildDir / MakeModuleName();

    // Flags básicas; o MSVC é contornado via cl com /EHsc (mais simples usar MinGW).
    std::string flags;
#if defined(_WIN32)
    // MinGW: SHARED é o padrão pro GCC; -shared explicita. O executável roda
    // com gcc; a stack de exceção iguais do DLL do editor (SHARED runtime C++).
    flags = "-std=c++20 -shared -O2 -fPIC -fvisibility=default";
#else
    flags = "-std=c++20 -shared -fPIC -O2";
#endif

    std::string cmd = ShellQuote(compiler) + " " + flags +
        " -I" + ShellQuote(sdkDir + "/include") +
        " -I" + ShellQuote(projectSourceDir);
    for (auto& s : sources) cmd += " " + ShellQuote(s);

    // Link contra a engine. O SDK guarda a lib de import (Windows) em sdk/lib;
    // no Linux linkamos direto contra o libKizuriEngine.so, que vive no bin do
    // editor (que, no layout padrão, é justamente o pai de sdk/).
#if defined(_WIN32)
    cmd += " -L" + ShellQuote((fs::path(sdkDir) / "lib").string()) + " -l:libKizuriEngine.dll.a";
#else
    // Linux: RPATH aponta pro diretório onde a libKizuriEngine.so vive (bin do
    // editor). O módulo fica em <Source>/build, longe da engine — $ORIGIN não
    // acharia ela em runtime.
    std::string engineLibDir = (fs::path(sdkDir) / "..").string();
    if (fs::exists(fs::path(sdkDir) / "lib" / "libKizuriEngine.so"))
        engineLibDir = (fs::path(sdkDir) / "lib").string();
    cmd += " -L" + ShellQuote(engineLibDir) + " -l:libKizuriEngine.so";
    cmd += " -Wl,-rpath," + ShellQuote(engineLibDir);
#endif
    cmd += " -o " + ShellQuote(outModule.string());

    result.Log += "$ " + cmd + "\n\n";
    int exitCode = 0;
    std::string output = RunCommandCapture(cmd, exitCode);
    result.Log += output;
    if (exitCode != 0) {
        result.ModulePath.clear();
        result.Log += "\n\nCompilação falhou (código " + std::to_string(exitCode) + ").";
        return result;
    }
    if (!fs::exists(outModule)) {
        result.Log += "\n\nO link terminou mas o módulo não foi gerado.";
        return result;
    }

    result.Ok = true;
    result.ModulePath = fs::absolute(outModule).string();
    result.Log += "\n\nMódulo gerado: " + result.ModulePath;
    return result;
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
