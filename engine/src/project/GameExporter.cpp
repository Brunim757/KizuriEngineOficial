#include "kizuri/project/GameExporter.hpp"
#include "kizuri/core/Log.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>
#include <cstdio>
#include <cstdlib>

#if defined(_WIN32)
    #ifndef NOMINMAX
        #define NOMINMAX
    #endif
    #include <windows.h>
#else
    #include <sys/wait.h>
    #include <unistd.h>
    #include <limits.h>
#endif

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace kizuri {

static bool LooksLikeAssetPath(const std::string& s) {
    if (s.empty() || s.rfind("builtin:", 0) == 0) return false;
    // Extensões comuns serializadas em .kzscene
    static const char* exts[] = {
        ".png", ".jpg", ".jpeg", ".bmp", ".tga", ".gif",
        ".obj", ".wav", ".mp3", ".ogg", ".flac", ".kzprefab"
    };
    std::string lower = s;
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return (char)std::tolower(c); });
    for (const char* ext : exts) {
        size_t n = std::strlen(ext);
        if (lower.size() >= n && lower.compare(lower.size() - n, n, ext) == 0)
            return true;
    }
    return false;
}

static void CollectAssetPaths(const json& node, std::unordered_set<std::string>& out) {
    if (node.is_string()) {
        const std::string& s = node.get_ref<const std::string&>();
        if (LooksLikeAssetPath(s)) out.insert(s);
        return;
    }
    if (node.is_array()) {
        for (const auto& child : node) CollectAssetPaths(child, out);
        return;
    }
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it)
            CollectAssetPaths(it.value(), out);
    }
}

static void RewriteAssetPaths(json& node, const std::unordered_map<std::string, std::string>& remap) {
    if (node.is_string()) {
        auto it = remap.find(node.get<std::string>());
        if (it != remap.end()) node = it->second;
        return;
    }
    if (node.is_array()) {
        for (auto& child : node) RewriteAssetPaths(child, remap);
        return;
    }
    if (node.is_object()) {
        for (auto it = node.begin(); it != node.end(); ++it)
            RewriteAssetPaths(it.value(), remap);
    }
}

static bool CopyFileTo(const fs::path& src, const fs::path& dst, std::string& err) {
    std::error_code ec;
    fs::create_directories(dst.parent_path(), ec);
    fs::copy_file(src, dst, fs::copy_options::overwrite_existing, ec);
    if (ec) {
        err = "Falha ao copiar '" + src.string() + "': " + ec.message();
        return false;
    }
    return true;
}

// Copia a pasta inteira (usada pro jogo C#: assembly + deps + runtimeconfig
// + runtime .NET self-contained, se o usuário publicou assim).
static bool CopyDirectoryRecursive(const fs::path& srcDir, const fs::path& dstDir, std::string& err) {
    std::error_code ec;
    fs::create_directories(dstDir, ec);
    if (ec) {
        err = "Falha ao criar a pasta de destino: " + dstDir.string();
        return false;
    }
    for (auto& entry : fs::recursive_directory_iterator(srcDir, ec)) {
        if (ec) { err = "Falha ao iterar '" + srcDir.string() + "': " + ec.message(); return false; }
        fs::path rel = fs::relative(entry.path(), srcDir, ec);
        if (ec) continue;
        fs::path dst = dstDir / rel;
        if (entry.is_directory(ec)) {
            fs::create_directories(dst, ec);
            continue;
        }
        if (!entry.is_regular_file(ec)) continue;
        if (!CopyFileTo(entry.path(), dst, err)) return false;
    }
    return true;
}

// UTF-8 -> UTF-16 (só Windows; pro CreateProcessW do publish).
#if defined(_WIN32)
static std::wstring ToWide(const std::string& s) {
    if (s.empty()) return L"";
    int len = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), nullptr, 0);
    std::wstring out(static_cast<size_t>(len), L'\0');
    if (len > 0)
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &out[0], len);
    return out;
}
#endif

