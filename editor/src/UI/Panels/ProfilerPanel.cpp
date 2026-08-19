#include "ProfilerPanel.hpp"
#include <imgui.h>
#include <cmath>
#include <cstdio>
#include <fstream>
#ifndef _WIN32
#include <unistd.h>
#endif



static double ProcessRAMMB() {
#ifdef _WIN32
    return -1.0;
#else
    std::ifstream f("/proc/self/statm");
    if (!f.is_open()) return -1.0;
    long total = 0, resident = 0;
    if (!(f >> total >> resident)) return -1.0;
    (void)total;
    long pageSize = sysconf(_SC_PAGESIZE);
    return (double)resident * (double)pageSize / (1024.0 * 1024.0);
#endif
}

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

    double ramMB = ProcessRAMMB();
    if (ramMB > 0.0)
        ImGui::Text("RAM do processo: %.0f MB", ramMB);
    ImGui::TextWrapped("GPU: %s", kizuri::GetOpenGLVersionString().c_str());

    ImGui::Separator();
    ImGui::TextDisabled("Tempo de frame (últimos %d frames)", kHistory);

    if (!m_FrameTimes.empty()) {
        
        ImVec2 avail = ImGui::GetContentRegionAvail();
        float w = avail.x, h = 60.0f;
        ImVec2 p0 = ImGui::GetCursorScreenPos();
        ImVec2 p1 = { p0.x + w, p0.y + h };
        ImDrawList* dl = ImGui::GetWindowDrawList();

        
        dl->AddRectFilled(p0, p1, IM_COL32(18, 18, 22, 255));
        dl->AddRect(p0, p1, IM_COL32(70, 70, 90, 255));

        float maxMs = 16.6f; 
        for (float v : m_FrameTimes) maxMs = std::max(maxMs, v);
        maxMs *= 1.1f;

        
        auto yAt = [&](float ms) { return p1.y - (ms / maxMs) * (p1.y - p0.y); };
        dl->AddLine({ p0.x, yAt(16.6f) }, { p1.x, yAt(16.6f) }, IM_COL32(60, 140, 70, 80));
        dl->AddLine({ p0.x, yAt(33.3f) }, { p1.x, yAt(33.3f) }, IM_COL32(140, 100, 50, 80));

        
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
