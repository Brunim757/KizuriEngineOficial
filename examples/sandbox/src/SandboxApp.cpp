#include <Kizuri.hpp>
#include <kizuri/core/EntryPoint.hpp>
#include "PlayerController.hpp"
#include <imgui.h>

using namespace kizuri;

class SandboxLayer : public Layer {
public:
    SandboxLayer() : Layer("SandboxLayer") {}

    void OnAttach() override {
        m_Scene = CreateRef<Scene>("Cena de Demonstração");

        Entity cameraEntity = m_Scene->CreateEntity("Camera2D");
        auto& cam = cameraEntity.AddComponent<CameraComponent>();
        cam.Type = CameraComponent::ProjectionType::Orthographic2D;
        cam.OrthoSize = 5.0f;
        cam.Primary = true;

        m_Player = m_Scene->CreateEntity("Player");
        auto& sprite = m_Player.AddComponent<SpriteRendererComponent>();
        sprite.Color = { 0.9f, 0.2f, 0.25f, 1.0f };
        m_Player.AddComponent<Rigidbody2DComponent>();
        m_Player.AddComponent<BoxCollider2DComponent>();
        auto& nsc = m_Player.AddComponent<NativeScriptComponent>();
        nsc.Bind<PlayerController>();

        Entity ground = m_Scene->CreateEntity("Chao");
        auto& groundTransform = ground.GetComponent<TransformComponent>();
        groundTransform.Translation = { 0.0f, -3.0f, 0.0f };
        groundTransform.Scale = { 10.0f, 0.5f, 1.0f };
        auto& groundSprite = ground.AddComponent<SpriteRendererComponent>();
        groundSprite.Color = { 0.3f, 0.3f, 0.35f, 1.0f };
        auto& groundBody = ground.AddComponent<Rigidbody2DComponent>();
        groundBody.Type = Rigidbody2DComponent::BodyType::Static;
        ground.AddComponent<BoxCollider2DComponent>();

        KZ_INFO("Kizuri Sandbox iniciado. Use WASD ou as setas para mover o jogador.");
        m_Scene->OnRuntimeStart();
    }

    void OnDetach() override {
        m_Scene->OnRuntimeStop();
    }

    void OnUpdate(Timestep ts) override {
        RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.11f, 1.0f });
        RenderCommand::Clear();
        m_Scene->OnUpdateRuntime(ts);
    }

    void OnImGuiRender() override {
        ImGui::Begin("Kizuri Engine — Debug");
        ImGui::Text("Cena: %s", m_Scene->GetName().c_str());
        auto stats = Renderer2D::GetStats();
        ImGui::Text("Draw Calls: %d", stats.DrawCalls);
        ImGui::Text("Quads: %d", stats.QuadCount);
        ImGui::Separator();
        ImGui::TextWrapped("Este é o Sandbox de exemplo da Kizuri Engine, escrito em C++ "
                            "(a futura KZScript irá gerar código equivalente a este).");
        ImGui::End();
        Renderer2D::ResetStats();
    }

    void OnEvent(Event& e) override {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
            if (ev.GetKeyCode() == Key::Escape) Application::Get().Close();
            return false;
        });
    }

private:
    Ref<Scene> m_Scene;
    Entity m_Player;
};

class SandboxApp : public Application {
public:
    SandboxApp() : Application(MakeSpec()) {
        PushLayer(new SandboxLayer());
    }

    static ApplicationSpec MakeSpec() {
        ApplicationSpec spec;
        spec.Name = "Kizuri Engine — Sandbox";
        spec.Width = 1600;
        spec.Height = 900;
        spec.VSync = true;
        return spec;
    }
};

kizuri::Application* kizuri::CreateApplication() {
    return new SandboxApp();
}