// Roda 'command' redirecionando stdout+stderr pra 'logFile' e espera o fim.
// Devolve o código de saída (0 = sucesso). Sem janela de console no Windows
// (CREATE_NO_WINDOW) — exportar não pode piscar um cmd pro usuário.
static int RunAndCapture(const std::string& command, const fs::path& logFile, std::string& outError) {
#if defined(_WIN32)
    std::string full = "cmd.exe /c \"" + command + " > \"" + logFile.string() + "\" 2>&1\"";
    std::wstring wfull = ToWide(full);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi = {};
    if (!CreateProcessW(nullptr, &wfull[0], nullptr, nullptr, FALSE,
                        CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi)) {
        outError = "Falha ao iniciar o processo (CreateProcess): " + command;
        return -1;
    }
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return (int)code;
#else
    std::string full = command + " > \"" + logFile.string() + "\" 2>&1";
    int status = std::system(full.c_str());
    if (status == -1) { outError = "Falha ao iniciar o processo: " + command; return -1; }
    return WIFEXITED(status) ? WEXITSTATUS(status) : status;
#endif
}

// RID padrão = plataforma do desenvolvedor (o KizuriGame copiado é o build
// local, então o runtime publicado precisa casar com ele).
static std::string DetectRid() {
#if defined(_WIN32)
    return "win-x64";
#elif defined(__APPLE__)
    return "osx-x64";
#else
    return "linux-x64";
#endif
}

// Últimas linhas da saída do publish (pra o erro no modal não virar um
// arquivo de 2MB de uma vez).
static std::string Tail(const std::string& text, size_t maxChars) {
    if (text.size() <= maxChars) return text;
    size_t start = text.size() - maxChars;
    size_t nl = text.find('\n', start);
    return (nl == std::string::npos) ? text.substr(start) : text.substr(nl + 1);
}

// No publish self-contained, o assembly do jogo é o .dll que tem um
// .runtimeconfig.json do lado (a Kizuri.Scripting.dll não tem). Devolve o
// nome do arquivo (ex: "Game.dll") ou vazio com 'err' preenchido.
static std::string FindGameAssembly(const fs::path& gameDir, std::string& err) {
    std::error_code ec;
    for (auto& entry : fs::directory_iterator(gameDir, ec)) {
        if (!entry.is_regular_file(ec)) continue;
        if (entry.path().extension().string() != ".dll") continue;
        fs::path rc = entry.path();
        rc.replace_extension(".runtimeconfig.json");
        if (fs::is_regular_file(rc, ec)) return entry.path().filename().string();
    }
    err = "Nenhum assembly de jogo (com .runtimeconfig.json do lado) encontrado em "
          + gameDir.string();
    return {};
}

#ifdef _WIN32
    constexpr const char* kHostfxrName = "hostfxr.dll";
    constexpr const char* kHostpolicyName = "hostpolicy.dll";
#else
    constexpr const char* kHostfxrName = "libhostfxr.so";
    constexpr const char* kHostpolicyName = "libhostpolicy.so";
#endif

// Compara versões numéricas ("10.0.0" > "9.0.0" — comparação lexical de
// string erraria).
static bool VersionGreater(const std::string& a, const std::string& b) {
    auto nums = [](const std::string& s, std::vector<int>& out) {
        std::string cur;
        for (char c : s) {
            if (c == '.') { out.push_back(std::atoi(cur.c_str())); cur.clear(); }
            else cur += c;
        }
        if (!cur.empty()) out.push_back(std::atoi(cur.c_str()));
    };
    std::vector<int> pa, pb;
    nums(a, pa); nums(b, pb);
    for (size_t i = 0; i < pa.size() || i < pb.size(); ++i) {
        int x = i < pa.size() ? pa[i] : 0;
        int y = i < pb.size() ? pb[i] : 0;
        if (x != y) return x > y;
    }
    return false;
}

