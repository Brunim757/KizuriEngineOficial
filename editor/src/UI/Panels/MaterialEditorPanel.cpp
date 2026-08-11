#include "MaterialEditorPanel.hpp"
#include <imgui.h>

kizuri::Material* MaterialEditorPanel::TargetMaterial() {
    // HasComponent/GetComponent não são const na Entity — copia para local.
    kizuri::Entity sel = m_Ctx.SelectedEntity;
    if (!sel || !m_Ctx.ActiveScene) return nullptr;
    if (!sel.HasComponent<kizuri::MeshRendererComponent>()) return nullptr;
    return &sel.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
}

void MaterialEditorPanel::RenderPreview(uint32_t w, uint32_t h) {
    if (!m_PreviewFramebuffer)
        m_PreviewFramebuffer = kizuri::Framebuffer::Create({ w, h, 1 });
    m_PreviewFramebuffer->Resize(w, h);
    if (!m_Sphere) m_Sphere = kizuri::Mesh::CreateSphere(24, 12);

    kizuri::Material* mat = TargetMaterial();
    if (!mat) return;

    // TAA/MotionBlur têm histórico global entre frames — o preview não pode
    // sobrescrever o histórico do viewport principal.
    ScopedTemporalOff temporal;
    m_PreviewFramebuffer->Bind();
    kizuri::RenderCommand::SetClearColor({ 0.10f, 0.11f, 0.13f, 1.0f });
    kizuri::RenderCommand::Clear();

    float aspect = h > 0 ? (float)w / (float)h : 1.0f;
    kizuri::PerspectiveCamera cam(40.0f, aspect, 0.1f, 20.0f);
    cam.SetPosition({ 0.0f, 0.0f, 2.6f });
    cam.SetRotation(0.0f, 15.0f);

    kizuri::Renderer3D::BeginScene(cam);
    kizuri::Light dl;
    dl.Type = kizuri::LightType::Directional;
    dl.Direction = { -0.4f, -1.0f, -0.3f };
    dl.Color = { 1.0f, 1.0f, 1.0f };
    dl.Intensity = 1.6f;
    kizuri::Renderer3D::SubmitLight(dl);
    kizuri::Light fill;
    fill.Type = kizuri::LightType::Point;
    fill.Position = { 1.2f, 1.0f, 1.5f };
    fill.Color = { 0.6f, 0.8f, 1.0f };
    fill.Intensity = 2.0f;
    fill.Range = 6.0f;
    kizuri::Renderer3D::SubmitLight(fill);

    kizuri::Renderer3D::Submit(m_Sphere, *mat, glm::mat4(1.0f));
    kizuri::Renderer3D::EndScene();
    m_PreviewFramebuffer->Unbind();

    kizuri::Application& app = kizuri::Application::Get();
    auto& window = app.GetWindow();
    kizuri::RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
}

void MaterialEditorPanel::OnUpdate(kizuri::Timestep ts) {
    (void)ts;
    if (!m_Visible) return;
    kizuri::Entity sel = m_Ctx.SelectedEntity;
    if (!m_Ctx.ActiveScene || !sel) return;
    if (!sel.HasComponent<kizuri::MeshRendererComponent>()) return;
    RenderPreview(320, 240);
}

void MaterialEditorPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    kizuri::Material* mat = TargetMaterial();
    if (!mat) {
        ImGui::TextDisabled("Selecione uma entidade com MeshRenderer para editar o material.");
        ImGui::End();
        return;
    }

    if (m_PreviewFramebuffer) {
        uint32_t texID = m_PreviewFramebuffer->GetColorAttachmentRendererID();
        ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(240.0f, 180.0f), ImVec2(0, 1), ImVec2(1, 0));
        ImGui::Separator();
    }

    ImGui::ColorEdit3("Albedo", &mat->Albedo.x);
    ImGui::DragFloat("Metallic", &mat->Metallic, 0.01f, 0.0f, 1.0f);
    ImGui::DragFloat("Roughness", &mat->Roughness, 0.01f, 0.02f, 1.0f);
    ImGui::DragFloat("AO", &mat->AO, 0.01f, 0.0f, 1.0f);
    ImGui::Separator();
    ImGui::ColorEdit3("Emissive", &mat->Emissive.x);
    ImGui::DragFloat("Intensidade emissiva", &mat->EmissiveStrength, 0.01f, 0.0f, 20.0f);
    ImGui::Separator();
    ImGui::DragFloat("Escala de paralaxe (POM)", &mat->HeightScale, 0.001f, 0.0f, 0.5f);
    ImGui::Checkbox("Reflexão planar (espelho)", &mat->PlanarReflect);
    ImGui::Separator();

    // Slots de mapa: campo de texto + botão nativo "..." + botão
    // "Gerenciador" (abre o Content Browser na pasta) + drop de arquivo.
    // O material é editado SOMENTE aqui — o Inspetor não duplica.
    auto mapSlot = [&](const char* label, std::string& path, kizuri::Ref<kizuri::Texture2D>& tex) {
        char buf[512];
        strncpy(buf, path.c_str(), sizeof(buf) - 1);
        buf[sizeof(buf) - 1] = '\0';
        if (ImGui::InputText(label, buf, sizeof(buf))) {
            path = buf;
            tex = path.empty() ? nullptr : kizuri::Texture2D::Create(path);
        }
        ImGui::SameLine();
        if (ImGui::Button("...")) {
            std::string picked = kizuri::FileDialog::OpenFile(label, "*.png;*.jpg;*.jpeg;*.bmp;*.tga");
            if (!picked.empty()) {
                path = kizuri::Project::MakeRelativePath(picked);
                tex = path.empty() ? nullptr : kizuri::Texture2D::Create(path);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Gerenciador")) {
            if (m_Ctx.RevealInContentBrowser) m_Ctx.RevealInContentBrowser(path);
        }
        if (tex) {
            uint32_t id = tex->GetRendererID();
            ImGui::Image((ImTextureID)(uint64_t)id, ImVec2(64.0f, 64.0f), ImVec2(0, 1), ImVec2(1, 0));
        }
    };
    mapSlot("Mapa de Albedo", mat->AlbedoMapPath, mat->AlbedoMap);
    mapSlot("Mapa de Normais", mat->NormalMapPath, mat->NormalMap);
    mapSlot("Mapa Metallic/Roughness", mat->MetallicRoughnessMapPath, mat->MetallicRoughnessMap);
    mapSlot("Mapa Emissivo", mat->EmissiveMapPath, mat->EmissiveMap);
    mapSlot("Mapa de Altura (POM)", mat->HeightMapPath, mat->HeightMap);
    ImGui::End();
}
