#include "Icons.hpp"
#include <Kizuri.hpp>
#include <kizuri/core/ImGuiLayer.hpp>

namespace kizuri::editor::icons {

void Torii(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    // Duas colunas (hashira) levemente inclinadas pra fora na base, e dois
    // travessões horizontais (kasagi em cima, nuki logo abaixo) — silhueta
    // clássica de torii, simplificada pra caber num ícone pequeno.
    float postW = s * 0.10f;
    float topY = p.y + s * 0.12f;
    float botY = p.y + s * 0.92f;
    float leftTopX  = p.x + s * 0.24f;
    float leftBotX  = p.x + s * 0.16f;
    float rightTopX = p.x + s * 0.76f;
    float rightBotX = p.x + s * 0.84f;

    dl->AddLine(ImVec2(leftTopX, topY), ImVec2(leftBotX, botY), color, postW);
    dl->AddLine(ImVec2(rightTopX, topY), ImVec2(rightBotX, botY), color, postW);

    // Kasagi (travessão superior, mais largo que as colunas)
    dl->AddLine(ImVec2(p.x + s * 0.04f, topY - s * 0.02f), ImVec2(p.x + s * 0.96f, topY - s * 0.02f), color, s * 0.14f);
    // Nuki (travessão secundário, mais fino, logo abaixo do kasagi)
    dl->AddLine(ImVec2(leftTopX, topY + s * 0.22f), ImVec2(rightTopX, topY + s * 0.22f), color, s * 0.08f);
}

void Hierarchy(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    // Tronco vertical com três "galhos" horizontais — remete a uma árvore
    // de hierarquia/cena (o mesmo padrão visual da lista de entidades).
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
    // Corpo de câmera simplificado: retângulo + visor saliente + lente.
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
    // Três sliders de propriedades em alturas diferentes — linguagem
    // visual comum pra "painel de propriedades" em ferramentas de dev.
    float xs[3]     = { s * 0.62f, s * 0.30f, s * 0.74f };
    float ys[3]     = { p.y + s * 0.20f, p.y + s * 0.5f, p.y + s * 0.80f };
    for (int i = 0; i < 3; ++i) {
        dl->AddLine(ImVec2(p.x + s * 0.08f, ys[i]), ImVec2(p.x + s * 0.92f, ys[i]), color, s * 0.045f);
        dl->AddCircleFilled(ImVec2(p.x + xs[i], ys[i]), s * 0.09f, color);
    }
}

void Console(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    // Moldura de terminal + prompt ">" + cursor — linguagem visual
    // universal de "isso é um log/console".
    ImVec2 bodyMin(p.x + s * 0.06f, p.y + s * 0.12f);
    ImVec2 bodyMax(p.x + s * 0.94f, p.y + s * 0.88f);
    dl->AddRect(bodyMin, bodyMax, color, s * 0.06f, 0, s * 0.07f);

    float promptY = p.y + s * 0.5f;
    dl->AddLine(ImVec2(p.x + s * 0.20f, promptY - s * 0.12f), ImVec2(p.x + s * 0.32f, promptY), color, s * 0.08f);
    dl->AddLine(ImVec2(p.x + s * 0.20f, promptY + s * 0.12f), ImVec2(p.x + s * 0.32f, promptY), color, s * 0.08f);
    dl->AddLine(ImVec2(p.x + s * 0.38f, promptY + s * 0.12f), ImVec2(p.x + s * 0.58f, promptY + s * 0.12f), color, s * 0.08f);
}

void Folder(ImDrawList* dl, ImVec2 p, float s, ImU32 color) {
    // Pasta com abinha — silhueta clássica, reconhecível em tamanho pequeno.
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

} // namespace kizuri::editor::icons
