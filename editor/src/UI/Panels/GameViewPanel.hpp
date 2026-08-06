#pragma once
#include "EditorPanel.hpp"

// Aba "Game View": mostra o JOGO rodando (Play) num framebuffer próprio,
// separado do viewport de edição. No Play, o update (física/scripts) roda
// uma vez no viewport; esta aba só RE-renderiza a mesma cena pro FBO dela
// via Scene::RenderRuntimeView().
class GameViewPanel : public EditorPanel {
public:
    explicit GameViewPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Game View"; }

    void OnUpdate(kizuri::Timestep ts) override;
    void OnImGuiRender() override;

private:
    const EditorContext& m_Ctx;
    kizuri::Ref<kizuri::Framebuffer> m_Framebuffer;
    bool m_Focused = false;
    bool m_Hovered = false;
};
