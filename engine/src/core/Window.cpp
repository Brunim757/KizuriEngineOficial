#include "kizuri/core/Window.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/renderer/Shader.hpp"
#include <glad/gl.h>
#include <GLFW/glfw3.h>
#include <cstdlib>

#if defined(_WIN32)
    #define WIN32_LEAN_AND_MEAN
    #include <Windows.h>
#endif

namespace kizuri {

// Mostra um popup nativo do sistema operacional. Necessário porque em
// ambientes sem console visível (ex: emuladores como Winlator, ou o app
// aberto por duplo-clique sem terminal) uma mensagem de log não é vista
// por ninguém — o usuário só vê "nada abriu".
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

Window::Window(const WindowProps& props) {
    Init(props);
}

Window::~Window() {
    Shutdown();
}

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

    // Tenta a versão mais alta primeiro e desce até 3.3 core (mínimo). A
    // cadeia inclui 4.6/4.5/4.3/4.1/4.0 — assim uma máquina que só chega a
    // 4.0 (ou 4.3, etc) usa a versão MÁXIMA dela, não cai pro 3.3 à toa.
    // Os shaders escalam via GetGLSLVersion (GL_SHADING_LANGUAGE_VERSION).
    const int contextVersions[][2] = {
        {4, 6}, {4, 5}, {4, 3}, {4, 1}, {4, 0}, {3, 3}
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

} // namespace kizuri
