#pragma once
// AndroidExporter.hpp — a engine COMPILA o APK sozinha (planeta do usuário),
// como Unity/Godot: exige Android SDK + NDK + .NET instalados (envs
// ANDROID_HOME / ANDROID_NDK_HOME / dotnet no PATH). Pipeline local
// idêntico ao da CI: cmake(NDK) -> dotnet publish android-arm64 + runtime
// pack -> aapt2 -> miniz(zip) -> zipalign -> assinatura (keystore própria).
#include <functional>
#include <string>

namespace kizuri {

class AndroidExporter {
public:
    struct Tools {
        bool Ok = false;
        std::string Missing;              // o que falta, em pt-BR
        std::string NdKToolchain;         // .../build/cmake/android.toolchain.cmake
        std::string Aapt2, Zipalign;      // build-tools/<v>/aapt2 (zipalign)
        std::string Apksigner;            // assinatura (se java presente)
        bool Java = false;
        std::string Cmake, Ninja, Dotnet; // comandos
        bool HaveNinja = true;
    };

    // Detecta SDK/NDK/dotnet/cmake. Preenche Missing quando não encontrar.
    static Tools DetectTools();

    // Compila TUDO e grava <outputDir>/<gameName>-android-arm64.apk.
    // gameContentDir = pasta copiada pro "game/" do APK (cena + assets).
    // log: callback de progresso (mensagens vão pro console do editor).
    static bool Export(const Tools& tools,
                       const std::string& engineRoot,   // checkout da engine
                       const std::string& gameCsproj,    // Source/*.csproj do jogo
                       const std::string& gameContentDir,
                       const std::string& gameName,
                       const std::string& outputDir,
                       std::string& outApkPath,
                       std::string& outError,
                       const std::function<void(const std::string&)>& log = nullptr);
};

} // namespace kizuri