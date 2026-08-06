#pragma once
#include "EditorPanel.hpp"

// Painel "Project Settings" dockável: seções em sidebar (Gráficos, Geral,
// Editor, Sobre). Edita o mesmo GraphicsSettings do editor + preferências +
// janela. Substitui a janela modal Configurações por um painel de verdade.
class ProjectSettingsPanel : public EditorPanel {
public:
    explicit ProjectSettingsPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Project Settings"; }

    void OnImGuiRender() override;

private:
    void DrawSidebar();
    void DrawGraphicsSection();
    void DrawGeneralSection();
    void DrawEditorSection();
    void DrawAboutSection();

    const EditorContext& m_Ctx;
    int m_Section = 0; // 0=Gráficos, 1=Geral, 2=Editor, 3=Sobre
    char m_EnvHDRIPath[512] = "";

    void ApplyPreset();
    void Save();
};
