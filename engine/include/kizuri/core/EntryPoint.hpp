#pragma once
#include "kizuri/core/Application.hpp"
#include "kizuri/core/CommandLineArgs.hpp"
#include "kizuri/core/Version.hpp"
#include "kizuri/core/Log.hpp"
#include <exception>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif




extern kizuri::Application* kizuri::CreateApplication();

static void KzShowUnhandledExceptionPopup(const char* what) {
#if defined(_WIN32)
    std::string msg = std::string("A Kizuri Engine encerrou por um erro inesperado:\n\n") + what +
        "\n\nSe estiver rodando em um emulador (ex: Winlator) ou máquina virtual, "
        "verifique se o dispositivo suporta OpenGL 3.3+ com aceleração de GPU.";
    
    
    auto toWide = [](const std::string& utf8) -> std::wstring {
        if (utf8.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring wide(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), len);
        return wide;
    };
    MessageBoxW(nullptr, toWide(msg).c_str(), L"Kizuri Engine — Erro Inesperado", MB_OK | MB_ICONERROR);
#else
    (void)what;
#endif
}

int main(int argc, char** argv) {
    for (int i = 0; i < argc; ++i) kizuri::GetCommandLineArgs().emplace_back(argv[i]);
    kizuri::Log::Init();
    KZ_CORE_INFO("=================================================");
    KZ_CORE_INFO("            KIZURI ENGINE v{0}                 ", KIZURI_VERSION);
    KZ_CORE_INFO("=================================================");

    try {
        auto* app = kizuri::CreateApplication();
        app->Run();
        delete app;
    } catch (const std::exception& e) {
        KZ_CORE_CRITICAL("Exceção não tratada: {0}", e.what());
        KzShowUnhandledExceptionPopup(e.what());
        return 1;
    } catch (...) {
        KZ_CORE_CRITICAL("Exceção desconhecida não tratada.");
        KzShowUnhandledExceptionPopup("Erro desconhecido.");
        return 1;
    }
    return 0;
}
