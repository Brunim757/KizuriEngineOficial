#pragma once
// Ponte de plataforma Android (EGL + android_native_app_glue). Só existe
// quando KZ_PLATFORM_ANDROID: o android_main (platform/android/AndroidEntry.cpp)
// alimenta este estado através de uma fila de eventos; o núcleo da engine
// (Window/Input/Application) consome pela mesma API que usaria GLFW.
//
// As teclas virtuais (kVirtualKeyBase + indice) permitem o jogo mapear
// zonas de toque ou botões de UI pra ações (Input::SetActionKey): o input
// físico (teclado) não existe no Android.
#include <cstdint>
#include <string>

struct ANativeWindow;

namespace kizuri {
namespace AndroidPlatform {

// Quantos toques simultâneos a engine rastreia (multi-touch).
constexpr int kMaxTouches = 4;
// Faixa de "teclas virtuais" (códigos reservados, fora do range GLFW).
constexpr int kVirtualKeyBase = 10000;

struct TouchPoint {
    int Id = -1;
    float X = 0.0f;   // pixels (escala nativa da janela)
    float Y = 0.0f;
    bool Down = false;
};

// ---- estado da janela (alimentado pelo android_main) ----
void SetNativeWindow(ANativeWindow* window);
ANativeWindow* GetNativeWindow();

// Callback quando a ANativeWindow é (re)criada (ex: rotação de tela).
using SurfaceChangedFn = void (*)(void* nativeWindow, void* userData);
void SetSurfaceChangedCallback(SurfaceChangedFn fn, void* userData);

// Hooks do app glue (android_main) injetados pela entrada do jogo: o loop
// da engine precisa drenar o ALooper e saber quando o app foi destruído.
using GluePumpFn = void (*)();
using ShouldExitFn = bool (*)();
void SetGlueHooks(GluePumpFn pump, ShouldExitFn shouldExit);
void PumpGlue();
bool ShouldExit();

// ---- diretórios do app (internalDataPath / externalDataPath) ----
void SetFilesDir(const std::string& path);
const std::string& GetFilesDir();
void SetExternalFilesDir(const std::string& path);
const std::string& GetExternalFilesDir();

// ---- eventos brutos enfileirados pelo android_main ----
void HandleResize(int width, int height);
void HandleTouch(float x, float y, bool down, int id);
void HandleMouseMove(float x, float y);
void HandleKey(int key, bool down, bool repeat);
void HandleAppPause();
void HandleAppResume();

// ---- teclas virtuais (input de toque -> "tecla") ----
void SetVirtualKey(int key, bool down);
bool IsVirtualKeyDown(int key);

// ---- estado de input (lido pelo Input.cpp) ----
int GetTouchCount();
const TouchPoint& GetTouch(int index);
bool IsAnyTouchDown();
float GetLastTouchX();
float GetLastTouchY();

// Processa a fila de eventos pendentes (chamado pelo Window::OnUpdate,
// substitui o glfwPollEvents no Android).
void PollEvents();

// Callback registrado pela janela: recebe eventos já convertidos.
using EventHandler = void (*)(void* userData, uint32_t type, int keyCode, int action,
                              float x, float y);
enum : uint32_t {
    EvWindowResize = 1,
    EvKeyPressed,
    EvKeyReleased,
    EvMouseButtonPressed,
    EvMouseButtonReleased,
    EvMouseMoved,
};
void SetEventHandler(EventHandler handler, void* userData);

} // namespace AndroidPlatform
} // namespace kizuri