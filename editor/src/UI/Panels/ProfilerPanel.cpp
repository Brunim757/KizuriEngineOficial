#include "ProfilerPanel.hpp"
#include <imgui.h>
#include <cmath>
#include <cstdio>

void ProfilerPanel::PushSample(float frameMs) {
    m_FrameTimes.push_back(frameMs);
    if ((int)m_FrameTimes.size() > kHistory) m_FrameTimes.erase(m_FrameTimes.begin());
}

void ProfilerPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    float frameMs = m_Ctx.DeltaTime > 0.0f ? m_Ctx.DeltaTime * 1000.0f : 0.0f;
    PushSample(frameMs);

    ImGui::TextDisabled("Ciclo de jogo");
    ImGui::Separator();
    ImGui::Text("FPS: %.1f", m_Ctx.FpsSmoothed);
    ImGui::Text("Frame: %.2f ms", frameMs);
    ImGui::Text("Draw calls: %u", kizuri::RenderCommand::GetFrameDrawCalls());
    ImGui::Text("Triângulos: %u", kizuri::RenderCommand::GetFrameTriangles());
    if (m_Ctx.ActiveScene)
        ImGui::Text("Entidades: %u", (uint32_t)m_Ctx.ActiveScene->GetRegistry().view<kizuri::TransformComponent>().size());

    ImGui::Separator();
    ImGui::TextDisabled("Tempo de frame (últimos %d frames)", kHistory);

    if (!m_FrameTimes.empty()) {
        // Gráfico rolante com o próprio ImDrawList (mais leve que PlotLines).
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float w = avail.x, h = 60.0f;
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = { p0.x + w, p0.y + h };
        ImDrawList* dl = ImGui::GetWindowDrawList();

        // Fundo + borda.
        dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 22, 255));
        dl->AddRect(p0, p1, IM_COL32(70, 70, 90, 255));

        float maxMs = 16.6f; // escala fixa: 16.6ms = 60 FPS
        for (float v : m_FrameTimes) maxMs = std::max(maxMs, v);
        maxMs *= 1.1f;

        // Linhas de referência de 60/30 FPS.
        auto yAt = [&](float ms) { return p1.y - (ms / maxMs) * (p1.y - p0.y); };
        dl->AddLine({ p0.x, yAt(16.6f) }, { p1.x, yAt(16.6f) }, IM_COL32(60, 140, 70, 80));
        dl->AddLine({ p0.x, yAt(33.3f) }, { p1.x, yAt(33.3f) }, IM_COL32(140, 100, 50, 80));

        // Polilinha dos últimos N samples.
        int n = (int)m_FrameTimes.size();
        float dx = n > 1 ? w / (float)(n - 1) : 0.0f;
        for (int i = 1; i < n; ++i) {
            float x0 = p0.x + (i - 1) * dx, x1 = p0.x + i * dx;
            float y0 = yAt(m_FrameTimes[i - 1]), y1 = yAt(m_FrameTimes[i]);
            ImU32 col = m_FrameTimes[i] > 16.6f ? IM_COL32(200, 90, 70, 255) : IM_COL32(90, 200, 120, 255);
            dl->AddLine({ x0, y0 }, { x1, y1 }, col, 1.5f);
        }

        ImGui::Dummy(ImVec2(w, h));
        char buf[64];
        std::snprintf(buf, sizeof(buf), "máx %.1f ms (%.0f FPS)", maxMs, 1000.0f / maxMs);
        ImGui::TextDisabled("%s", buf);
    }
    ImGui::End();
}
