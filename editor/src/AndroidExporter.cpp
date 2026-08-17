// AndroidExporter.cpp — export Android LOCAL (a engine compila o APK).
#include "AndroidExporter.hpp"
#include <kizuri/core/Log.hpp>

#include "miniz.h"
#include <nlohmann/json.hpp>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

namespace fs = std::filesystem;

namespace {

bool FileExists(const fs::path& p) { std::error_code ec; return fs::is_regular_file(p, ec); }
bool DirExists(const fs::path& p) { std::error_code ec; return fs::is_directory(p, ec); }

std::string Quote(const std::string& s) { return "\"" + s + "\""; }

// Encontra o primeiro executável no PATH (com ou sem <exe> do Windows).
std::string FindInPath(const std::string& name) {
    const char* pathEnv = getenv("PATH");
    if (!pathEnv) return {};
    std::string paths(pathEnv);
    std::stringstream ss(paths);
    std::string dir;
    while (std::getline(ss, dir, ':')) {
        if (dir.empty()) continue;
        for (const std::string& candidateName : { name, name + ".exe", name + ".bat" }) {
            fs::path cand = fs::path(dir) / candidateName;
            if (FileExists(cand)) return cand.string();
        }
    }
    return {};
}

// Maior versão de build-tools disponível em <sdk>/build-tools/<v>/{aapt2,zipalign}.
std::string FindBuildTools(const std::string& sdkRoot, const char* tool) {
    fs::path bt = fs::path(sdkRoot) / "build-tools";
    std::error_code ec;
    std::string best;
    std::string bestKey;
    if (!DirExists(bt)) return {};
    for (auto& e : fs::directory_iterator(bt, ec)) {
        if (!e.is_directory(ec)) continue;
        std::string v = e.path().filename().string();
        if (FileExists(e.path() / tool)) {
            // compara "34.0.0" por partes numericas
            auto parts = [](const std::string& s) {
                int a = 0, b = 0, c = 0;
                sscanf(s.c_str(), "%d.%d.%d", &a, &b, &c);
                struct { long key; } o; 
                o.key = a * 1000000L + b * 1000L + c;
                return o.key;
            };
            long key = parts(v);
            if (best.empty() || key > parts(bestKey)) { best = (e.path() / tool).string(); bestKey = v; }
        }
    }
    return best;
}

void CopyRecursive(const fs::path& from, const fs::path& to) {
    std::error_code ec;
    fs::create_directories(to, ec);
    for (auto& e : fs::recursive_directory_iterator(from, ec)) {
        if (e.is_directory(ec)) continue;
        auto rel = fs::relative(e.path(), from);
        fs::path dst = to / rel;
        if (dst.has_parent_path()) fs::create_directories(dst.parent_path(), ec);
        fs::copy_file(e.path(), dst, fs::copy_options::overwrite_existing, ec);
    }
}

// Copia runtime pack android-arm64 do cache NuGet (o publish puro não copia
// os .so — mesmíssima regra da CI).
void CopyDotnetRuntimePack(const fs::path& dotnetDir) {
    const char* home = getenv("HOME");
    if (!home) return;
    fs::path packs = fs::path(home) / ".nuget" / "packages" / "microsoft.netcore.app.runtime.android-arm64";
    std::error_code ec;
    if (!DirExists(packs)) return;
    // usa a maior versão instalada
    fs::path best;
    for (auto& e : fs::directory_iterator(packs, ec)) {
        if (!e.is_directory(ec)) continue;
        if (best.empty() || e.path().filename().string() > best.filename().string()) best = e.path();
    }
    if (best.empty()) return;
    fs::path native = best / "runtimes" / "android-arm64" / "native";
    fs::path libs = best / "runtimes" / "android-arm64" / "lib";
    if (DirExists(native)) CopyRecursive(native, dotnetDir);
    if (DirExists(libs)) CopyRecursive(libs, dotnetDir);
}

// Adiciona arquivos (recursivo) num zip existente com miniz (append).
bool ZipAppendDir(mz_zip_archive& zip, const fs::path& base, const fs::path& dir,
                  const std::string& prefix, std::string& err) {
    std::error_code ec;
    for (auto& e : fs::recursive_directory_iterator(dir, ec)) {
        if (e.is_directory(ec)) continue;
        auto rel = fs::relative(e.path(), base);
        std::string entry = prefix + "/" + rel.generic_string();
        // zip exige caminhos com '/'
        for (auto& c : entry) if (c == '\\') c = '/';
        std::ifstream in(e.path(), std::ios::binary);
        if (!in.is_open()) { err = "não foi possível ler " + e.path().string(); return false; }
        std::ostringstream buf;
        buf << in.rdbuf();
        std::string data = buf.str();
        if (!mz_zip_writer_add_mem(&zip, entry.c_str(), data.data(), data.size(),
                                   MZ_BEST_COMPRESSION)) {
            err = "falha ao adicionar " + entry + " no APK";
            return false;
        }
    }
    return true;
}

} // namespace

