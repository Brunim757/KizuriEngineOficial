#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/Event.hpp"
#include <string>
#include <vector>

#if defined(KZ_PLATFORM_ANDROID)
struct ANativeWindow;
#endif
struct GLFWwindow;

namespace kizuri {

struct WindowProps {
    std::string Title = "Kizuri Engine";
    uint32_t Width = 1600;
    uint32_t Height = 900;
    bool VSync = true;
    bool Maximized = false;
    // Quando true, a janela é criada sem a decoração nativa do SO
    // (sem barra de título/botões do sistema) para que o app desenhe sua
    // própria barra de título via ImGui. Ver Window::BeginTitlebarDrag,
    // Window::ToggleMaximize etc. Continua redimensionável e movível —
    // essas operações só deixam de ser feitas pelo SO e passam a ser
    // feitas manualmente (ver EditorLayer::DrawTitlebar).
    bool CustomTitlebar = false;
};

// Janela nativa multiplataforma. Desktop: GLFW + OpenGL 3.3 core. Android
// (KZ_PLATFORM_ANDROID): EGL + GLES 3.x direto via android_native_app_glue
// — ver Window.cpp (a API pública é a mesma pros dois backends).
class Window {
public:
    explicit Window(const WindowProps& props = WindowProps());
    ~Window();

    void OnUpdate();
    void SetEventCallback(const EventCallbackFn& cb) { m_Data.EventCallback = cb; }

    uint32_t GetWidth() const  { return m_Data.Width; }
    uint32_t GetHeight() const { return m_Data.Height; }

    void SetVSync(bool enabled);
    bool IsVSync() const { return m_Data.VSync; }

    // Arquivos soltos pelo sistema na janela (Explorer) — editor consome e limpa.
    std::vector<std::string>& GetDroppedFiles() { return m_Data.DroppedFiles; }

    GLFWwindow* GetNativeWindow() const { return m_Window; }

    // Versão real do contexto OpenGL que acabou sendo criado (Init() tenta
    // 4.5 -> 4.1 -> 3.3 core e fica com a primeira que a GPU/driver aceitar
    // — ver Window::Init). Existe pra quem monta código GLSL em tempo de
    // execução (hoje só o ImGuiLayer) poder casar a diretiva "#version"
    // com o contexto que a máquina do usuário de fato conseguiu, em vez de
    // arriscar pedir uma versão de GLSL mais nova do que o contexto
    // suporta — isso compila por sorte em drivers tolerantes (ex: Mesa
    // zink) e falha silenciosamente ou trava em drivers rígidos (NVIDIA/
    // AMD/Intel oficiais), que rejeitam GLSL acima da versão do contexto.
    int GetGLVersionMajor() const { return m_GLVersionMajor; }
    int GetGLVersionMinor() const { return m_GLVersionMinor; }

    // ---- Controle de janela para barra de título customizada ----
    // Só fazem sentido quando WindowProps::CustomTitlebar == true, já que
    // com decoração nativa o próprio SO cuida de mover/redimensionar.
    void GetPosition(int& x, int& y) const;
    void SetPosition(int x, int y);
    void SetSize(int width, int height);
    void Minimize();
    void ToggleMaximize();
    bool IsMaximized() const;

private:
    void Init(const WindowProps& props);
    void Shutdown();

#if defined(KZ_PLATFORM_ANDROID)
    void* m_EGLDisplay = nullptr;
    void* m_EGLSurface = nullptr;
    void* m_EGLContext = nullptr;
    void* m_EGLConfig = nullptr;
    bool m_SurfaceValid = false;
    void HandleAndroidSurfaceChanged(void* nativeWindow);
    void DestroyAndroidEGLSurface();
#endif
    GLFWwindow* m_Window = nullptr;
    int m_GLVersionMajor = 0;
    int m_GLVersionMinor = 0;

    struct WindowData {
        std::string Title;
        uint32_t Width = 0, Height = 0;
        bool VSync = true;
        EventCallbackFn EventCallback;
        // Arquivos soltos pelo SISTEMA na janela (glfwSetDropCallback) —
        // consumidos/limpos pelo consumidor (editor) a cada frame.
        std::vector<std::string> DroppedFiles;
    };
    WindowData m_Data;
};

} // namespace kizuri
