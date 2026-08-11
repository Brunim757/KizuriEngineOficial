#pragma once
#include "EditorPanel.hpp"

// Editor de partículas (pilar AAA v0.34): painel dedicado que edita o
// ParticleSystemComponent da entidade selecionada com PREVIEW ao vivo —
// simula as partículas localmente (mesma física do Scene) e renderiza num
// FBO próprio, além de todos os campos com feedback imediato.
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
    std::vector<kizuri::Particle> m_PreviewParticles; // cópia da simulação
    float m_PreviewAccumulator = 0.0f;
    bool m_PreviewDirty = true; // o componente mudou — reseta a simulação
};