namespace kizuri {

AndroidExporter::Tools AndroidExporter::DetectTools() {
    Tools t;
    bool ok = true;
    auto fail = [&](const std::string& what) {
        if (!t.Missing.empty()) t.Missing += "; ";
        t.Missing += what;
        ok = false;
    };

    t.Dotnet = FindInPath("dotnet");
    if (t.Dotnet.empty()) fail("dotnet (SDK .NET 10 no PATH)");
    t.Cmake = FindInPath("cmake");
    if (t.Cmake.empty()) fail("cmake");
    t.Ninja = FindInPath("ninja");
    t.HaveNinja = !t.Ninja.empty();

    const char* ndk = getenv("ANDROID_NDK_HOME");
    if (!ndk || !DirExists(ndk)) {
        const char* sdk = getenv("ANDROID_HOME");
        if (sdk && DirExists(sdk)) {
            // acha o NDK r26+/r27 instalado via sdkmanager
            fs::path ndkRoot = fs::path(sdk) / "ndk";
            std::error_code ec;
            for (auto& e : fs::directory_iterator(ndkRoot, ec)) {
                if (e.is_directory(ec)) { ndk = e.path().string().c_str(); break; }
            }
        }
    }
    if (!ndk || !DirExists(ndk)) {
        fail("Android NDK (defina ANDROID_NDK_HOME)");
    } else {
        fs::path tc = fs::path(ndk) / "build" / "cmake" / "android.toolchain.cmake";
        if (FileExists(tc)) t.NdKToolchain = tc.string();
        else t.Missing += (t.Missing.empty() ? "" : "; ") + std::string("android.toolchain.cmake não achado no NDK");
    }

    const char* sdkRoot = getenv("ANDROID_HOME");
    if (!sdkRoot || !DirExists(sdkRoot)) {
        fail("Android SDK (defina ANDROID_HOME — Android Studio instala)");
    } else {
        t.Aapt2 = FindBuildTools(sdkRoot, "aapt2");
        t.Zipalign = FindBuildTools(sdkRoot, "zipalign");
        if (t.Aapt2.empty()) fail("aapt2 (build-tools do SDK)");
        t.Apksigner = FindInPath("apksigner");
        if (t.Apksigner.empty())
            t.Apksigner = FindBuildTools(sdkRoot, "apksigner");
        t.Java = !FindInPath("keytool").empty();
        if (t.Apksigner.empty()) fail("apksigner (build-tools do SDK — instale via sdkmanager 'build-tools;34.0.0')");
    }

    t.Ok = ok;
    return t;
}

bool AndroidExporter::Export(const Tools& tools,
                             const std::string& engineRoot,
                             const std::string& gameCsproj,
                             const std::string& gameContentDir,
                             const std::string& gameName,
                             const std::string& outputDir,
                             std::string& outApkPath,
                             std::string& outError,
                             const std::function<void(const std::string&)>& log) {
    auto info = [&](const std::string& m) { if (log) log(m); KZ_CORE_INFO("{0}", m); };

    fs::path work = fs::path(outputDir) / "android_build";
    fs::path apkBuild = work / "apk";
    fs::path dotnetDir = work / "dotnet";
    std::error_code ec;
    fs::create_directories(apkBuild / "lib" / "arm64-v8a", ec);
    fs::create_directories(dotnetDir, ec);

    const std::string cmake = tools.HaveNinja ? "cmake -S " + Quote(engineRoot) +
        " -B " + Quote((work / "build-android").string()) +
        " -G Ninja -DCMAKE_BUILD_TYPE=Release" +
        " -DCMAKE_TOOLCHAIN_FILE=" + Quote(tools.NdKToolchain) +
        " -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24" +
        " -DKZ_BUILD_EDITOR=OFF -DKZ_BUILD_SANDBOX=OFF -DKZ_BUILD_TESTS=OFF"
        : "cmake -S " + Quote(engineRoot) +
        " -B " + Quote((work / "build-android").string()) +
        " -DCMAKE_BUILD_TYPE=Release" +
        " -DCMAKE_TOOLCHAIN_FILE=" + Quote(tools.NdKToolchain) +
        " -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-24" +
        " -DKZ_BUILD_EDITOR=OFF -DKZ_BUILD_SANDBOX=OFF -DKZ_BUILD_TESTS=OFF";

    info("[1/6] CMake (NDK) — configurando e compilando o jogo p/ Android...");
    int code = std::system(cmake.c_str());
    if (code != 0) { outError = "cmake (configure) falhou (código " + std::to_string(code) + ")."; return false; }
    std::string buildCmd = tools.HaveNinja
        ? "cmake --build " + Quote((work / "build-android").string()) + " --target KizuriGame --parallel -j4"
        : "cmake --build " + Quote((work / "build-android").string()) + " --target KizuriGame --parallel";
    code = std::system(buildCmd.c_str());
    if (code != 0) { outError = "build C++ (NDK) falhou (código " + std::to_string(code) + ")."; return false; }

    fs::path androidBin = work / "build-android" / "bin";
    if (!FileExists(androidBin / "libKizuriGame.so") || !FileExists(androidBin / "libKizuriEngine.so")) {
        outError = "Build Android não gerou libKizuriGame.so/libKizuriEngine.so.";
        return false;
    }

    info("[2/6] dotnet publish (jogo C# → android-arm64, self-contained)...");
    std::string publish = tools.Dotnet + " publish " + Quote(gameCsproj) +
        " -c Release -r android-arm64 --self-contained true -o " + Quote(dotnetDir.string()) + " --nologo";
    code = std::system(publish.c_str());
    if (code != 0) { outError = "dotnet publish falhou (código " + std::to_string(code) + ")."; return false; }

    // Runtime pack (nativos): o publish puro não copia os .so — mesmo da CI.
    CopyDotnetRuntimePack(dotnetDir);
    if (!FileExists(dotnetDir / "libcoreclr.so")) {
        outError = "libcoreclr.so não copiado do runtime pack (rode 'dotnet publish' uma vez p/ baixar o pack).";
        return false;
    }
    info("[3/6] runtime CoreCLR montado (libcoreclr.so + assemblies).");

    // game/ = conteúdo do jogo (cena + assets)
    fs::path gameDir = apkBuild / "assets" / "game";
    if (!gameContentDir.empty() && DirExists(gameContentDir)) {
        CopyRecursive(gameContentDir, gameDir);
    }
    if (!FileExists(gameDir / "Start.kzscene")) {
        // Sem cena inicial: gera uma cena vazia com câmera? Pelo menos não
        // quebra: loga aviso (o jogo fica na tela inicial sem cena).
        info("AVISO: Start.kzscene não encontrada em '" + gameContentDir + "' — cena vazia no APK.");
    }

    // libs
    CopyRecursive(androidBin, apkBuild / "lib" / "arm64-v8a");

    info("[4/6] dotnet assemblies → assets/dotnet...");
    CopyRecursive(dotnetDir, apkBuild / "assets" / "dotnet");

    // ---- aapt2 compile/link ---
    info("[5/6] aapt2 + zip + assinatura...");
    fs::path res = work / "res";
    fs::create_directories(res / "values", ec);
    {
        std::ofstream cfg(res / "compatibility_version.txt");
        cfg << "default\n";
    }
    std::string compileCmd = Quote(tools.Aapt2) + " compile --dir " + Quote(res.string()) + " -o " + Quote((work / "resources.zip").string());
    code = std::system(compileCmd.c_str());
    if (code != 0) { outError = "aapt2 compile falhou."; return false; }

    // manifest: o do repo da engine (platform/android/AndroidManifest.xml)
    fs::path manifestPath = fs::path(engineRoot) / "platform" / "android" / "AndroidManifest.xml";
    if (!FileExists(manifestPath)) {
        // fallback: escreve um manifest mínimo
        manifestPath = work / "AndroidManifest.xml";
        std::ofstream mf(manifestPath);
        mf << R"(<?xml version="1.0" encoding="utf-8"?>
<manifest xmlns:android="http://schemas.android.com/apk/res/android"
    package="br.com.kizuri.game" android:versionCode="1" android:versionName="1.0">
  <uses-feature android:glEsVersion="0x00030000" android:required="true"/>
  <application android:label="Kizuri Game" android:hasCode="false">
    <activity android:name="android.app.NativeActivity" android:exported="true"
        android:configChanges="orientation|screenSize|keyboardHidden">
      <meta-data android:name="android.app.lib_name" android:value="KizuriGame"/>
      <intent-filter>
        <action android:name="android.intent.action.MAIN"/>
        <category android:name="android.intent.category.LAUNCHER"/>
      </intent-filter>
    </activity>
  </application>
</manifest>
)";
    }

