#include "kizuri/core/Window.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/renderer/Shader.hpp"
#include "kizuri/core/WindowIcon.hpp"
#include <glad/gl.h>
#if defined(KZ_PLATFORM_ANDROID)
    #include "kizuri/core/AndroidPlatform.hpp"
    #include <EGL/egl.h>
    #include <android/native_window.h>
#else
    #include <GLFW/glfw3.h>
#endif
#include <cstdlib>
#include <new>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace kizuri {

#if defined(KZ_PLATFORM_ANDROID)
// Converte os eventos brutos do android_main (AndroidPlatform) nos eventos
// Kizuri equivalentes (KeyPressed/MouseButton/Resize), igual GLFW faria.
void Window::Init(const WindowProps& props) {
    m_Data.Title = props.Title;

    // ---- registra a ponte de eventos (android_main alimenta a fila) ----
    AndroidPlatform::SetEventHandler([](void* userData, uint32_t type, int keyCode, int action,
                                        float x, float y) {
        auto& data = *(WindowData*)userData;
        switch (type) {
            case AndroidPlatform::EvWindowResize: {
                WindowResizeEvent event(keyCode, action);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
            case AndroidPlatform::EvKeyPressed: {
                KeyPressedEvent event(keyCode, action != 0);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
            case AndroidPlatform::EvKeyReleased: {
                KeyReleasedEvent event(keyCode);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
            case AndroidPlatform::EvMouseButtonPressed: {
                MouseButtonPressedEvent event(keyCode);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
            case AndroidPlatform::EvMouseButtonReleased: {
                MouseButtonReleasedEvent event(keyCode);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
            case AndroidPlatform::EvMouseMoved: {
                MouseMovedEvent event(x, y);
                if (data.EventCallback) data.EventCallback(event);
                break;
            }
        }
    }, &m_Data);

    ANativeWindow* nativeWindow = AndroidPlatform::GetNativeWindow();
    if (!nativeWindow) {
        KZ_CORE_CRITICAL("FALHA FATAL: nenhuma ANativeWindow disponível (APP_CMD_INIT_WINDOW não chegou).");
        std::exit(1);
    }

    int winW = ANativeWindow_getWidth(nativeWindow);
    int winH = ANativeWindow_getHeight(nativeWindow);
    if (winW <= 0 || winH <= 0) {
        KZ_CORE_WARN("ANativeWindow com tamanho inválido ({0}x{1}); usando spec.", winW, winH);
        winW = (int)props.Width;
        winH = (int)props.Height;
    }

    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    KZ_ASSERT(display != EGL_NO_DISPLAY, "eglGetDisplay falhou!");
    if (!eglInitialize(display, nullptr, nullptr)) {
        KZ_CORE_CRITICAL("FALHA FATAL: eglInitialize falhou (código 0x{0:x}).", (unsigned)eglGetError());
        std::exit(1);
    }

    // Config ES 3.x: RGBA8 + depth24 + MSAA 4. Cai pra ES 2 se o driver só
    // tiver isso (a engine não é testada em ES2; melhor telas com aviso).
    EGLint configAttribs[] = {
        EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_DEPTH_SIZE, 24,
        EGL_SAMPLE_BUFFERS, 1, EGL_SAMPLES, 4,
        EGL_NONE
    };
    EGLint configCount = 0;
    EGLConfig config = nullptr;
    eglChooseConfig(display, configAttribs, &config, 1, &configCount);
    if (configCount == 0) {
        // Sem MSAA no driver: tenta sem antialias.
        EGLint fallbackAttribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 24,
            EGL_NONE
        };
        eglChooseConfig(display, fallbackAttribs, &config, 1, &configCount);
        KZ_CORE_WARN("MSAA 4 não disponível no dispositivo; usando sem MSAA.");
    }
    if (configCount == 0) {
        KZ_CORE_CRITICAL("FALHA FATAL: nenhum EGLConfig compatível (EGL 0x{0:x}).", (unsigned)eglGetError());
        std::exit(1);
    }

    EGLint contextAttribs[] = { EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE };
    EGLContext context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
    if (context == EGL_NO_CONTEXT) {
        KZ_CORE_WARN("Contexto GLES 3.0 falhou (EGL 0x{0:x}); tentando ES 2...", (unsigned)eglGetError());
        EGLint es2Attribs[] = { EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE };
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, es2Attribs);
        if (context == EGL_NO_CONTEXT) {
            KZ_CORE_CRITICAL("FALHA FATAL: não foi possível criar contexto GLES (EGL 0x{0:x}).", (unsigned)eglGetError());
            std::exit(1);
        }
    }

    EGLSurface surface = eglCreateWindowSurface(display, config, nativeWindow, nullptr);
    if (surface == EGL_NO_SURFACE) {
        KZ_CORE_CRITICAL("FALHA FATAL: eglCreateWindowSurface falhou (EGL 0x{0:x}).", (unsigned)eglGetError());
        std::exit(1);
    }

    if (!eglMakeCurrent(display, surface, surface, context)) {
        KZ_CORE_CRITICAL("FALHA FATAL: eglMakeCurrent falhou (EGL 0x{0:x}).", (unsigned)eglGetError());
        std::exit(1);
    }

    m_EGLDisplay = display;
    m_EGLSurface = surface;
    m_EGLContext = context;
    m_EGLConfig = config;
    m_SurfaceValid = true;

    m_Data.Width = (uint32_t)winW;
    m_Data.Height = (uint32_t)winH;
    m_GLVersionMajor = 3;
    m_GLVersionMinor = 0;

    // Tetos: GLSL ES 300 (a transformação #version em Shader.cpp cuida do
    // resto — "330 core" vira "300 es" + precision).
    kizuri::SetContextGLSLVersion(300);

    int gladOk = gladLoadGL((GLADloadfunc)eglGetProcAddress);
    if (!gladOk) {
        KZ_CORE_CRITICAL("FALHA FATAL: glad não conseguiu carregar as funções GLES.");
        std::exit(1);
    }

    KZ_CORE_INFO("GLES: {0}", (const char*)glGetString(GL_VERSION));
    KZ_CORE_INFO("GLES renderer: {0}", (const char*)glGetString(GL_RENDERER));
    KZ_CORE_INFO("Janela Android criada: {0}x{1}.", winW, winH);

    SetVSync(props.VSync);
    AndroidPlatform::HandleResize(winW, winH);
    AndroidPlatform::SetSurfaceChangedCallback([](void* nativeWindow, void* userData) {
        static_cast<Window*>(userData)->HandleAndroidSurfaceChanged(nativeWindow);
    }, this);
}

void Window::DestroyAndroidEGLSurface() {
    if (!m_SurfaceValid) return;
    eglMakeCurrent((EGLDisplay)m_EGLDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroySurface((EGLDisplay)m_EGLDisplay, (EGLSurface)m_EGLSurface);
    m_EGLSurface = nullptr;
    m_SurfaceValid = false;
}

void Window::HandleAndroidSurfaceChanged(void* nativeWindow) {
    ANativeWindow* window = (ANativeWindow*)nativeWindow;
    if (!window) {
        // Tela destruída (app em background / rotação): derruba a superfície.
        DestroyAndroidEGLSurface();
        return;
    }
    if (!m_EGLDisplay || !m_EGLContext) {
        // A janela ainda não foi criada (Init roda depois do primeiro
        // INIT_WINDOW) — nada a fazer; o Init cuida da superfície.
        return;
    }
    // (Re)cria a superfície EGL sobre a ANativeWindow nova.
    DestroyAndroidEGLSurface();
    EGLSurface surface = eglCreateWindowSurface((EGLDisplay)m_EGLDisplay, (EGLConfig)m_EGLConfig,
                                                window, nullptr);
    if (surface == EGL_NO_SURFACE) {
        KZ_CORE_ERROR("eglCreateWindowSurface falhou ao recriar (EGL 0x{0:x}).", (unsigned)eglGetError());
        return;
    }
    m_EGLSurface = surface;
    if (!eglMakeCurrent((EGLDisplay)m_EGLDisplay, surface, surface, (EGLContext)m_EGLContext)) {
        KZ_CORE_ERROR("eglMakeCurrent falhou ao recriar superfície (0x{0:x}).", (unsigned)eglGetError());
        eglDestroySurface((EGLDisplay)m_EGLDisplay, surface);
        return;
    }
    m_SurfaceValid = true;

    EGLint w = 0, h = 0;
    eglQuerySurface((EGLDisplay)m_EGLDisplay, surface, EGL_WIDTH, &w);
    eglQuerySurface((EGLDisplay)m_EGLDisplay, surface, EGL_HEIGHT, &h);
    if (w > 0 && h > 0 && ((uint32_t)w != m_Data.Width || (uint32_t)h != m_Data.Height)) {
        m_Data.Width = (uint32_t)w;
        m_Data.Height = (uint32_t)h;
        AndroidPlatform::HandleResize(w, h);
    }
    SetVSync(m_Data.VSync);
    KZ_CORE_INFO("Superfície EGL recriada: {0}x{1}.", w, h);
}

void Window::Shutdown() {
    if (!m_SurfaceValid && !m_EGLDisplay) return;
    if (m_SurfaceValid) DestroyAndroidEGLSurface();
    if (m_EGLContext) {
        eglDestroyContext((EGLDisplay)m_EGLDisplay, (EGLContext)m_EGLContext);
        m_EGLContext = nullptr;
    }
    if (m_EGLDisplay) {
        eglTerminate((EGLDisplay)m_EGLDisplay);
        m_EGLDisplay = nullptr;
    }
}

void Window::OnUpdate() {
    AndroidPlatform::PollEvents();
    if (m_SurfaceValid)
        eglSwapBuffers((EGLDisplay)m_EGLDisplay, (EGLSurface)m_EGLSurface);
}

void Window::SetVSync(bool enabled) {
    if (m_SurfaceValid)
        eglSwapInterval((EGLDisplay)m_EGLDisplay, enabled ? 1 : 0);
    m_Data.VSync = enabled;
}

void Window::GetPosition(int& x, int& y) const { x = 0; y = 0; }
void Window::SetPosition(int, int) {}
void Window::SetSize(int width, int height) {
    m_Data.Width = (uint32_t)width;
    m_Data.Height = (uint32_t)height;
}
void Window::Minimize() {}
void Window::ToggleMaximize() {}
bool Window::IsMaximized() const { return false; }

#else
#endif

// Mostra um popup nativo do sistema operacional. Necessário porque em
// ambientes sem console visível (ex: emuladores como Winlator, ou o app
// aberto por duplo-clique sem terminal) uma mensagem de log não é vista
// por ninguém — o usuário só vê "nada abriu".
#if !defined(KZ_PLATFORM_ANDROID)
static void ShowFatalErrorPopup(const std::string& title, const std::string& message) {
#if defined(_WIN32)
    // MessageBoxA interpreta os bytes pela ANSI codepage do sistema (ex: CP-1252), não como
    // UTF-8 — como as strings no código-fonte SÃO UTF-8 (acentos, travessão), isso produzia
    // "NÃ£o foi possÃ-vel..." em vez de "Não foi possível...". MessageBoxW não tem esse
    // problema: precisa só converter UTF-8 -> UTF-16 antes.
    auto toWide = [](const std::string& utf8) -> std::wstring {
        if (utf8.empty()) return std::wstring();
        int len = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
        std::wstring wide(len, L'\0');
        MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), wide.data(), len);
        return wide;
    };
    MessageBoxW(nullptr, toWide(message).c_str(), toWide(title).c_str(), MB_OK | MB_ICONERROR);
#else
    (void)title; (void)message;
#endif
}

static uint8_t s_GLFWWindowCount = 0;

static void GLFWErrorCallback(int error, const char* description) {
    KZ_CORE_ERROR("Erro do GLFW ({0}): {1}", error, description);
}
#endif // !KZ_PLATFORM_ANDROID

Window::Window(const WindowProps& props) {
    Init(props);
}

Window::~Window() {
    Shutdown();
}

// ===========================================================================
// Backend desktop: GLFW + OpenGL 3.3 core (Win32/Linux/macOS). No Android a
// implementação está no bloco #else do topo (EGL direto) — ver acima.
// ===========================================================================
#if !defined(KZ_PLATFORM_ANDROID)
void Window::Init(const WindowProps& props) {
    m_Data.Title = props.Title;
    m_Data.Width = props.Width;
    m_Data.Height = props.Height;
    m_Data.VSync = props.VSync;

    KZ_CORE_INFO("Criando a janela '{0}' ({1}x{2}).", props.Title, props.Width, props.Height);

    if (s_GLFWWindowCount == 0) {
        [[maybe_unused]] int success = glfwInit();
        KZ_ASSERT(success, "Falha ao inicializar GLFW!");
        glfwSetErrorCallback(GLFWErrorCallback);
    }

    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_DECORATED, props.CustomTitlebar ? GLFW_FALSE : GLFW_TRUE);
    if (props.Maximized) glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);
#ifdef KZ_DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    // Responsividade: uma janela maior que a área de trabalho do monitor
    // primário fica cortada/sem acesso às bordas ("a engine só funciona em
    // resolução grande"). Limita o tamanho pedido ao work area e centraliza.
    int winW = (int)props.Width, winH = (int)props.Height;
    {
        GLFWmonitor* monitor = glfwGetPrimaryMonitor();
        if (monitor) {
            int waX = 0, waY = 0, waW = 0, waH = 0;
            glfwGetMonitorWorkarea(monitor, &waX, &waY, &waW, &waH);
            if (waW > 0 && waH > 0) {
                if (winW > waW || winH > waH) {
                    winW = (winW > waW) ? waW : winW;
                    winH = (winH > waH) ? waH : winH;
                    if (winW < 800) winW = 800;
                    if (winH < 600) winH = 600;
                }
                glfwWindowHint(GLFW_POSITION_X, waX + (waW - winW) / 2);
                glfwWindowHint(GLFW_POSITION_Y, waY + (waH - winH) / 2);
            }
        }
    }
    m_Data.Width = (uint32_t)winW;
    m_Data.Height = (uint32_t)winH;

    // A engine roda SEMPRE em OpenGL 3.3 core: é o caminho comprovado e
    // estável em QUALQUER máquina (Wine, iGPU, VM, driver novo) — sem a saga
    // de viewport preto de drivers que anunciam GLSL 4.x e rejeitam os
    // shaders. Não há mais features de outras versões no código (PCSS, sombra
    // de luz pontual e SSR foram REMOVIDOS) — mesmo num PC com OpenGL 4.5/4.6
    // o contexto pedido é só 3.3 e os shaders são #version 330 core.
    const int contextVersions[][2] = {
        {3, 3}
    };
    for (auto& version : contextVersions) {
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, version[0]);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, version[1]);
        m_Window = glfwCreateWindow(winW, winH, m_Data.Title.c_str(), nullptr, nullptr);
        if (m_Window) {
            m_GLVersionMajor = version[0];
            m_GLVersionMinor = version[1];
            // Trava a GLSL no teto da versão pedida (3.3 -> 330, 4.0 -> 400,
            // 4.5 -> 450, ...). Alguns drivers reportam GLSL maior que o
            // contexto real; compilar acima da GL quebra os shaders.
            kizuri::SetContextGLSLVersion(version[0] == 3 ? 330 : version[0] * 100 + version[1] * 10);
            KZ_CORE_INFO("Contexto OpenGL {0}.{1} core criado com sucesso.", version[0], version[1]);
            break;
        }
        KZ_CORE_WARN("Falha ao criar o contexto OpenGL {0}.{1} core; tentando uma versão anterior...", version[0], version[1]);
    }

    if (m_Window) {
        // Ícone da janela (torii da marca): embutido no binário — o ícone do
        // sisteminha (taskbar/alt-tab) funciona até sem arquivo externo.
        GLFWimage icon;
        icon.width = kizuri::windowicon::kWidth;
        icon.height = kizuri::windowicon::kHeight;
        icon.pixels = (unsigned char*)kizuri::windowicon::kPixels;
        glfwSetWindowIcon(m_Window, 1, &icon);
    }

    if (!m_Window) {
        KZ_CORE_CRITICAL("=================================================================");
        KZ_CORE_CRITICAL(" FALHA FATAL: não foi possível criar a janela/contexto OpenGL.");
        KZ_CORE_CRITICAL(" Verifique se sua GPU/driver de vídeo suporta OpenGL 3.3+ e se");
        KZ_CORE_CRITICAL(" você não está rodando via um ambiente remoto sem aceleração 3D.");
        KZ_CORE_CRITICAL("=================================================================");
        ShowFatalErrorPopup(
            "Kizuri Engine — Erro Fatal",
            "Não foi possível criar um contexto OpenGL 3.3+ (core profile).\n\n"
            "Isso normalmente acontece quando o dispositivo/emulador (ex: Winlator, "
            "máquina virtual, área de trabalho remota) só oferece OpenGL 2.x ou não "
            "expõe aceleração de GPU adequada.\n\n"
            "A Kizuri Engine exige, no mínimo, OpenGL 3.3 com perfil core.\n"
            "Teste em um PC com GPU/driver de vídeo atualizado."
        );
        --s_GLFWWindowCount;
        if (s_GLFWWindowCount == 0) glfwTerminate();
        std::exit(1);
    }
    ++s_GLFWWindowCount;

    glfwMakeContextCurrent(m_Window);
    int gladOk = gladLoadGL((GLADloadfunc)glfwGetProcAddress);
    if (!gladOk) {
        KZ_CORE_CRITICAL("=================================================================");
        KZ_CORE_CRITICAL(" FALHA FATAL: não foi possível carregar as funções OpenGL (glad).");
        KZ_CORE_CRITICAL(" O driver de vídeo pode não expor as extensões necessárias.");
        KZ_CORE_CRITICAL("=================================================================");
        ShowFatalErrorPopup(
            "Kizuri Engine — Erro Fatal",
            "O contexto OpenGL foi criado, mas não foi possível carregar as funções "
            "necessárias (glad).\n\nO driver de vídeo do dispositivo pode não expor "
            "as extensões OpenGL 3.3+ exigidas pela engine."
        );
        std::exit(1);
    }

    KZ_CORE_INFO("Fabricante OpenGL: {0}", (const char*)glGetString(0x1F00));
    KZ_CORE_INFO("Renderer OpenGL: {0}", (const char*)glGetString(0x1F01));
    KZ_CORE_INFO("Versão do OpenGL: {0}", (const char*)glGetString(0x1F02));

    glfwSetWindowUserPointer(m_Window, &m_Data);
    SetVSync(props.VSync);

    // ---- callbacks GLFW -> eventos Kizuri ----
    glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* w, int width, int height) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        data.Width = width; data.Height = height;
        WindowResizeEvent event(width, height);
        if (data.EventCallback) data.EventCallback(event);
    });

    glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* w) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        WindowCloseEvent event;
        if (data.EventCallback) data.EventCallback(event);
    });

    glfwSetKeyCallback(m_Window, [](GLFWwindow* w, int key, int, int action, int) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        switch (action) {
            case GLFW_PRESS: { KeyPressedEvent e(key, false); if (data.EventCallback) data.EventCallback(e); break; }
            case GLFW_RELEASE: { KeyReleasedEvent e(key); if (data.EventCallback) data.EventCallback(e); break; }
            case GLFW_REPEAT: { KeyPressedEvent e(key, true); if (data.EventCallback) data.EventCallback(e); break; }
        }
    });

    glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* w, int button, int action, int) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        switch (action) {
            case GLFW_PRESS: { MouseButtonPressedEvent e(button); if (data.EventCallback) data.EventCallback(e); break; }
            case GLFW_RELEASE: { MouseButtonReleasedEvent e(button); if (data.EventCallback) data.EventCallback(e); break; }
        }
    });

    glfwSetScrollCallback(m_Window, [](GLFWwindow* w, double xOff, double yOff) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        MouseScrolledEvent e((float)xOff, (float)yOff);
        if (data.EventCallback) data.EventCallback(e);
    });

    glfwSetCursorPosCallback(m_Window, [](GLFWwindow* w, double x, double y) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        MouseMovedEvent e((float)x, (float)y);
        if (data.EventCallback) data.EventCallback(e);
    });

    // Drop de arquivos do SISTEMA (Explorer/gerenciador) na janela: guarda
    // os caminhos — o editor consome depois no OnUpdate (cria entidades na
    // posição do mouse). É o que permite arrastar um .glb/.png do Windows
    // direto pra cena, além do arrasto interno do Content Browser.
    glfwSetDropCallback(m_Window, [](GLFWwindow* w, int count, const char** paths) {
        auto& data = *(WindowData*)glfwGetWindowUserPointer(w);
        if (!paths || count <= 0) return;
        // Fila limitada a 64 caminhos — o editor drena no OnUpdate; se o
        // usuário soltar 10.000 arquivos, não cresce sem limite (memória).
        constexpr size_t kMaxPendingDrops = 64;
        for (int i = 0; i < count; ++i) {
            if (!paths[i]) continue;
            if (data.DroppedFiles.size() >= kMaxPendingDrops) {
                KZ_CORE_WARN("Fila de drops cheia ({0}); ignorando o restante.", kMaxPendingDrops);
                break;
            }
            try { data.DroppedFiles.emplace_back(paths[i]); }
            catch (const std::bad_alloc&) {
                KZ_CORE_ERROR("Sem memória pra armazenar o drop; ignorado.");
                break;
            }
        }
        KZ_CORE_INFO("Arquivo(s) solto(s) na janela: {0}", count);
    });
}

