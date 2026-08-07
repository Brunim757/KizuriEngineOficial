#include "GameViewPanel.hpp"
#include <imgui.h>
#include <algorithm>

void GameViewPanel::OnUpdate(kizuri::Timestep ts) {
    (void)ts;
    // Renderiza SEMPRE (edição E Play): o Game View mostra a visão da CÂMERA
    // PRINCIPAL da cena ao vivo — edite no viewport e veja como fica pro
    // jogador sem apertar Play. Mesmo tamanho/aspecto do viewport
    // (Scene::RenderRuntimeView usa o m_ViewportWidth/Height).
    if (!m_Visible || !m_Ctx.ActiveScene) return;
    if (m_Ctx.ViewportSize.x < 1.0f || m_Ctx.ViewportSize.y < 1.0f) return;

    if (!m_Framebuffer)
        m_Framebuffer = kizuri::Framebuffer::Create({ (uint32_t)m_Ctx.ViewportSize.x, (uint32_t)m_Ctx.ViewportSize.y, 1 });
    m_Framebuffer->Resize((uint32_t)m_Ctx.ViewportSize.x, (uint32_t)m_Ctx.ViewportSize.y);

    // TAA/MotionBlur têm histórico global entre frames — render extra não pode
    // sobrescrever o histórico do viewport principal.
    ScopedTemporalOff temporal;
    m_Framebuffer->Bind();
    kizuri::RenderCommand::SetClearColor({ 0.05f, 0.05f, 0.06f, 1.0f });
    kizuri::RenderCommand::Clear();
    m_Ctx.ActiveScene->RenderRuntimeView();
    m_Framebuffer->Unbind();

    // Restaura o viewport pro tamanho da janela (o Framebuffer trocou o
    // glViewport pro tamanho dele — sem restaurar, o ImGui herdaria errado).
    kizuri::Application& app = kizuri::Application::Get();
    auto& window = app.GetWindow();
    kizuri::RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
}

void GameViewPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    if (!m_Framebuffer) {
        ImGui::TextDisabled("Visão ao vivo da câmera principal da cena.");
        ImGui::End();
        return;
    }

    // Moldura "GAME" no topo do painel.
    ImGui::PushStyleColor(ImGuiCol_Text, IM_COL32(80, 220, 120, 255));
    ImGui::Text("GAME");
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::TextDisabled("câmera principal %s", m_Ctx.IsPlay ? "(Play)" : "(edição, ao vivo)");
    ImGui::SameLine();
    if (ImGui::SmallButton("Focar câmera")) {
        // Seleciona a primeira câmera principal da cena para o Inspetor.
        if (m_Ctx.ActiveScene) {
            auto cams = m_Ctx.ActiveScene->GetRegistry()
                .view<kizuri::TransformComponent, kizuri::CameraComponent>();
            for (auto e : cams) {
                auto& cam = cams.get<kizuri::CameraComponent>(e);
                if (cam.Primary && m_Ctx.SelectEntity) {
                    m_Ctx.SelectEntity(kizuri::Entity{ e, m_Ctx.ActiveScene.get() });
                    break;
                }
            }
        }
    }

    uint32_t texID = m_Framebuffer->GetColorAttachmentRendererID();
    ImVec2 avail = ImGui::GetContentRegionAvail();
    if (avail.x > 4.0f && avail.y > 4.0f) {
        float aspect = (float)m_Framebuffer->GetSpec().Width / (float)std::max(m_Framebuffer->GetSpec().Height, 1u);
        // Ajusta mantendo a proporção (letterbox).
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
