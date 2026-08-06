#include "ProjectSettingsPanel.hpp"
#include <imgui.h>
#include <cstring>

void ProjectSettingsPanel::ApplyPreset() {
    if (!m_Ctx.Graphics) return;
    m_Ctx.Graphics->ApplyPreset(m_Ctx.Graphics->Preset);
    kizuri::Renderer3D::SetGraphicsSettings(*m_Ctx.Graphics);
    Save();
}

void ProjectSettingsPanel::Save() {
    if (!m_Ctx.Graphics) return;
    m_Ctx.Graphics->Clamp();
    kizuri::Renderer3D::SetGraphicsSettings(*m_Ctx.Graphics);
    if (kizuri::SaveGraphicsSettings("settings.json", *m_Ctx.Graphics))
        KZ_CORE_INFO("Project Settings: configurações salvas em settings.json.");
}

void ProjectSettingsPanel::DrawSidebar() {
    ImGui::BeginChild("##ps_sidebar", ImVec2(140.0f, 0.0f), true);
    const char* items[] = { "Gráficos", "Geral", "Editor", "Sobre" };
    for (int i = 0; i < 4; ++i) {
        if (ImGui::Selectable(items[i], m_Section == i)) m_Section = i;
    }
    ImGui::Separator();
    ImGui::TextDisabled("Kizuri Engine");
    ImGui::EndChild();
    ImGui::SameLine();
}

