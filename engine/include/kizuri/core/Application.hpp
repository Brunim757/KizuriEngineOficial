#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/Window.hpp"
#include "kizuri/core/Layer.hpp"
#include "kizuri/core/Event.hpp"

namespace kizuri {

struct ApplicationSpec {
    std::string Name = "Aplicativo Kizuri";
    uint32_t Width = 1600;
    uint32_t Height = 900;
    bool VSync = true;
    bool CustomTitlebar = false;
};

class Application {
public:
    explicit Application(const ApplicationSpec& spec = ApplicationSpec());
    virtual ~Application();

    void Run();
    void Close();

    void OnEvent(Event& e);

    void PushLayer(Layer* layer);
    void PushOverlay(Layer* overlay);

    Window& GetWindow() { return *m_Window; }
    class ImGuiLayer* GetImGuiLayer() { return m_ImGuiLayer; }

    static Application& Get();

private:
    bool OnWindowClose(WindowCloseEvent& e);
    bool OnWindowResize(WindowResizeEvent& e);

    Scope<Window> m_Window;
    LayerStack m_LayerStack;
    bool m_Running = true;
    bool m_Minimized = false;
    float m_LastFrameTime = 0.0f;

    class ImGuiLayer* m_ImGuiLayer = nullptr;

    static Application* s_Instance;
};

Application* CreateApplication();

}
