#pragma once
#include "EditorPanel.hpp"

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