void ProjectSettingsPanel::DrawGraphicsSection() {
    if (!m_Ctx.Graphics) { ImGui::TextDisabled("Sem configurações gráficas."); return; }
    auto& g = *m_Ctx.Graphics;

    static const char* presetNames[] = { "Ultra", "High", "Medium", "Low", "Custom" };
    int presetIdx = (int)g.Preset;
    if (ImGui::Combo("Preset", &presetIdx, presetNames, 5)) {
        g.Preset = (kizuri::QualityPreset)presetIdx;
        ApplyPreset();
    }
    ImGui::Separator();

    bool changed = false;
    changed |= ImGui::DragFloat("Resolução interna (render scale)", &g.RenderScale, 0.01f, 0.25f, 2.0f);
    const char* msaaNames[] = { "1x (off)", "2x", "4x", "8x" };
    int msaaIdx = g.MSAA == 1 ? 0 : g.MSAA == 2 ? 1 : g.MSAA == 4 ? 2 : 3;
    if (ImGui::Combo("MSAA", &msaaIdx, msaaNames, 4)) { g.MSAA = (msaaIdx == 0) ? 1 : (msaaIdx == 1) ? 2 : (msaaIdx == 2) ? 4 : 8; changed = true; }

    static const char* shadowNames[] = { "512", "1024", "2048", "4096" };
    static const int shadowValues[] = { 512, 1024, 2048, 4096 };
    int shadowIdx = 0;
    for (int i = 0; i < 4; ++i) if (g.ShadowMapSize == shadowValues[i]) shadowIdx = i;
    if (ImGui::Combo("Shadow map (CSM)", &shadowIdx, shadowNames, 4)) { g.ShadowMapSize = shadowValues[shadowIdx]; changed = true; }
    changed |= ImGui::SliderInt("PCF", &g.ShadowPCFRadius, 0, 3);
    changed |= ImGui::DragFloat("Penumbra (PCSS)", &g.ShadowSoftness, 0.01f, 0.0f, 1.0f);
    ImGui::Separator();

    changed |= ImGui::Checkbox("SSAO", &g.SSAOEnabled);
    if (g.SSAOEnabled) { changed |= ImGui::SliderInt("Amostras SSAO", &g.SSAOSamples, 8, 64); changed |= ImGui::DragFloat("Raio SSAO", &g.SSAORadius, 0.01f, 0.05f, 2.0f); }
    changed |= ImGui::Checkbox("SSR (reflexos)", &g.SSREnabled);
    if (g.SSREnabled) { changed |= ImGui::SliderInt("Passos SSR", &g.SSRMaxSteps, 8, 48); changed |= ImGui::DragFloat("Intensidade SSR", &g.SSRIntensity, 0.01f, 0.0f, 2.0f); }
    changed |= ImGui::Checkbox("SSGI (iluminação global)", &g.SSGIEnabled);
    if (g.SSGIEnabled) changed |= ImGui::DragFloat("Intensidade SSGI", &g.SSGIIntensity, 0.01f, 0.0f, 2.0f);
    changed |= ImGui::Checkbox("TAA", &g.TAAEnabled);
    changed |= ImGui::Checkbox("FXAA", &g.FXAAEnabled);
    changed |= ImGui::Checkbox("God rays", &g.GodRaysEnabled);
    if (g.GodRaysEnabled) changed |= ImGui::DragFloat("Intensidade god rays", &g.GodRaysIntensity, 0.01f, 0.0f, 3.0f);
    changed |= ImGui::Checkbox("Nuvens volumétricas", &g.CloudsEnabled);
    changed |= ImGui::Checkbox("Lens flare", &g.LensFlareEnabled);
    if (g.LensFlareEnabled) changed |= ImGui::DragFloat("Intensidade lens flare", &g.LensFlareIntensity, 0.01f, 0.0f, 3.0f);
    ImGui::Separator();

    changed |= ImGui::Checkbox("Bloom", &g.BloomEnabled);
    if (g.BloomEnabled) { changed |= ImGui::DragFloat("Limiar bloom", &g.BloomThreshold, 0.01f, 0.1f, 10.0f); changed |= ImGui::DragFloat("Intensidade bloom", &g.BloomIntensity, 0.01f, 0.0f, 3.0f); }
    changed |= ImGui::SliderInt("Iterações bloom", &g.BloomIterations, 1, 12);
    changed |= ImGui::DragFloat("Bloom anamórfico", &g.BloomAnamorphic, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::Checkbox("DOF (bokeh)", &g.DOFEnabled);
    if (g.DOFEnabled) { changed |= ImGui::DragFloat("Distância focal", &g.DOFFocusDistance, 0.1f, 0.1f, 500.0f); changed |= ImGui::DragFloat("Força bokeh", &g.DOFStrength, 0.05f, 0.0f, 5.0f); }
    changed |= ImGui::Checkbox("Motion blur", &g.MotionBlurEnabled);
    if (g.MotionBlurEnabled) changed |= ImGui::DragFloat("Intensidade motion blur", &g.MotionBlurIntensity, 0.01f, 0.0f, 2.0f);
    ImGui::Separator();

    changed |= ImGui::DragFloat("Exposição", &g.Exposure, 0.01f, 0.1f, 8.0f);
    static const char* tmNames[] = { "ACES", "Reinhard", "Filmic" };
    changed |= ImGui::Combo("Tonemapping", &g.ToneMapping, tmNames, 3);
    changed |= ImGui::DragFloat("Saturação", &g.Saturation, 0.01f, 0.0f, 2.0f);
    changed |= ImGui::DragFloat("Contraste", &g.Contrast, 0.01f, 0.0f, 2.0f);
    changed |= ImGui::DragFloat("Vinheta", &g.Vignette, 0.01f, 0.0f, 1.0f);
    changed |= ImGui::Checkbox("Fog", &g.FogEnabled);
    if (g.FogEnabled) {
        changed |= ImGui::DragFloat("Densidade da névoa", &g.FogDensity, 0.001f, 0.0f, 0.2f);
        changed |= ImGui::DragFloat("Altura da névoa", &g.FogHeight, 0.1f, -100.0f, 100.0f);
        changed |= ImGui::DragFloat("Atenuação por altura", &g.FogHeightFalloff, 0.1f, 0.0f, 200.0f);
        ImGui::ColorEdit3("Cor da névoa", g.FogColor);
    }
    ImGui::Separator();

    // Céu: HDRI, ou gradiente procedural (limpo), ou raymarch atmosférico.
    ImGui::Text("Ambiente (céu):");
    ImGui::TextDisabled(".hdr equirect = ambiente. Vazio = gradiente procedural.");
    changed |= ImGui::Checkbox("Céu atmosférico Rayleigh/Mie", &g.AtmosphereSky);
    ImGui::TextDisabled("Rayleigh/Mie é raymarch físico — pode pesar em GPUs fracas/emuladores.");
    if (m_EnvHDRIPath[0] == '\0')
        std::strncpy(m_EnvHDRIPath, kizuri::Renderer3D::GetEnvironmentHDRIPath().c_str(), sizeof(m_EnvHDRIPath) - 1);
    if (ImGui::InputText("HDRI do céu", m_EnvHDRIPath, sizeof(m_EnvHDRIPath))) {
        kizuri::Renderer3D::SetEnvironmentHDRIPath(m_EnvHDRIPath);
    }

    if (changed) { g.Preset = kizuri::QualityPreset::Custom; Save(); }
}

void ProjectSettingsPanel::DrawGeneralSection() {
    kizuri::Application& app = kizuri::Application::Get();
    auto& window = app.GetWindow();

    ImGui::TextDisabled("Janela");
    ImGui::Separator();
    static int w = 0, h = 0;
    if (w == 0) { w = (int)window.GetWidth(); h = (int)window.GetHeight(); }
    ImGui::SetNextItemWidth(90.0f); ImGui::InputInt("Largura", &w);
    ImGui::SetNextItemWidth(90.0f); ImGui::InputInt("Altura", &h);
    if (ImGui::Button("Aplicar resolução")) {
        window.SetSize(w, h);
    }
    ImGui::SameLine();
    if (ImGui::Button(window.IsMaximized() ? "Restaurar" : "Maximizar")) window.ToggleMaximize();
    ImGui::SameLine();
    if (ImGui::Button("Minimizar")) window.Minimize();
    ImGui::Checkbox("VSync", &m_Ctx.Graphics->VSync);
    if (ImGui::IsItemDeactivatedAfterEdit() && m_Ctx.Graphics) { window.SetVSync(m_Ctx.Graphics->VSync); Save(); }
    ImGui::Separator();

    ImGui::TextDisabled("Informações de OpenGL");
    ImGui::Separator();
    ImGui::TextWrapped("%s", kizuri::GetOpenGLVersionString().c_str());
}

void ProjectSettingsPanel::DrawEditorSection() {
    ImGui::TextDisabled("Preferências do editor (aplicam no próximo uso)");
    ImGui::Separator();
    if (m_Ctx.EditorCamFlySpeed) ImGui::DragFloat("Velocidade da câmera livre", m_Ctx.EditorCamFlySpeed, 0.1f, 0.1f, 60.0f);
    if (m_Ctx.EditorCamSensitivity) ImGui::DragFloat("Sensibilidade do mouse", m_Ctx.EditorCamSensitivity, 0.01f, 0.01f, 1.0f);
    if (m_Ctx.GizmoSnapTranslation) ImGui::DragFloat("Snap de translação", m_Ctx.GizmoSnapTranslation, 0.05f, 0.0f, 10.0f);
    if (m_Ctx.GizmoSnapRotation) ImGui::DragFloat("Snap de rotação (graus)", m_Ctx.GizmoSnapRotation, 1.0f, 0.0f, 90.0f);
    if (m_Ctx.AutoCompileOnPlay) ImGui::Checkbox("Compilar C# antes do Play", m_Ctx.AutoCompileOnPlay);
    if (m_Ctx.ShowColliders) ImGui::Checkbox("Desenhar colliders (overlay)", m_Ctx.ShowColliders);
    ImGui::Separator();
    ImGui::TextDisabled("Modo 2D/3D do viewport (botão na toolbar do viewport).");
}

void ProjectSettingsPanel::DrawAboutSection() {
    ImGui::Text("Kizuri Engine");
    ImGui::TextDisabled("Editor + Engine 3D/2D — 100%% OpenGL 3.3 core");
    ImGui::Separator();
    ImGui::TextWrapped("Render 3D PBR + IBL, física 2D/3D, scripting C# (Kizuri.Scripting), "
                       "tilemap, partículas, áudio, UI e export de jogo standalone.");
}

void ProjectSettingsPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    DrawSidebar();
    ImGui::BeginChild("##ps_body");
    switch (m_Section) {
        case 0: DrawGraphicsSection(); break;
        case 1: DrawGeneralSection(); break;
        case 2: DrawEditorSection(); break;
        default: DrawAboutSection(); break;
    }
    ImGui::EndChild();
    ImGui::End();
}
