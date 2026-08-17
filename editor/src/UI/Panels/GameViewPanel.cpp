#include "GameViewPanel.hpp"
#include <imgui.h>
#include <algorithm>

namespace kizuri {

// Acha a câmera primária ativa (ou a primeira de perspectiva, caída) da
// cena — usada pelo preview e pelo editor de câmera.
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

static bool SceneHasPrimaryCamera(const Scene& scene) {
    auto& registry = const_cast<Scene&>(scene).GetRegistry();
    auto view = registry.view<const TransformComponent, const CameraComponent>();
    for (auto e : view) {
        auto* cc = view.get<const CameraComponent>(e);
        if (cc->Primary && scene.IsEntityActive(Entity{ e, &const_cast<Scene&>(scene) }))
            return true;
    }
    return false;
}

} // namespace kizuri

void GameViewPanel::OnUpdate(kizuri::Timestep ts) {
    (void)ts;
    if (!m_Visible) return;
    kizuri::Scene* scene = m_Ctx.ActiveScene.get();
    if (!scene) return;

    // Renderiza a câmera do jogo SEMPRE que existe câmera primária — em
    // Play ou em Edit (preview ao vivo; o testador pediu pra não precisar
    // dar Play pra ver a câmera).
    if (!kizuri::SceneHasPrimaryCamera(*scene)) {
        // Sem câmera: limpa (sem ficar congelado no último frame).
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

    // TAA/MotionBlur têm histórico global entre frames — render extra não pode
    // sobrescrever o histórico do viewport principal.
    kizuri::ScopedTemporalOff temporal;
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
    if (ImGui::SmallButton("✦ Editar câmera")) {
        m_ShowCameraEditor = !m_ShowCameraEditor;
        if (scene) {
            kizuri::Entity cam;
            kizuri::CameraComponent* cc = nullptr;
            if (kizuri::FindPrimaryCamera(*scene, cam, cc) && m_Ctx.SelectEntity)
                m_Ctx.SelectEntity(cam);
        }
    }

    // ---- Mini-editor da câmera primária, AO VIVO (edit persiste; play
    // atua na cópia em execução — config não se perde ao parar).
    if (m_ShowCameraEditor && scene) {
        ImGui::Separator();
        kizuri::Entity cam;
        kizuri::CameraComponent* cc = nullptr;
        if (kizuri::FindPrimaryCamera(*scene, cam, cc)) {
            bool changed = false;
            int type = (int)cc->Type;
            if (ImGui::Combo("Tipo", &type, "2D (Ortográfica)\0" "3D (Perspectiva)\0")) {
                cc->Type = (kizuri::CameraComponent::ProjectionType)type;
                changed = true;
            }
            if (cc->Type == kizuri::CameraComponent::ProjectionType::Perspective3D) {
                changed |= ImGui::DragFloat("FOV (graus)", &cc->PerspectiveFOV, 0.5f, 20.0f, 120.0f);
                changed |= ImGui::DragFloat("Near", &cc->NearClip, 0.01f, 0.01f, 10.0f);
                changed |= ImGui::DragFloat("Far", &cc->FarClip, 1.0f, 10.0f, 2000.0f);
            } else {
                changed |= ImGui::DragFloat("Tamanho (Ortho)", &cc->OrthoSize, 0.1f, 0.5f, 200.0f);
            }
            if (ImGui::Checkbox("Câmera principal", &cc->Primary)) changed = true;
            if (changed && scene)
                scene->OnViewportResize((uint32_t)m_Ctx.ViewportSize.x, (uint32_t)m_Ctx.ViewportSize.y);
            ImGui::TextDisabled("%s", m_Ctx.IsPlay
                ? "Mudanças valem durante o Play (a cena original não é tocada)."
                : "Mudanças vão pra cena (salve pra manter). Preview atualizado ao vivo.");
        } else {
            ImGui::TextDisabled("Nenhuma câmera na cena — crie uma (Inspetor > + Adicionar Componente > Camera).");
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