#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/core/Log.hpp"

#include <glm/gtc/matrix_transform.hpp>

// Fonte embutida (JetBrains Mono) — gerada pelo CMake em tempo de build
// (EmbedResource.cmake) e compilada como array de bytes na engine.
#include <EmbeddedFontRegular.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace kizuri {

namespace {
    constexpr int kAtlasWidth = 512;
    constexpr int kAtlasHeight = 512;
    constexpr int kPixelHeight = 64;   // altura da fonte no bake (px)
    constexpr int kFirstChar = 32;     // espaço
    constexpr int kCharCount = 95;     // 32..126 (imprimíveis ASCII básicos)

    Ref<Texture2D> s_AtlasTexture;
    stbtt_bakedchar s_BakedChars[kCharCount];
    bool s_Ready = false;
}

void TextRenderer::EnsureAtlas() {
    if (s_Ready) return;

    std::vector<uint8_t> bitmap(kAtlasWidth * kAtlasHeight, 0);
    int baked = stbtt_BakeFontBitmap(kizuri::embedded::kFontRegularTTF, 0,
        (float)kPixelHeight, bitmap.data(), kAtlasWidth, kAtlasHeight,
        kFirstChar, kCharCount, s_BakedChars);
    if (baked <= 0) {
        KZ_CORE_ERROR("TextRenderer: falha ao cozinhar o atlas da fonte (chars={0}).", baked);
        return;
    }

    // O bake gera um bitmap 1 canal (alfa); a engine só tem textura RGBA,
    // então expande pra branco + alfa.
    std::vector<uint8_t> rgba(kAtlasWidth * kAtlasHeight * 4);
    for (size_t i = 0; i < bitmap.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = bitmap[i];
    }

    s_AtlasTexture = Texture2D::Create(kAtlasWidth, kAtlasHeight);
    s_AtlasTexture->SetData(rgba.data(), (uint32_t)rgba.size());
    s_Ready = true;
    KZ_CORE_INFO("TextRenderer: atlas {0}x{1} pronto ({2} glifos).", kAtlasWidth, kAtlasHeight, baked);
}

void TextRenderer::Init() {
    EnsureAtlas();
}

void TextRenderer::Shutdown() {
    s_AtlasTexture = nullptr;
    s_Ready = false;
}

float TextRenderer::MeasureWidth(const std::string& text, float fontSize) {
    if (!s_Ready) EnsureAtlas();
    float scale = fontSize / (float)kPixelHeight;
    float maxWidth = 0.0f, lineWidth = 0.0f;
    for (char c : text) {
        if (c == '\n') { maxWidth = glm::max(maxWidth, lineWidth); lineWidth = 0.0f; continue; }
        if (c < kFirstChar || c >= kFirstChar + kCharCount) continue;
        lineWidth += s_BakedChars[c - kFirstChar].xadvance * scale;
    }
    return glm::max(maxWidth, lineWidth);
}

void TextRenderer::DrawString(const std::string& text, const glm::vec3& position,
                              float fontSize, const glm::vec4& color, TextAlignment alignment) {
    if (!s_Ready) EnsureAtlas();
    if (!s_Ready || text.empty()) return;

    // Divide o texto em linhas e desenha cada uma — posição é o canto
    // esquerdo-superior da primeira linha; alinhamento desloca cada linha.
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') { lines.push_back(current); current.clear(); }
        else current += c;
    }
    lines.push_back(current);

    float scale = fontSize / (float)kPixelHeight;
    float lineHeight = fontSize * 1.2f;
    float penY = position.y;

    for (const std::string& line : lines) {
        float lineWidth = MeasureWidth(line, fontSize);
        float penX = position.x;
        if (alignment == TextAlignment::Center) penX -= lineWidth * 0.5f;
        else if (alignment == TextAlignment::Right) penX -= lineWidth;

        for (char c : line) {
            if (c < kFirstChar || c >= kFirstChar + kCharCount) continue;
            const stbtt_bakedchar& b = s_BakedChars[c - kFirstChar];

            float x = penX + b.xoff * scale;
            float y = penY + b.yoff * scale;
            float w = (b.x1 - b.x0) * scale;
            float h = (b.y1 - b.y0) * scale;

            // Bitmap do stbtt tem y=0 no topo; a textura GL tem v=0 embaixo.
            float u0 = b.x0 / (float)kAtlasWidth;
            float u1 = b.x1 / (float)kAtlasWidth;
            float v0 = 1.0f - b.y1 / (float)kAtlasHeight;
            float v1 = 1.0f - b.y0 / (float)kAtlasHeight;

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), { x + w * 0.5f, y + h * 0.5f, position.z })
                                * glm::scale(glm::mat4(1.0f), { w, h, 1.0f });
            Renderer2D::DrawTransformedQuadUV(transform, s_AtlasTexture, { u0, v0 }, { u1, v1 }, color);

            penX += b.xadvance * scale;
        }
        penY += lineHeight;
    }
}

} // namespace kizuri
