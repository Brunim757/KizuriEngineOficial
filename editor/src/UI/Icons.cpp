#include "Icons.hpp"
#include <Kizuri.hpp>
#include <kizuri/core/ImGuiLayer.hpp>
#include <cmath>

namespace kizuri::editor::icons {

void Torii(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    
    
    float postW = s * 0.10f;
    float topY = p.y + s * 0.12f;
    float botY = p.y + s * 0.92f;
    float leftTopX  = p.x + s * 0.24f;
    float leftBotX  = p.x + s * 0.16f;
    float rightTopX = p.x + s * 0.76f;
    float rightBotX = p.x + s * 0.84f;

    dl->AddLine(ImVec2(leftTopX, topY), ImVec2(leftBotX, botY), color, postW);
    dl->AddLine(ImVec2(rightTopX, topY), ImVec2(rightBotX, botY), color, postW);

    
    dl->AddLine(ImVec2(p.x + s * 0.04f, topY - s * 0.02f), ImVec2(p.x + s * 0.96f, topY - s * 0.02f), color, s * 0.14f);
    
    dl->AddLine(ImVec2(leftTopX, topY + s * 0.22f), ImVec2(rightTopX, topY + s * 0.22f), color, s * 0.08f);
}

void Hierarchy(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    
    float trunkX = p.x + s * 0.18f;
    dl->AddLine(ImVec2(trunkX, p.y + s * 0.1f), ImVec2(trunkX, p.y + s * 0.9f), color, s * 0.09f);

    float ys[3] = { p.y + s * 0.22f, p.y + s * 0.52f, p.y + s * 0.82f };
    float lens[3] = { s * 0.5f, s * 0.36f, s * 0.62f };
    for (int i = 0; i < 3; ++i) {
        dl->AddLine(ImVec2(trunkX, ys[i]), ImVec2(trunkX + lens[i], ys[i]), color, s * 0.08f);
        dl->AddCircleFilled(ImVec2(trunkX + lens[i], ys[i]), s * 0.06f, color);
    }
}

void Viewport(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    ImVec2 bodyMin(p.x + s * 0.08f, p.y + s * 0.30f);
    ImVec2 bodyMax(p.x + s * 0.92f, p.y + s * 0.82f);
    dl->AddRect(bodyMin, bodyMax, color, s * 0.08f, 0, s * 0.07f);

    ImVec2 viewMin(p.x + s * 0.30f, p.y + s * 0.14f);
    ImVec2 viewMax(p.x + s * 0.62f, p.y + s * 0.32f);
    dl->AddRectFilled(viewMin, viewMax, color, s * 0.04f);

    ImVec2 center((bodyMin.x + bodyMax.x) * 0.5f, (bodyMin.y + bodyMax.y) * 0.5f + s * 0.02f);
    dl->AddCircle(center, s * 0.16f, color, 0, s * 0.06f);
}

void Inspector(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    
    float xs[3]     = { s * 0.62f, s * 0.30f, s * 0.74f };
    float ys[3]     = { p.y + s * 0.20f, p.y + s * 0.5f, p.y + s * 0.80f };
    for (int i = 0; i < 3; ++i) {
        dl->AddLine(ImVec2(p.x + s * 0.08f, ys[i]), ImVec2(p.x + s * 0.92f, ys[i]), color, s * 0.045f);
        dl->AddCircleFilled(ImVec2(p.x + xs[i], ys[i]), s * 0.09f, color);
    }
}

void Console(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    
    ImVec2 bodyMin(p.x + s * 0.06f, p.y + s * 0.12f);
    ImVec2 bodyMax(p.x + s * 0.94f, p.y + s * 0.88f);
    dl->AddRect(bodyMin, bodyMax, color, s * 0.06f, 0, s * 0.07f);

    float promptY = p.y + s * 0.5f;
    dl->AddLine(ImVec2(p.x + s * 0.20f, promptY - s * 0.12f), ImVec2(p.x + s * 0.32f, promptY), color, s * 0.08f);
    dl->AddLine(ImVec2(p.x + s * 0.20f, promptY + s * 0.12f), ImVec2(p.x + s * 0.32f, promptY), color, s * 0.08f);
    dl->AddLine(ImVec2(p.x + s * 0.38f, promptY + s * 0.12f), ImVec2(p.x + s * 0.58f, promptY + s * 0.12f), color, s * 0.08f);
}

void Folder(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    float tabW = s * 0.34f;
    float tabH = s * 0.12f;
    float bodyTop = p.y + s * 0.28f;
    ImVec2 bodyMin(p.x + s * 0.06f, bodyTop);
    ImVec2 bodyMax(p.x + s * 0.94f, p.y + s * 0.86f);

    dl->AddLine(ImVec2(bodyMin.x, bodyTop), ImVec2(bodyMin.x, bodyTop - tabH), color, s * 0.07f);
    dl->AddLine(ImVec2(bodyMin.x, bodyTop - tabH), ImVec2(p.x + s * 0.06f + tabW, bodyTop - tabH), color, s * 0.07f);
    dl->AddLine(ImVec2(p.x + s * 0.06f + tabW, bodyTop - tabH), ImVec2(p.x + s * 0.06f + tabW + s * 0.1f, bodyTop), color, s * 0.07f);

    dl->AddRect(bodyMin, bodyMax, color, s * 0.05f, 0, s * 0.07f);
}

void Play(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    dl->AddTriangleFilled(ImVec2(p.x + s * 0.32f, p.y + s * 0.14f),
                          ImVec2(p.x + s * 0.32f, p.y + s * 0.86f),
                          ImVec2(p.x + s * 0.90f, p.y + s * 0.50f), color);
}

void Stop(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    dl->AddRectFilled(ImVec2(p.x + s * 0.26f, p.y + s * 0.26f),
                      ImVec2(p.x + s * 0.74f, p.y + s * 0.74f), color, s * 0.06f);
}

void Move(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.5f;
    float lw = s * 0.10f;
    dl->AddLine(ImVec2(p.x + s * 0.18f, cy), ImVec2(p.x + s * 0.82f, cy), color, lw);
    dl->AddLine(ImVec2(cx, p.y + s * 0.18f), ImVec2(cx, p.y + s * 0.82f), color, lw);

    float ah = s * 0.09f; 
    float al = s * 0.15f; 
    dl->AddTriangleFilled(ImVec2(p.x + s * 0.92f, cy),
                          ImVec2(p.x + s * 0.92f - al, cy - ah),
                          ImVec2(p.x + s * 0.92f - al, cy + ah), color);
    dl->AddTriangleFilled(ImVec2(p.x + s * 0.08f, cy),
                          ImVec2(p.x + s * 0.08f + al, cy - ah),
                          ImVec2(p.x + s * 0.08f + al, cy + ah), color);
    dl->AddTriangleFilled(ImVec2(cx, p.y + s * 0.08f),
                          ImVec2(cx - ah, p.y + s * 0.08f + al),
                          ImVec2(cx + ah, p.y + s * 0.08f + al), color);
    dl->AddTriangleFilled(ImVec2(cx, p.y + s * 0.92f),
                          ImVec2(cx - ah, p.y + s * 0.92f - al),
                          ImVec2(cx + ah, p.y + s * 0.92f - al), color);
}

void Rotate(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    float cx = p.x + s * 0.5f;
    float cy = p.y + s * 0.52f;
    float r = s * 0.28f;
    dl->AddCircle(ImVec2(cx, cy), r, color, 24, s * 0.10f);

    float ang = -0.85f; 
    float ax = cx + r * std::cos(ang);
    float ay = cy + r * std::sin(ang);
    ImVec2 dir(std::cos(ang), std::sin(ang));  
    ImVec2 tan(-std::sin(ang), std::cos(ang)); 
    ImVec2 apex(ax + dir.x * s * 0.16f, ay + dir.y * s * 0.16f);
    ImVec2 b1(ax + dir.x * s * 0.04f - tan.x * s * 0.12f, ay + dir.y * s * 0.04f - tan.y * s * 0.12f);
    ImVec2 b2(ax + dir.x * s * 0.04f + tan.x * s * 0.12f, ay + dir.y * s * 0.04f + tan.y * s * 0.12f);
    dl->AddTriangleFilled(apex, b1, b2, color);
}

void Scale(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    float lw = s * 0.10f;
    dl->AddRect(ImVec2(p.x + s * 0.18f, p.y + s * 0.18f),
                ImVec2(p.x + s * 0.82f, p.y + s * 0.82f), color, s * 0.05f, 0, lw);
    dl->AddLine(ImVec2(p.x + s * 0.32f, p.y + s * 0.68f),
                ImVec2(p.x + s * 0.68f, p.y + s * 0.32f), color, lw);
    dl->AddTriangleFilled(ImVec2(p.x + s * 0.82f, p.y + s * 0.18f),
                          ImVec2(p.x + s * 0.82f - s * 0.16f, p.y + s * 0.18f),
                          ImVec2(p.x + s * 0.82f, p.y + s * 0.18f + s * 0.16f), color);
}

void PanelHeader(const char* label, IconFn icon) {
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 cursor = ImGui::GetCursorScreenPos();
    const float iconSize = 15.0f;
    ImU32 accent = ImGui::GetColorU32(ImVec4(0.82f, 0.24f, 0.27f, 1.0f));

    icon(dl, ImVec2(cursor.x, cursor.y + 1.0f), iconSize, accent);
    ImGui::Dummy(ImVec2(iconSize + 6.0f, iconSize));
    ImGui::SameLine();

    ImFont* boldFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Bold);
    ImGui::PushFont(boldFont);
    ImGui::TextUnformatted(label);
    ImGui::PopFont();