    std::string linkCmd = Quote(tools.Aapt2) + " link -o " + Quote((work / "unsigned.apk").string()) +
        " --manifest " + Quote(manifestPath.string()) +
        " --min-sdk-version 24 --target-sdk-version 34" +
        " --version-code 1 --version-name 1.0 " + Quote((work / "resources.zip").string());
    code = std::system(linkCmd.c_str());
    if (code != 0) { outError = "aapt2 link falhou (código " + std::to_string(code) + ")."; return false; }

    // zip (miniz): adiciona lib/ e assets/ no APK do aapt2 (append).
    mz_zip_archive zip;
    memset(&zip, 0, sizeof(zip));
    if (!mz_zip_writer_init_file(&zip, (work / "unsigned.apk").string().c_str(), 0)) {
        outError = "Não foi possível abrir o APK intermediário.";
        return false;
    }
    if (!ZipAppendDir(zip, apkBuild, apkBuild / "lib", "lib", outError) ||
        !ZipAppendDir(zip, apkBuild, apkBuild / "assets", "assets", outError)) {
        mz_zip_writer_end(&zip);
        outError = "Falha ao adicionar libs/assets no APK: " + outError;
        return false;
    }
    mz_zip_writer_finalize_archive(&zip);
    mz_zip_writer_end(&zip);

