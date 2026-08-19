#include "kizuri/core/Application.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/Input.hpp"
#if defined(KZ_PLATFORM_ANDROID)
    #include "kizuri/core/AndroidPlatform.hpp"
    #include <chrono>
#else
    #include <GLFW/glfw3.h>
#endif
#include "kizuri/renderer/Renderer.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/assets/AssetManager.hpp"

#if !defined(KZ_PLATFORM_ANDROID)
    #include "kizuri/core/ImGuiLayer.hpp"
#endif

namespace kizuri {













Application* Application::s_Instance = nullptr;

Application& Application::Get() { return *s_Instance; }

Application::Application(const ApplicationSpec& spec) {
    KZ_ASSERT(!s_Instance, "Application já existe!");
    s_Instance = this;

    WindowProps props;
    props.Title = spec.Name;
    props.Width = spec.Width;
    props.Height = spec.Height;
    props.VSync = spec.VSync;
    props.CustomTitlebar = spec.CustomTitlebar;

    m_Window = CreateScope<Window>(props);
    m_Window->SetEventCallback([this](Event& e) { OnEvent(e); });
    Input::SetContext(m_Window->GetNativeWindow());

    Renderer::Init();
    AudioEngine::Init();

#if !defined(KZ_PLATFORM_ANDROID)
    
    m_ImGuiLayer = new ImGuiLayer();
    PushOverlay(m_ImGuiLayer);
#endif
}

Application::~Application() {
    AudioEngine::Shutdown();
    Renderer::Shutdown();
}

void Application::PushLayer(Layer* layer)     { m_LayerStack.PushLayer(layer); }
void Application::PushOverlay(Layer* overlay) { m_LayerStack.PushOverlay(overlay); }

void Application::Close() { m_Running = false; }

void Application::OnEvent(Event& e) {
    EventDispatcher dispatcher(e);
    dispatcher.Dispatch<WindowCloseEvent>([this](WindowCloseEvent& ev) { return OnWindowClose(ev); });
    dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) { return OnWindowResize(ev); });

    for (auto it = m_LayerStack.end(); it != m_LayerStack.begin();) {
        --it;
        if (e.Handled) break;
        (*it)->OnEvent(e);
    }
}

bool Application::OnWindowClose(WindowCloseEvent&) {
    m_Running = false;
    return true;
}

bool Application::OnWindowResize(WindowResizeEvent& e) {
    if (e.GetWidth() == 0 || e.GetHeight() == 0) {
        m_Minimized = true;
        return false;
    }
    m_Minimized = false;
    Renderer::OnWindowResize(e.GetWidth(), e.GetHeight());
    return false;
}

void Application::Run() {
    KZ_CORE_INFO("Kizuri Engine: iniciando o loop principal.");
#if defined(KZ_PLATFORM_ANDROID)
    
    
    using Clock = std::chrono::steady_clock;
    auto lastTick = Clock::now();
    while (m_Running && !AndroidPlatform::ShouldExit()) {
        
        
        AndroidPlatform::PumpGlue();
        auto now = Clock::now();
        float time = (float)std::chrono::duration<double>(now - lastTick).count();
        lastTick = now;
        Timestep timestep(time);
        m_LastFrameTime = time;

        if (!m_Minimized) {
            RenderCommand::ResetFrameStats();
            for (Layer* layer : m_LayerStack) {
                layer->OnUpdate(timestep);
            }
        }

        m_Window->OnUpdate();
    }
#else
    while (m_Running) {
        float time = (float)glfwGetTime();
        Timestep timestep(time - m_LastFrameTime);
        m_LastFrameTime = time;

        if (!m_Minimized) {
            RenderCommand::ResetFrameStats(); 
            for (Layer* layer : m_LayerStack) {
                KZ_CORE_TRACE("Application::Run — layer->OnUpdate ('{0}')", layer->GetName());
                layer->OnUpdate(timestep);
            }

            KZ_CORE_TRACE("Application::Run — ImGuiLayer::Begin");
            m_ImGuiLayer->Begin();
            for (Layer* layer : m_LayerStack) {
                KZ_CORE_TRACE("Application::Run — layer->OnImGuiRender ('{0}')", layer->GetName());
                layer->OnImGuiRender();
            }
            KZ_CORE_TRACE("Application::Run — ImGuiLayer::End");
            m_ImGuiLayer->End();
        }

        KZ_CORE_TRACE("Application::Run — Window::OnUpdate");
        m_Window->OnUpdate();
        KZ_CORE_TRACE("Application::Run — fim do frame");
    }
#endif
}

} 
