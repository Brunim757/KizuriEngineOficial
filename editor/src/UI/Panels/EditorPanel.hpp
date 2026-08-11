#pragma once
#include <Kizuri.hpp>
#include <imgui.h>
#include <functional>

// ---------------------------------------------------------------------------
// Framework de painéis do Kizuri Editor.
//
// Cada painel (Profiler, Game View, Material Editor, Animator, Project
// Settings...) é uma classe própria num arquivo próprio, derivada de
// EditorPanel. O EditorLayer só:
//   1. cria os painéis no construtor,
//   2. preenche o EditorContext a cada frame,
//   3. chama panel->OnUpdate(ts) no OnUpdate e panel->OnImGuiRender() na UI.
//
// Assim o EditorLayer NÃO cresce mais — os painéis novos não tocam nele.
// ---------------------------------------------------------------------------

// Estado compartilhado que os painéis leem. Preenchido pelo EditorLayer todo
// frame. Os painéis NUNCA dependem do EditorLayer diretamente.
struct EditorContext {
    kizuri::Ref<kizuri::Scene> ActiveScene;   // cena em uso (edit ou a cópia do Play)
    kizuri::Ref<kizuri::Scene> EditorScene;   // cena mestra (edição)
    kizuri::Entity SelectedEntity;
    bool IsPlay = false;
    float DeltaTime = 0.0f;
    float FpsSmoothed = 0.0f;                 // FPS médio (suavizado)
    glm::vec2 ViewportSize = { 0.0f, 0.0f };  // tamanho do viewport (px)
    kizuri::GraphicsSettings* Graphics = nullptr;
    bool ViewportFocused = false;
    bool ViewportHovered = false;

    // Ponteiros para preferências do editor (vivem no EditorLayer; o painel
    // edita direto). Projeto Settings > Editor usa isto.
    float* EditorCamFlySpeed = nullptr;
    float* EditorCamSensitivity = nullptr;
    float* GizmoSnapTranslation = nullptr;
    float* GizmoSnapRotation = nullptr;
    bool* AutoCompileOnPlay = nullptr;
    bool* ShowColliders = nullptr;

    // Ações que um painel pode pedir de volta ao EditorLayer.
    std::function<void(kizuri::Entity)> SelectEntity;
    std::function<void()> TogglePlay;
    // Abre o Content Browser na pasta do arquivo (botão "Gerenciador").
    std::function<void(const std::string& filePath)> RevealInContentBrowser;
};

class EditorPanel {
public:
    virtual ~EditorPanel() = default;

    virtual const char* GetTitle() const = 0;
    bool IsVisible() const { return m_Visible; }
    void SetVisible(bool visible) { m_Visible = visible; }

    // Update de verdade (renderização pra textura própria, ex.: Game View e
    // preview do Material Editor). Chamado pelo EditorLayer::OnUpdate.
    virtual void OnUpdate(kizuri::Timestep ts) { (void)ts; }

    // UI do painel dentro do dockspace. Chamado pelo OnImGuiRender.
    virtual void OnImGuiRender() = 0;

protected:
    bool m_Visible = false;
};

// Guarda RAII que desliga os efeitos TEMPORAIS do Renderer3D (TAA e Motion
// blur) durante um render EXTRA do frame (Game View, preview de material).
// TAA/MotionBlur guardam histórico entre frames no Renderer3D — se um painel
// rodar o pipeline de novo com eles ligados, ele sobrescreve o histórico do
// VIEWPORT principal e o próximo frame do editor fica com ghosting/borrado.
struct ScopedTemporalOff {
    kizuri::GraphicsSettings saved;
    ScopedTemporalOff() {
        saved = kizuri::Renderer3D::GetGraphicsSettings();
        if (saved.TAAEnabled || saved.MotionBlurEnabled) {
            saved.TAAEnabled = false;
            saved.MotionBlurEnabled = false;
            kizuri::Renderer3D::SetGraphicsSettings(saved);
        }
    }
    ~ScopedTemporalOff() {
        kizuri::Renderer3D::SetGraphicsSettings(saved);
    }
};
