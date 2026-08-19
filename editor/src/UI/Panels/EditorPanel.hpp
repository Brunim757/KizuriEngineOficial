#pragma once
#include <Kizuri.hpp>
#include <imgui.h>
#include <functional>

struct EditorContext {
    kizuri::Ref<kizuri::Scene> ActiveScene;
    kizuri::Ref<kizuri::Scene> EditorScene;
    kizuri::Entity SelectedEntity;
    bool IsPlay = false;
    float DeltaTime = 0.0f;
    float FpsSmoothed = 0.0f;
    glm::vec2 ViewportSize = { 0.0f, 0.0f };
    kizuri::GraphicsSettings* Graphics = nullptr;
    bool ViewportFocused = false;
    bool ViewportHovered = false;

    float* EditorCamFlySpeed = nullptr;
    float* EditorCamSensitivity = nullptr;
    float* GizmoSnapTranslation = nullptr;
    float* GizmoSnapRotation = nullptr;
    bool* AutoCompileOnPlay = nullptr;
    bool* ShowColliders = nullptr;

    std::function<void(kizuri::Entity)> SelectEntity;
    std::function<void()> TogglePlay;

    std::function<void(const std::string& filePath)> RevealInContentBrowser;
};

class EditorPanel {
public:
    virtual ~EditorPanel() = default;

    virtual const char* GetTitle() const = 0;
    bool IsVisible() const { return m_Visible; }
    void SetVisible(bool visible) { m_Visible = visible; }

    virtual void OnUpdate(kizuri::Timestep ts) { (void)ts; }

    virtual void OnImGuiRender() = 0;

protected:
    bool m_Visible = false;
};

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