void Window::Shutdown() {
    if (!m_Window) return;
    glfwDestroyWindow(m_Window);
    --s_GLFWWindowCount;
    if (s_GLFWWindowCount == 0) glfwTerminate();
}

void Window::OnUpdate() {
    KZ_CORE_TRACE("Window::OnUpdate — glfwPollEvents");
    glfwPollEvents();
    KZ_CORE_TRACE("Window::OnUpdate — glfwSwapBuffers");
    glfwSwapBuffers(m_Window);
    KZ_CORE_TRACE("Window::OnUpdate — fim");
}

void Window::SetVSync(bool enabled) {
    glfwSwapInterval(enabled ? 1 : 0);
    m_Data.VSync = enabled;
}

void Window::GetPosition(int& x, int& y) const {
    glfwGetWindowPos(m_Window, &x, &y);
}

void Window::SetPosition(int x, int y) {
    glfwSetWindowPos(m_Window, x, y);
}

void Window::SetSize(int width, int height) {
    glfwSetWindowSize(m_Window, width, height);
}

void Window::Minimize() {
    glfwIconifyWindow(m_Window);
}

void Window::ToggleMaximize() {
    if (IsMaximized())
        glfwRestoreWindow(m_Window);
    else
        glfwMaximizeWindow(m_Window);
}

bool Window::IsMaximized() const {
    return glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED) != 0;
}

#endif // !KZ_PLATFORM_ANDROID

} // namespace kizuri