    // zipalign (escreve em outro arquivo)
    fs::path aligned = work / "aligned.apk";
    std::string zipalignCmd = Quote(tools.Zipalign) + " -f 4 " + Quote((work / "unsigned.apk").string()) + " " + Quote(aligned.string());
    code = std::system(zipalignCmd.c_str());
    if (code != 0) { outError = "zipalign falhou."; return false; }

    // assinatura (keystore própria gerada uma vez; se keytool existir)
    fs::path finalApk = fs::path(outputDir) / (gameName + "-android-arm64.apk");
    if (tools.Java) {
        fs::path key = fs::path(outputDir) / ("kizuri-debug.keystore");
        if (!FileExists(key)) {
            std::string keytool = FindInPath("keytool");
            std::string gen = Quote(keytool) + " -genkeypair -v -keystore " + Quote(key.string()) +
                " -storepass kizuri123 -alias kizuri -keypass kizuri123 -keyalg RSA -keysize 2048 -validity 10000" +
                " -dname \"CN=Kizuri, OU=Kizuri, O=Kizuri, L=Kizuri, S=Kizuri, C=BR\"";
            code = std::system(gen.c_str());
            if (code != 0) { outError = "Falha ao gerar keystore de assinatura (keytool)."; return false; }
        }
        std::string signCmd = Quote(tools.Apksigner) + " sign --ks " + Quote(key.string()) +
            " --ks-pass pass:kizuri123 --key-pass pass:kizuri123 --out " + Quote(finalApk.string()) + " " + Quote(aligned.string());
        code = std::system(signCmd.c_str());
        if (code != 0) { outError = "apksigner falhou (código " + std::to_string(code) + ")."; return false; }
    } else {
        fs::copy_file(aligned, finalApk, fs::copy_options::overwrite_existing, ec);
        outError = "APK NÃO assinado (instale o JDK pra assinar)";
    }

    outApkPath = finalApk.string();
    info("[6/6] APK gerado: " + outApkPath);
    return true;
}

} // namespace kizuri