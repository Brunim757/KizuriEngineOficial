#pragma once
#include "EditorPanel.hpp"




class AnimatorPanel : public EditorPanel {
public:
    explicit AnimatorPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Animator"; }

    void OnImGuiRender() override;

private:
    const EditorContext& m_Ctx;
    int m_SelectedClip = -1;
};
