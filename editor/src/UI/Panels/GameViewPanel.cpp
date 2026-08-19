#include "GameViewPanel.hpp"
#include <imgui.h>
#include <algorithm>

namespace kizuri {



static bool FindPrimaryCamera(Scene& scene, Entity& outCamera, CameraComponent*& outComp) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<TransformComponent, CameraComponent>();
    for (auto e : view) {
        Entity ent{ e, &scene };
        if (!scene.IsEntityActive(ent)) continue;
        auto& cam = view.get<CameraComponent>(e);
        if (cam.Primary) {
            outCamera = ent;
            outComp = &cam;
            return true;
        }
    }
    for (auto e : view) {
        Entity ent{ e, &scene };
        if (!scene.IsEntityActive(ent)) continue;
        outCamera = ent;
        outComp = &view.get<CameraComponent>(e);
        return true;
    }
    return false;
}

static bool SceneHasPrimaryCamera(Scene& scene) {
    auto& registry = scene.GetRegistry();
    auto view = registry.view<TransformComponent, CameraComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraComponent>(e);
        if (cc.Primary && scene.IsEntityActive(Entity{ e, &scene }))
            return true;
    }
    return false;
}

} 

void GameViewPanel::OnUpdate(kizuri::Timestep ts) {
    (void)ts;
    if (!m_Visible) return;
    kizuri::Scene* scene = m_Ctx.ActiveScene.get();
    if (!scene) return;

    
    
    
    if (!kizuri::SceneHasPrimaryCamera(*scene)) {
        
        if (m_Framebuffer) {
            m_Framebuffer->Bind();
            kizuri::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
            kizuri::RenderCommand::Clear();
            m_Framebuffer->Unbind();
            auto& win = kizuri::Application::Get().GetWindow();
            kizuri::RenderCommand::SetViewport(0, 0, win.GetWidth(), win.GetHeight());
        }
        return;
    }

    if (m_Ctx.ViewportSize.x < 1.0f || m_Ctx.ViewportSize.y < 1.0f) return;

    if (!m_Framebuffer)
        m_Framebuffer = kizuri::Framebuffer::Create({ (uint32_t)m_Ctx.ViewportSize.x, (uint32_t)m_Ctx.ViewportSize.y, 1 });
    m_Framebuffer->Resize((uint32_t)m_Ctx.ViewportSize.x, (uint32_t)m_Ctx.ViewportSize.y);

    
    
    ScopedTemporalOff temporal;
    m_Framebuffer->Bind();
    kizuri::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
    kizuri::RenderCommand::Clear();
    scene->RenderRuntimeView();
    m_Framebuffer->Unbind();

    auto& win = kizuri::Application::Get().GetWindow();
    kizuri::RenderCommand::SetViewport(0, 0, win.GetWidth(), win.GetHeight());
}

void GameViewPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    kizuri::Scene* scene = m_Ctx.ActiveScene.get();
    bool hasCamera = scene && kizuri::SceneHasPrimaryCamera(*scene);

    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 220, 120, 255));
    ImGui::Text("GAME");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("%s", m_Ctx.IsPlay ? "Play — câmera do jogador" : "preview ao vivo (sem Play)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Focar câmera")) {
        
        
        
        if (scene) {
            kizuri::Entity cam;
            kizuri::CameraComponent* cc = nullptr;
            if (kizuri::FindPrimaryCamera(*scene, cam, cc) && m_Ctx.SelectEntity)
                m_Ctx.SelectEntity(cam);
        }
    }

    if (!m_Framebuffer || !hasCamera) {
        ImGui::TextDisabled("Sem câmera primária na cena — este painel mostra a câmera do jogo.");
        ImGui::End();
        return;
    }

    uint32_t texID = m_Framebuffer->GetColorAttachmentRendererID();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 4.0f && avail.y > 4.0f) {
        float aspect = (float)m_Framebuffer->GetSpec().Width / (float)std::max(m_Framebuffer->GetSpec().Height, 1u);
        ImVec2 size = avail;
        if (size.x / size.y > aspect) size.x = size.y * aspect;
        else size.y = size.x / aspect;
        ImGui::Image((ImTextureID)(uint64_t)texID, size, ImVec2(0, 1), ImVec2(1, 0));
        m_Hovered = ImGui::IsItemHovered();
        m_Focused = ImGui::IsItemFocused();
        ImGui::TextDisabled("%ux%u  (%.0f FPS)",
                            m_Framebuffer->GetSpec().Width, m_Framebuffer->GetSpec().Height, m_Ctx.FpsSmoothed);
    }
    ImGui::End();
}