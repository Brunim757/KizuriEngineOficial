#include "kizuri/project/GameExporter.hpp"
#include "kizuri/core/Log.hpp"

#include <nlohmann/json.hpp>
#include <filesystem>
#include <fstream>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include <cstring>

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

    // Módulo do jogo: C# (assembly .NET — copia a pasta toda pra Game/) ou
    // módulo C++ legado (dll/so única na raiz).
    fs::path moduleOutPath;
    if (!request.GameModulePath.empty()) {
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

} // namespace kizuri
