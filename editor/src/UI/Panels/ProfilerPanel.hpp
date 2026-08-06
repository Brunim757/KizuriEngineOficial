#pragma once
#include "EditorPanel.hpp"
#include <vector>

// Painel de perfilamento do editor: FPS, tempo de frame (com gráfico),
// draw calls, triângulos e contagem de entidades. Puramente informativo e
// aditivo — não altera nenhuma cena.
class ProfilerPanel : public EditorPanel {
public:
    explicit ProfilerPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Profiler"; }

    void OnImGuiRender() override;

private:
    void PushSample(float frameMs);

    const EditorContext& m_Ctx;
    std::vector<float> m_FrameTimes; // histórico de ms por frame (rolante)
    static constexpr int kHistory = 180;
};
