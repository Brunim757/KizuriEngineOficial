#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

// Renderiza texto de jogo (HUD, pontuação, diálogo) com a fonte embutida
// JetBrains Mono, usando o pipeline de quads do Renderer2D com recorte de UV.
// O atlas é gerado uma vez em Init() via stb_truetype; DrawString desenha
// cada caractere como um quad texturizado do atlas (uma draw call por lote).
class TextRenderer {
public:
    static void Init();
    static void Shutdown();

    // Mede a largura do texto em pixels — útil pra centralizar HUD.
    static float MeasureWidth(const std::string& text, float fontSize);

    // Desenha 'text' em posição de mundo (esquerda-baixo do primeiro glyph),
    // com altura de fonte em pixels de tela. Dentro de BeginScene/EndScene 2D.
    static void DrawString(const std::string& text, const glm::vec3& position,
                           float fontSize, const glm::vec4& color);

private:
    static void EnsureAtlas();
};

} // namespace kizuri