static fs::path FindDotnetRoot() {
    std::error_code ec;
    if (const char* root = std::getenv("DOTNET_ROOT"))
        if (fs::is_directory(root, ec)) return root;
#ifdef _WIN32
    if (fs::is_directory("C:\\Program Files\\dotnet", ec)) return "C:\\Program Files\\dotnet";
    if (fs::is_directory("C:\\Program Files (x86)\\dotnet", ec)) return "C:\\Program Files (x86)\\dotnet";
#else
    if (fs::is_directory("/usr/share/dotnet", ec)) return "/usr/share/dotnet";
    if (fs::is_directory("/usr/local/share/dotnet", ec)) return "/usr/local/share/dotnet";
    if (const char* home = std::getenv("HOME")) {
        fs::path p = fs::path(home) / ".dotnet";
        if (fs::is_directory(p, ec)) return p;
    }
#endif
    return {};
}

// Copia o runtime .NET instalado na máquina pra dentro de gameDir, deixando
// o jogo self-contained (o jogador final não instala nada). Layout esperado
// pelo hostfxr com dotnet_root = gameDir:
//   gameDir/hostfxr.dll, gameDir/hostpolicy.dll
//   gameDir/shared/Microsoft.NETCore.App/<versão>/*
static bool EmbedDotnetRuntime(const fs::path& gameDir, std::string& outError) {
    fs::path dotnetRoot = FindDotnetRoot();
    if (dotnetRoot.empty()) {
        outError = "Runtime .NET não encontrado na máquina para embutir no jogo. "
                   "Instale o .NET ou exporte com self-contained (checkbox).";
        return false;
    }

    std::error_code ec;
    fs::path fxrDir, sharedDir;
    std::string fxrVer, fwVer;
    for (auto& e : fs::directory_iterator(dotnetRoot / "host" / "fxr", ec)) {
        if (!e.is_directory(ec)) continue;
        std::string v = e.path().filename().string();
        if (fxrDir.empty() || VersionGreater(v, fxrVer)) { fxrVer = v; fxrDir = e.path(); }
    }
    for (auto& e : fs::directory_iterator(dotnetRoot / "shared" / "Microsoft.NETCore.App", ec)) {
        if (!e.is_directory(ec)) continue;
        std::string v = e.path().filename().string();
        if (sharedDir.empty() || VersionGreater(v, fwVer)) { fwVer = v; sharedDir = e.path(); }
    }
    if (fxrDir.empty() || sharedDir.empty()) {
        outError = "Instalação .NET incompleta (sem host/fxr ou shared framework).";
        return false;
    }

    if (!CopyFileTo(fxrDir / kHostfxrName, gameDir / kHostfxrName, outError)) return false;
    if (fs::exists(fxrDir / kHostpolicyName, ec) &&
        !CopyFileTo(fxrDir / kHostpolicyName, gameDir / kHostpolicyName, outError))
        return false;

    if (!CopyDirectoryRecursive(sharedDir, gameDir / "shared" / "Microsoft.NETCore.App" / fwVer, outError))
        return false;

    KZ_CORE_INFO("Runtime .NET embutido no jogo: v{0} (vem da máquina, sem download).", fwVer);
    return true;
}

