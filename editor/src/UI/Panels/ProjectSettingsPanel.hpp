#pragma once
#include "EditorPanel.hpp"




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
    void DrawAudioSection();
    void DrawAboutSection();

    const EditorContext& m_Ctx;
    int m_Section = 0; 
    char m_EnvHDRIPath[512] = "";

    void ApplyPreset();
    void Save();
};
