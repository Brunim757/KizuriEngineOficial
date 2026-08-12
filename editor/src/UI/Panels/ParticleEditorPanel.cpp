#include "ParticleEditorPanel.hpp"
#include <imgui.h>
#include <glm/gtc/random.hpp>

// Preview: simula as partículas localmente (mesma física do Scene: spawn na
// origem, velocidade/gravidade/idade) e renderiza num FBO próprio — sem
// precisar apertar Play pra ver o efeito.
void ParticleEditorPanel::OnUpdate(kizuri::Timestep ts) {
    if (!m_Visible) return;
    kizuri::Entity sel = m_Ctx.SelectedEntity;
    if (!m_Ctx.ActiveScene || !sel || !sel.HasComponent<kizuri::ParticleSystemComponent>()) {
        m_PreviewParticles.clear();
        return;
    }
    auto& pc = sel.GetComponent<kizuri::ParticleSystemComponent>();
    float dt = (float)ts;

    if (m_PreviewDirty) {
        m_PreviewDirty = false;
        m_PreviewParticles.clear();
        m_PreviewAccumulator = 0.0f;
        m_PreviewTime = 0.0f;
    }
    m_PreviewTime += dt;

    if (pc.Playing) {
        m_PreviewAccumulator += dt * pc.EmissionRate;
        int toSpawn = (int)m_PreviewAccumulator;
        m_PreviewAccumulator -= (float)toSpawn;
        for (int i = 0; i < toSpawn && m_PreviewParticles.size() < pc.MaxParticles; ++i) {
            kizuri::Particle p;
            p.Position = { 0.0f, 0.0f, 0.0f };
            p.Velocity = glm::linearRand(pc.VelocityMin, pc.VelocityMax);
            p.Lifetime = glm::linearRand(pc.LifetimeMin, pc.LifetimeMax);
            m_PreviewParticles.push_back(p);
        }
    }
    for (size_t i = 0; i < m_PreviewParticles.size();) {
        auto& p = m_PreviewParticles[i];
        p.Age += dt;
        if (p.Age >= p.Lifetime) { p = m_PreviewParticles.back(); m_PreviewParticles.pop_back(); continue; }
        p.Velocity += pc.Gravity * dt;
        p.Position += p.Velocity * dt;
        ++i;
    }

    // Renderiza o preview.
    if (!m_PreviewFramebuffer)
        m_PreviewFramebuffer = kizuri::Framebuffer::Create({ 320, 240, 1 });
    m_PreviewFramebuffer->Resize(320, 240);

    ScopedTemporalOff temporal;
    m_PreviewFramebuffer->Bind();
    kizuri::RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.11f, 1.0f });
    kizuri::RenderCommand::Clear();

    kizuri::PerspectiveCamera cam(45.0f, 320.0f / 240.0f, 0.05f, 20.0f);
    cam.SetPosition({ 0.0f, 1.6f, 4.2f });
    cam.SetRotation(0.0f, 10.0f);

    kizuri::Renderer3D::BeginScene(cam);
    if (!m_PreviewParticles.empty()) {
        std::vector<kizuri::ParticleInstance> instances;
        instances.reserve(m_PreviewParticles.size());
        for (auto& p : m_PreviewParticles) {
            float t = glm::clamp(p.Age / glm::max(p.Lifetime, 0.0001f), 0.0f, 1.0f);
            instances.push_back({ p.Position, glm::mix(pc.StartSize, pc.EndSize, t),
                                  glm::mix(pc.StartColor, pc.EndColor, t) });
        }
        kizuri::Renderer3D::SubmitParticles(instances, pc.Additive, pc.Texture);
    }
    kizuri::Renderer3D::EndScene();
    m_PreviewFramebuffer->Unbind();

    kizuri::Application& app = kizuri::Application::Get();
    auto& window = app.GetWindow();
    kizuri::RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
}

void ParticleEditorPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) { ImGui::End(); return; }
    kizuri::Entity sel = m_Ctx.SelectedEntity;
    if (!m_Ctx.ActiveScene || !sel || !sel.HasComponent<kizuri::ParticleSystemComponent>()) {
        ImGui::TextDisabled("Selecione uma entidade com Particle System (Componente > Particle System).");
        ImGui::End();
        return;
    }
    auto& pc = sel.GetComponent<kizuri::ParticleSystemComponent>();

    if (m_PreviewFramebuffer) {
        uint32_t texID = m_PreviewFramebuffer->GetColorAttachmentRendererID();
        ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(320.0f, 240.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Separator();
    }

    ImGui::Checkbox("Tocando", &pc.Playing);
    ImGui::SameLine();
    ImGui::Checkbox("Aditivo (fogo)", &pc.Additive);
    ImGui::SliderFloat("Taxa (partículas/s)", &pc.EmissionRate, 0.0f, 500.0f, "%.0f");
    int maxP = (int)pc.MaxParticles;
    if (ImGui::DragInt("Máximo de partículas", &maxP, 1, 1, 20000)) { pc.MaxParticles = (uint32_t)maxP; }
    ImGui::Text("Vida: %.2f ~ %.2f s", pc.LifetimeMin, pc.LifetimeMax);
    ImGui::DragFloat("Vida mínima", &pc.LifetimeMin, 0.05f, 0.05f, 60.0f);
    ImGui::DragFloat("Vida máxima", &pc.LifetimeMax, 0.05f, 0.05f, 60.0f);

    ImGui::Separator();
    ImGui::Text("Velocidade inicial (bloco de mundo):");
    ImGui::DragFloat3("Mínimo", &pc.VelocityMin.x, 0.05f);
    ImGui::DragFloat3("Máximo", &pc.VelocityMax.x, 0.05f);
    ImGui::DragFloat3("Gravidade", &pc.Gravity.x, 0.05f);

    ImGui::Separator();
    ImGui::ColorEdit4("Cor inicial", &pc.StartColor.x);
    ImGui::ColorEdit4("Cor final", &pc.EndColor.x);
    ImGui::DragFloat("Tamanho inicial", &pc.StartSize, 0.01f, 0.0f, 10.0f);
    ImGui::DragFloat("Tamanho final", &pc.EndSize, 0.01f, 0.0f, 10.0f);

    ImGui::Separator();
    char texBuf[512];
    strncpy(texBuf, pc.TexturePath.c_str(), sizeof(texBuf) - 1);
    texBuf[sizeof(texBuf) - 1] = '\0';
    if (ImGui::InputText("Textura (vazio = degradê radial)", texBuf, sizeof(texBuf))) {
        pc.TexturePath = texBuf;
        pc.Texture = pc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(pc.TexturePath);
    }
    ImGui::SameLine();
    if (ImGui::Button("Gerenciador")) {
        if (m_Ctx.RevealInContentBrowser) m_Ctx.RevealInContentBrowser(pc.TexturePath);
    }

    ImGui::Separator();
    ImGui::Text("Preview: %zu partículas (%.1fs)", m_PreviewParticles.size(), m_PreviewTime);
    if (ImGui::Button("Reiniciar preview")) m_PreviewDirty = true;
    ImGui::SameLine();
    ImGui::TextDisabled("Simulado localmente — a cena usa a mesma configuração no Play.");

    ImGui::End();
}