bool GameExporter::Export(const GameExportRequest& request, std::string& outError) {
    if (request.OutputDirectory.empty()) {
        outError = "Pasta de destino vazia.";
        return false;
    }
    if (request.ScenePath.empty()) {
        outError = "Cena inicial não definida.";
        return false;
    }
    if (request.EngineBinDirectory.empty()) {
        outError = "Pasta dos binários da engine não informada.";
        return false;
    }

    fs::path outDir = request.OutputDirectory;
    fs::path binDir = request.EngineBinDirectory;
    fs::path sceneSrc = request.ScenePath;

    std::error_code ec;
    fs::create_directories(outDir, ec);
    if (ec) {
        outError = "Não foi possível criar a pasta de exportação: " + ec.message();
        return false;
    }

    // Host + DLL da engine (+ runtimes MinGW se existirem na pasta bin).
    const char* required[] = {
#ifdef _WIN32
        "KizuriGame.exe", "KizuriEngine.dll"
#else
        "KizuriGame", "libKizuriEngine.so"
#endif
    };
    for (const char* name : required) {
        fs::path src = binDir / name;
        if (!fs::exists(src)) {
            outError = std::string("Binário não encontrado: ") + src.string()
                + " (compile KizuriGame antes de exportar).";
            return false;
        }
        if (!CopyFileTo(src, outDir / name, outError)) return false;
    }

    // Copia DLLs auxiliares que costumam acompanhar o build MinGW.
    for (auto& entry : fs::directory_iterator(binDir, ec)) {
        if (!entry.is_regular_file()) continue;
        auto ext = entry.path().extension().string();
        std::transform(ext.begin(), ext.end(), ext.begin(),
                       [](unsigned char c) { return (char)std::tolower(c); });
        if (ext != ".dll") continue;
        auto name = entry.path().filename().string();
        if (name == "KizuriEngine.dll" || name == "GameModule.dll") continue;
        if (!CopyFileTo(entry.path(), outDir / name, outError)) return false;
    }

    // Scene + assets
    std::ifstream in(sceneSrc);
    if (!in.is_open()) {
        outError = "Não foi possível abrir a cena: " + sceneSrc.string();
        return false;
    }
    json root;
    try { in >> root; }
    catch (const std::exception& e) {
        outError = std::string("JSON da cena inválido: ") + e.what();
        return false;
    }

    std::unordered_set<std::string> assets;
    CollectAssetPaths(root, assets);

    fs::path assetsOut = outDir / "Assets";
    fs::create_directories(assetsOut, ec);

    std::unordered_map<std::string, std::string> remap;
    fs::path sceneDir = sceneSrc.parent_path();

    for (const auto& original : assets) {
        fs::path srcPath(original);
        if (!srcPath.is_absolute()) {
            // Tenta relativo à cena, depois CWD.
            fs::path cand = sceneDir / srcPath;
            if (fs::exists(cand)) srcPath = cand;
            else srcPath = fs::absolute(srcPath);
        }
        if (!fs::exists(srcPath)) {
            KZ_CORE_WARN("GameExporter: asset não encontrado, ignorado: {0}", original);
            continue;
        }

        std::string fileName = srcPath.filename().string();
        fs::path dst = assetsOut / fileName;
        // Evita colisão de nomes iguais vindos de pastas diferentes.
        if (fs::exists(dst) && fs::equivalent(dst, srcPath) == false) {
            static int counter = 0;
            dst = assetsOut / (srcPath.stem().string() + "_" + std::to_string(++counter) + srcPath.extension().string());
        }
        if (!CopyFileTo(srcPath, dst, outError)) return false;
        remap[original] = (fs::path("Assets") / dst.filename()).generic_string();
    }

    RewriteAssetPaths(root, remap);

    fs::path sceneOut = outDir / "Start.kzscene";
    std::ofstream sceneFile(sceneOut);
    if (!sceneFile.is_open()) {
        outError = "Não foi possível escrever Start.kzscene";
        return false;
    }
    sceneFile << root.dump(4);

    // Módulo do jogo. Dois caminhos:
    //  1. GameProjectPath definido → `dotnet publish --self-contained` do
    //     csproj do jogo pra <out>/Game/ (runtime .NET embutido — o jogador
    //     não instala nada). É o fluxo normal de projeto.
    //  2. Só GameModulePath → cópia da pasta do assembly (fallback; só serve
    //     se já estiver publicado self-contained ou o jogador tiver o runtime).
    fs::path moduleOutPath;
    if (!request.GameProjectPath.empty()) {
        fs::path gameDir = outDir / "Game";
        fs::path logPath = outDir / "publish.log";
        std::error_code pec;
        fs::create_directories(gameDir, pec);

        std::string cmd = "\"" + ResolveDotnetCli() + "\" publish \"" + request.GameProjectPath
            + "\" -c Release -r " + DetectRid()
            + " --self-contained true";
        if (!request.EngineRoot.empty())
            cmd += " -p:EngineDir=\"" + request.EngineRoot + "\"";
        cmd += " -o \"" + gameDir.string() + "\"";

        KZ_CORE_INFO("Publicando jogo self-contained: {0}", cmd);
        std::string processError;
        int rc = RunAndCapture(cmd, logPath, processError);
        bool ok = (rc == 0) && fs::is_regular_file(gameDir / "Kizuri.Scripting.dll", pec);
        if (!ok) {
            std::string logText = processError;
            std::ifstream logIn(logPath);
            if (logIn.is_open()) {
                std::stringstream ss;
                ss << logIn.rdbuf();
                logText = ss.str();
            }
            outError = "Falha no 'dotnet publish' self-contained (código " + std::to_string(rc)
                + "). Detalhes:\n" + Tail(logText, 1500);
            return false;
        }

        std::string gameAsm;
        std::string asmName = FindGameAssembly(gameDir, gameAsm);
        if (asmName.empty()) { outError = gameAsm; return false; }

        // Remove o apphost do .NET (<AssemblyName>.exe ou <AssemblyName> sem
        // extensão) — quem roda o jogo é o KizuriGame, não o exe do publish.
        fs::path stem = fs::path(asmName).replace_extension().filename();
        fs::remove(gameDir / stem, pec);
        fs::remove(gameDir / (stem.string() + ".exe"), pec);

        // Copia a engine pro lado do assembly: a resolução do P/Invoke
        // 'KizuriEngine' nunca deve depender de PATH/CWD do jogador.
#ifdef _WIN32
        fs::path engineDll = binDir / "KizuriEngine.dll";
#else
        fs::path engineDll = binDir / "libKizuriEngine.so";
#endif
        if (fs::exists(engineDll, pec) && !CopyFileTo(engineDll, gameDir / engineDll.filename(), outError))
            return false;

        moduleOutPath = fs::path("Game") / asmName;
        KZ_CORE_INFO("Jogo publicado self-contained (runtime .NET embutido): {0}", gameDir.string());
    } else if (!request.GameModulePath.empty()) {
        fs::path modSrc = request.GameModulePath;
        if (!fs::exists(modSrc)) {
            outError = "GameModule não encontrado: " + modSrc.string();
            return false;
        }

        fs::path runtimeConfig = modSrc.parent_path() / (modSrc.stem().string() + ".runtimeconfig.json");
        if (fs::exists(runtimeConfig)) {
            fs::path gameDir = outDir / "Game";
            if (!CopyDirectoryRecursive(modSrc.parent_path(), gameDir, outError)) return false;
            moduleOutPath = fs::path("Game") / modSrc.filename();

            // Garante runtime embutido MESMO no fallback: se a pasta copiada
            // não é self-contained (sem hostfxr), embute o runtime da máquina
            // — todo jogo exportado roda sem o jogador instalar nada.
            std::error_code rec;
            if (!fs::exists(gameDir / kHostfxrName, rec)) {
                KZ_CORE_INFO("Game/ sem runtime .NET — embutindo o runtime local...");
                if (!EmbedDotnetRuntime(gameDir, outError)) return false;
            }
        } else {
            if (!CopyFileTo(modSrc, outDir / modSrc.filename(), outError)) return false;
            moduleOutPath = modSrc.filename();
        }
    }

#ifdef _WIN32
    std::ofstream bat(outDir / "Jogar.bat");
    if (bat.is_open()) {
        bat << "@echo off\n";
        if (moduleOutPath.empty())
            bat << "KizuriGame.exe Start.kzscene\n";
        else
            bat << "KizuriGame.exe Start.kzscene " << moduleOutPath.string() << "\n";
        bat << "pause\n";
    }
#else
    std::ofstream sh(outDir / "jogar.sh");
    if (sh.is_open()) {
        sh << "#!/bin/sh\ncd \"$(dirname \"$0\")\"\n";
        if (moduleOutPath.empty())
            sh << "./KizuriGame Start.kzscene\n";
        else
            sh << "./KizuriGame Start.kzscene " << moduleOutPath.generic_string() << "\n";
    }
#endif

    std::ofstream readme(outDir / "LEIA-ME.txt");
    if (readme.is_open()) {
        readme << "Jogo exportado pela Kizuri Engine\n"
               << "---------------------------------\n"
               << "Rode KizuriGame com Start.kzscene"
               << (moduleOutPath.empty() ? ".\n" : (" e " + moduleOutPath.generic_string() + ".\n"))
               << "No Windows: dê dois cliques em Jogar.bat\n";
    }

    KZ_CORE_INFO("Jogo exportado para: {0}", outDir.string());
    return true;
}

