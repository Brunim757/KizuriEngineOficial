#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

// Alinhamento horizontal do texto — usado pelo TextComponent (HUD) e pelo
// TextRenderer::DrawString. A posição do transform é a âncora: esquerda
// (canto esquerdo), centro (meio da linha) ou direita (canto direito).
enum class TextAlignment : int { Left = 0, Center = 1, Right = 2 };

// Renderiza texto de jogo (HUD, pontuação, diálogo) com a fonte embutida
// JetBrains Mono, usando o pipeline de quads do Renderer2D com recorte de UV.
// O atlas é gerado uma vez em Init() via stb_truetype; DrawString desenha
// cada caractere como um quad texturizado do atlas (uma draw call por lote).
// Suporta múltiplas linhas: '\n' quebra a linha (altura de linha = 1.2x a
// fonte) e o alinhamento desloca cada linha em relação à posição âncora.
class TextRenderer {
public:
    // Diagnóstico (janela do editor): atlas gerado + estado de prontidão.
    static bool IsReady();
    static Ref<Texture2D> GetAtlasTexture();
    static std::string GetDiagnostics();

public:
    static void Init();
    static void Shutdown();

    // Mede a largura da LINHA MAIS LARGA do texto em pixels (multilinha) —
    // útil pra centralizar HUD com alinhamento.
    static float MeasureWidth(const std::string& text, float fontSize);

    // Desenha 'text' em posição de mundo (canto esquerdo-superior do primeiro
    // glyph), com altura de fonte em pixels de tela. Dentro de BeginScene/EndScene 2D.
    static void DrawString(const std::string& text, const glm::vec3& position,
                           float fontSize, const glm::vec4& color,
                           TextAlignment alignment = TextAlignment::Left);

private:
    static void EnsureAtlas();
};

} // namespace kizuri
