#pragma once
#include "EditorPanel.hpp"
#include <vector>

class ProfilerPanel : public EditorPanel {
public:
    explicit ProfilerPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Profiler"; }

    void OnImGuiRender() override;

private:
    void PushSample(float frameMs);

    const EditorContext& m_Ctx;
    std::vector<float> m_FrameTimes;
    static constexpr int kHistory = 180;
};
