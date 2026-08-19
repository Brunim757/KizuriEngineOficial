#pragma once
#include "EditorPanel.hpp"
#include <array>

class MaterialEditorPanel : public EditorPanel {
public:
    explicit MaterialEditorPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Material Editor"; }

    void OnUpdate(kizuri::Timestep ts) override;
    void OnImGuiRender() override;

private:
    const EditorContext& m_Ctx;
    kizuri::Ref<kizuri::Framebuffer> m_PreviewFramebuffer;
    kizuri::Ref<kizuri::Mesh> m_Sphere;

    kizuri::Material* TargetMaterial();
    void RenderPreview(uint32_t w, uint32_t h);
};