    ImGui::Spacing();
    ImGui::Separator();
    ImGui::Spacing();
}

void Maximize(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    dl->AddRect(ImVec2(p.x + s * 0.16f, p.y + s * 0.16f), ImVec2(p.x + s * 0.84f, p.y + s * 0.84f), color, 0, 0, s * 0.09f);
    const ImU32 c = color;
    
    dl->AddLine(ImVec2(p.x + s * 0.30f, p.y + s * 0.16f), ImVec2(p.x + s * 0.16f, p.y + s * 0.30f), c, s * 0.09f);
    dl->AddLine(ImVec2(p.x + s * 0.16f, p.y + s * 0.16f), ImVec2(p.x + s * 0.30f, p.y + s * 0.16f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.16f, p.y + s * 0.16f), ImVec2(p.x + s * 0.16f, p.y + s * 0.30f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.70f, p.y + s * 0.16f), ImVec2(p.x + s * 0.84f, p.y + s * 0.30f), c, s * 0.09f);
    dl->AddLine(ImVec2(p.x + s * 0.84f, p.y + s * 0.16f), ImVec2(p.x + s * 0.70f, p.y + s * 0.16f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.84f, p.y + s * 0.16f), ImVec2(p.x + s * 0.84f, p.y + s * 0.30f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.30f, p.y + s * 0.84f), ImVec2(p.x + s * 0.16f, p.y + s * 0.70f), c, s * 0.09f);
    dl->AddLine(ImVec2(p.x + s * 0.16f, p.y + s * 0.84f), ImVec2(p.x + s * 0.16f, p.y + s * 0.70f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.16f, p.y + s * 0.84f), ImVec2(p.x + s * 0.30f, p.y + s * 0.84f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.70f, p.y + s * 0.84f), ImVec2(p.x + s * 0.84f, p.y + s * 0.70f), c, s * 0.09f);
    dl->AddLine(ImVec2(p.x + s * 0.84f, p.y + s * 0.84f), ImVec2(p.x + s * 0.70f, p.y + s * 0.84f), c, s * 0.06f);
    dl->AddLine(ImVec2(p.x + s * 0.84f, p.y + s * 0.84f), ImVec2(p.x + s * 0.84f, p.y + s * 0.70f), c, s * 0.06f);
}

void Settings(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    
    ImVec2 c(p.x + s * 0.5f, p.y + s * 0.5f);
    const float r = s * 0.22f;
    const int teeth = 8;
    for (int i = 0; i < teeth; ++i) {
        float a0 = (float)i / teeth * 6.2831853f;
        float a1 = a0 + 6.2831853f / teeth;
        ImVec2 in0(c.x + cosf(a0) * r, c.y + sinf(a0) * r);
        ImVec2 in1(c.x + cosf(a1) * r, c.y + sinf(a1) * r);
        ImVec2 out0(c.x + cosf(a0) * (r + s * 0.08f), c.y + sinf(a0) * (r + s * 0.08f));
        ImVec2 out1(c.x + cosf(a1) * (r + s * 0.08f), c.y + sinf(a1) * (r + s * 0.08f));
        dl->AddQuadFilled(in0, in1, out1, out0, color);
    }
    dl->AddCircle(c, r + s * 0.08f, color, teeth * 2, s * 0.06f);
    dl->AddCircleFilled(c, s * 0.10f, color);
}

} 