// Diretório do executável atual (editor/KizuriGame) — onde o .NET embutido
// vive (bin/dotnet/) quando a engine é distribuída self-contained.
static fs::path ExeDir() {
#if defined(_WIN32)
    wchar_t buf[MAX_PATH];
    DWORD len = GetModuleFileNameW(nullptr, buf, MAX_PATH);
    if (len == 0) return {};
    return fs::path(buf).parent_path();
#else
    char buf[PATH_MAX];
    ssize_t len = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (len <= 0) return {};
    buf[len] = '\0';
    return fs::path(buf).parent_path();
#endif
}

// CLI do dotnet: prefere o embutido junto ao executável (bin/dotnet/dotnet),
// senão o do PATH. É o que deixa a engine compilar/publicar o jogo C# sem o
// usuário instalar o .NET.
static std::string ResolveDotnetCli() {
    fs::path exeDir = ExeDir();
#if defined(_WIN32)
    fs::path cli = exeDir / "dotnet" / "dotnet.exe";
#else
    fs::path cli = exeDir / "dotnet" / "dotnet";
#endif
    std::error_code ec;
    if (!exeDir.empty() && fs::is_regular_file(cli, ec)) return cli.string();
    return "dotnet";
}

bool GameExporter::BuildGameModule(const std::string& csprojPath,
                                   const std::string& engineRoot,
                                   std::string& outDllPath,
                                   std::string& outError) {
    fs::path csproj(csprojPath);
    if (!fs::is_regular_file(csproj)) {
        outError = "Projeto C# do jogo não encontrado: " + csprojPath;
        return false;
    }

    fs::path logPath = csproj.parent_path() / "build.log";
    std::string cmd = "\"" + ResolveDotnetCli() + "\" build \"" + csproj.string() + "\" -c Debug --nologo -v:m";
    if (!engineRoot.empty())
        cmd += " -p:EngineDir=\"" + engineRoot + "\"";

    KZ_CORE_INFO("Compilando assembly do jogo: {0}", cmd);
    std::string processError;
    int rc = RunAndCapture(cmd, logPath, processError);
    if (rc != 0) {
        std::string logText = processError;
        std::ifstream logIn(logPath);
        if (logIn.is_open()) {
            std::stringstream ss;
            ss << logIn.rdbuf();
            logText = ss.str();
        }
        outError = "Falha ao compilar o jogo (dotnet build, código " + std::to_string(rc) + ").\n"
            + Tail(logText, 1500);
        return false;
    }

    // A .dll do jogo é a que tem um .runtimeconfig.json do lado (a
    // Kizuri.Scripting.dll não tem). Pega a mais recente em <projeto>/bin.
    fs::path binDir = csproj.parent_path() / "bin";
    fs::path best;
    auto bestTime = fs::file_time_type::min();
    std::error_code rec;
    for (auto& entry : fs::recursive_directory_iterator(binDir, rec)) {
        if (!entry.is_regular_file(rec)) continue;
        if (entry.path().extension().string() != ".dll") continue;
        fs::path rcPath = entry.path();
        rcPath.replace_extension(".runtimeconfig.json");
        if (!fs::is_regular_file(rcPath, rec)) continue;
        std::error_code tec;
        auto t = fs::last_write_time(entry.path(), tec);
        if (!tec && t > bestTime) { bestTime = t; best = entry.path(); }
    }
    if (best.empty()) {
        outError = "O build terminou mas não achou a .dll do jogo em " + binDir.string();
        return false;
    }

    outDllPath = best.string();
    KZ_CORE_INFO("Assembly do jogo compilado: {0}", outDllPath);
    return true;
}

} // namespace kizuri
