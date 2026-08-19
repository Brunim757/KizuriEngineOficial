#pragma once
#include "EditorPanel.hpp"





class ParticleEditorPanel : public EditorPanel {
public:
    explicit ParticleEditorPanel(EditorContext& ctx) : m_Ctx(ctx) {}

    const char* GetTitle() const override { return "Particle Editor"; }
    void OnUpdate(kizuri::Timestep ts) override;
    void OnImGuiRender() override;

private:
    EditorContext& m_Ctx;
    kizuri::Ref<kizuri::Framebuffer> m_PreviewFramebuffer;
    float m_PreviewTime = 0.0f;
    std::vector<kizuri::Particle> m_PreviewParticles; 
    float m_PreviewAccumulator = 0.0f;
    bool m_PreviewDirty = true; 
};