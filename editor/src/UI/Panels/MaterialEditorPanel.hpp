#pragma once
#include "EditorPanel.hpp"
#include <array>

// Janela de material: edita o material do MeshRenderer selecionado com um
// PREVIEW ao vivo (esfera renderizada num framebuffer próprio, a cada frame).
// Os sliders/cores editam o material em tempo real; os MAPAS são atribuídos
// pelo Inspetor (que já tem os slots com drop/arquivo).
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

    kizuri::Material* TargetMaterial(); // nullptr se não houver MeshRenderer selecionado
    void RenderPreview(uint32_t w, uint32_t h);
};
