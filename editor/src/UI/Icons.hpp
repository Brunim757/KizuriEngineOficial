#pragma once
#include <imgui.h>

// Ícones vetoriais desenhados à mão com ImDrawList, em vez de uma fonte de
// ícones de terceiros (Font Awesome etc). Duas razões: (1) evita depender
// de mais um arquivo de fonte externo só pra isso, e (2) um conjunto de
// ícones desenhado sob medida — incluindo o torii, que é a própria marca
// da Kizuri Engine — reforça a identidade visual em vez de usar o mesmo
// conjunto de ícones que qualquer outro app usa.
namespace kizuri::editor::icons {

// Marca da Kizuri Engine: um torii estilizado (portal xintoísta), usado na
// barra de título customizada. "Kizuri" remete a torii — daí o vermelho
// de destaque da paleta já ser apelidado de "vermelho torii" no código.
void Torii(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícone do painel Hierarquia: uma pequena árvore/lista hierárquica.
void Hierarchy(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícone do painel Viewport: uma câmera simplificada.
void Viewport(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícone do painel Inspetor: sliders de propriedades.
void Inspector(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícone do painel Console: prompt de terminal (">" + cursor).
void Console(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícone do painel Content Browser: uma pasta.
void Folder(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);

// Ícones do toolbar do viewport (ferramentas, não texto):
void Play(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);   // triângulo de play
void Stop(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);   // quadrado de stop
void Move(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);   // setas nas 4 direções
void Rotate(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color); // círculo com ponta
void Scale(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color);  // quadrado com diagonal
void Maximize(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color); // fullscreen (cantos)
void Settings(ImDrawList* dl, ImVec2 topLeft, float size, ImU32 color); // engrenagem

using IconFn = void(*)(ImDrawList*, ImVec2, float, ImU32);

// Desenha um cabeçalho de seção padronizado (ícone + título em destaque +
// linha separadora) no topo do conteúdo de um painel. Usado no lugar do
// título nativo simples do ImGui pra dar aos painéis uma cara própria.
void PanelHeader(const char* label, IconFn icon);

} // namespace kizuri::editor::icons
