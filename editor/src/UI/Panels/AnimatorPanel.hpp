#pragma once
#include "EditorPanel.hpp"

// Painel de animação (skinning): controles do AnimatorComponent da entidade
// selecionada — lista de clips, play/pause, loop, velocidade e time scrubber.
// Substitui a UI espremida no Inspetor por um painel dockável dedicado.
class AnimatorPanel : public EditorPanel {
public:
    explicit AnimatorPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Animator"; }

    void OnImGuiRender() override;

private:
    const EditorContext& m_Ctx;
    int m_SelectedClip = -1;
};
