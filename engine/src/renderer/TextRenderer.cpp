#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>
#include <chrono>

#include <glm/gtc/matrix_transform.hpp>

// Fonte embutida (JetBrains Mono) — gerada pelo CMake em tempo de build
// (EmbedResource.cmake) e compilada como array de bytes na engine.
#include <EmbeddedFontRegular.hpp>

#define STB_TRUETYPE_IMPLEMENTATION
#include <stb_truetype.h>

namespace kizuri {

namespace {
    // Atlas 1024x1024 dá folga pra CABER os dois blocos (ASCII + Latin-1);
    // em 512x512 o bake pode estourar (retorno negativo = só uma parte
    // coube) e os glifos que sobraram ficavam com caixa vazia (o sintoma
    // clássico de "letra que some / vira retângulo").
    constexpr int kAtlasWidth = 1024;
    constexpr int kAtlasHeight = 1024;
    constexpr int kPixelHeight = 48;   // altura do bake (px)


    // Bloco 3: pontuação tipográfica comum em 3 bytes (— “ ” ‘ ’ … « »).
    // A JetBrains Mono tem esses glifos; sem eles o travessão da demo 2D
    // (e textos com aspas curvas) sumiria/viraria retângulo.
    // Atlas ÚNICO e CONTÍNUO: 32..255 (ASCII + Latin-1 — ç ã é ê etc).
    // Um único bake (stbtt_BakeFontBitmap), sem ranges/hacks: é o caminho
    // mais simples e à prova de erro do stb.
    constexpr int kGlyphFirst = 32;
    constexpr int kGlyphCount = 0xFF - 32 + 1;   // 224 glifos

    Ref<Texture2D> s_AtlasTexture;
    stbtt_bakedchar s_Baked[kGlyphCount];
    bool s_Ready = false;

    // Uma única textura 1024² com os DOIS blocos: ASCII é bakes do canto
    // (0,0) e Latin entra logo em seguida (o range 0xA0 usa o mesmo atlas,
    // bake sequencial — stbtt empacota a partir de onde parou? Não:
    // BakeFontBitmap reinicia de (1,1)! Então os blocos são bakes numa
    // bitmap 1024² COMBINADA via dois buffers e empilhamento em y.
    // Solução robusta: bake ASCII na metade de cima, Latin na de baixo.
    std::vector<uint8_t> s_Bitmap;

    // MeasureWidth/DrawString decodifica UTF-8 (até 2 bytes — suficiente
    // pro Latin-1; emojis/3+ bytes são ignorados sem quebrar a linha).
    // Índice do glifo no atlas contínuo (32..255). Para codepoints de 3
    // bytes (— “ ” …) normaliza pra um glifo Latin-1/ASCII próximo — sem
    // glifo dedicado, sem retângulo, sem letra errada.
    int DecodeGlyphIndex(const char*& p, const char* end) {
        unsigned char c = (unsigned char)*p;
        if (c >= 0x20 && c <= 0x7E) { ++p; return (int)(c - kGlyphFirst); }
        if ((c & 0xE0) == 0xC0 && end - p >= 2) { // 2-byte: U+0080..U+07FF
            unsigned char c2 = (unsigned char)p[1];
            if ((c2 & 0xC0) == 0x80) {
                unsigned int cp = ((c & 0x1Fu) << 6) | (c2 & 0x3Fu);
                if (cp >= 0xA0 && cp <= 0xFF) {
                    p += 2;
                    return (int)(cp - kGlyphFirst);
                }
            }
        }
        if ((c & 0xF0) == 0xE0 && end - p >= 3) { // 3-byte: tipografia
            unsigned char c2 = (unsigned char)p[1], c3 = (unsigned char)p[2];
            if ((c2 & 0xC0) == 0x80 && (c3 & 0xC0) == 0x80) {
                unsigned int cp = ((c & 0x0Fu) << 12) | ((c2 & 0x3Fu) << 6) | (c3 & 0x3Fu);
                p += 3;
                switch (cp) {
                    case 0x2013: case 0x2014: case 0x2015: return '-'- kGlyphFirst; // – — ― → -
                    case 0x2018: case 0x2019: case 0x201C: case 0x201D: return '"'- kGlyphFirst; // ‘ ’ “ ” → "
                    case 0x2026: return '.'- kGlyphFirst;                                   // … → .
                    default: return -1;
                }
            }
        }
        ++p;
        return -1; // fora do conjunto: pula o caractere
    }

    // Escala determinística: px por unidade de mundo vem da projeção 2D
    // ATUAL (já aberta por BeginScene) e do tamanho do viewport REAL.
    // Casos degenerados caem num fallback saninho em vez de explodir
    // (o que gerava os "retângulos brancos" gigantes no editor).
    float ComputeWorldScale(float fontSize) {
        float worldHeight = 2.0f / glm::max(glm::abs(Renderer2D::GetViewProjection()[1][1]), 0.1f);
        GLint vp[4];
        glGetIntegerv(GL_VIEWPORT, vp);
        float pxPerUnit = (float)vp[3] / glm::max(worldHeight, 1e-3f);
        float fallback = (fontSize / (float)kPixelHeight) * 0.02f;
        if (!(pxPerUnit > 0.01f)) return fallback;
        float scale = (fontSize / (float)kPixelHeight) / pxPerUnit;
        if (!(scale > 0.0f) || scale > 100.0f) return fallback; // quad gigante = bug de projeção
        return scale;
    }
}

void TextRenderer::EnsureAtlas() {
    if (s_Ready) return;

    const char* font = (const char*)kizuri::embedded::kFontRegularTTF;
    const size_t fontLen = (size_t)kizuri::embedded::kFontRegularTTF_size;
    if (!font || fontLen < 4) {
        KZ_CORE_ERROR("TextRenderer: fonte embutida vazia — texto desativado.");
        return;
    }

    // UM único bake contínuo (32..255) — o mecanismo mais simples do stb.
    // Sem ranges, sem oversampling, sem quadrantes: x0/y0..x1/y1 do
    // bakedchar apontam direto pros glifos no atlas.
    std::vector<uint8_t> bitmap(kAtlasWidth * kAtlasHeight, 0);
    int baked = stbtt_BakeFontBitmap((const unsigned char*)font, 0, (float)kPixelHeight,
                                     bitmap.data(), kAtlasWidth, kAtlasHeight,
                                     kGlyphFirst, kGlyphCount, s_Baked);
    if (baked <= 0) {
        KZ_CORE_ERROR("TextRenderer: bake do atlas falhou (couberam {0}).", baked);
        return;
    }
    if (baked < kGlyphCount)
        KZ_CORE_WARN("TextRenderer: só {0}/{1} glifos couberam no atlas.", baked, kGlyphCount);

    // Expande 1 canal (alfa) → RGBA (branco + alfa), como a engine exige.
    std::vector<uint8_t> rgba(kAtlasWidth * kAtlasHeight * 4);
    for (size_t i = 0; i < bitmap.size(); ++i) {
        rgba[i * 4 + 0] = 255; rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255; rgba[i * 4 + 3] = bitmap[i];
    }

    s_AtlasTexture = Texture2D::Create(kAtlasWidth, kAtlasHeight);
    s_AtlasTexture->SetData(rgba.data(), (uint32_t)rgba.size());
    s_Ready = true;
    KZ_CORE_INFO("TextRenderer: atlas {0}x{1} pronto ({2}/{3} glifos, faixa 32..255).",
                 kAtlasWidth, kAtlasHeight, baked, kGlyphCount);
}

void TextRenderer::Init() {
    EnsureAtlas();
}

void TextRenderer::Shutdown() {
    s_AtlasTexture = nullptr;
    s_Bitmap.clear();
    s_Bitmap.shrink_to_fit();
    s_Ready = false;
}

float TextRenderer::MeasureWidth(const std::string& text, float fontSize) {
    if (!s_Ready) EnsureAtlas();
    if (!s_Ready) return 0.0f;
    float scale = ComputeWorldScale(fontSize);
    float maxWidth = 0.0f, lineWidth = 0.0f;
    const char* p = text.data();
    const char* end = p + text.size();
    while (p < end) {
        if (*p == '\n') { maxWidth = glm::max(maxWidth, lineWidth); lineWidth = 0.0f; ++p; continue; }
        const char* before = p;
        int idx = DecodeGlyphIndex(p, end);
        if (idx < 0) continue;
        if (p == before) ++p; // avanço garantido (proteção anti-loop)
        if (idx < kGlyphCount) lineWidth += s_Baked[idx].xadvance * scale;
    }
    return glm::max(maxWidth, lineWidth);
}

bool TextRenderer::IsReady() { return s_Ready; }
Ref<Texture2D> TextRenderer::GetAtlasTexture() { return s_AtlasTexture; }
std::string TextRenderer::GetDiagnostics() {
    if (!s_Ready) return "atlas: NAO PRONTO";
    return "atlas: " + std::to_string(kAtlasWidth) + "x" + std::to_string(kAtlasHeight) +
           " (" + std::to_string(kGlyphCount) + " glifos, faixa 32..255)";
}

void TextRenderer::DrawString(const std::string& text, const glm::vec3& position,
                              float fontSize, const glm::vec4& color, TextAlignment alignment) {
    if (!s_Ready) EnsureAtlas();
    if (!s_Ready || text.empty()) return;

    // Diagnóstico (v0.37.x): bytes que não são UTF-8 válido geram "letras
    // estranhas" ou glifos ausentes. Loga uma vez por 10s com os bytes em
    // hex pra identificar a origem (encoding errado no .cs / import).
    static int64_t s_LastDiag = 0;
    {
        auto nowMs = (int64_t)std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        if (nowMs - s_LastDiag > 10000) {
            const unsigned char* q = (const unsigned char*)text.data();
            const unsigned char* qe = q + text.size();
            while (q < qe) {
                unsigned char c = *q;
                bool ok = (c < 0x80) ||
                          ((c & 0xE0) == 0xC0 && q + 1 < qe && (q[1] & 0xC0) == 0x80) ||
                          ((c & 0xF0) == 0xE0 && q + 2 < qe && (q[1] & 0xC0) == 0x80 && (q[2] & 0xC0) == 0x80);
                if (!ok) {
                    s_LastDiag = nowMs;
                    char hex[64]; int n = 0;
                    for (int i = 0; i < 8 && q + i < qe; ++i) n += snprintf(hex + n, sizeof(hex) - n, "%02X ", q[i]);
                    KZ_CORE_WARN("Texto com bytes não-UTF8 (ex: {0}) — o arquivo de script/import pode estar em outro encoding. Texto: {1}",
                                 hex, text.substr(0, 60));
                    break;
                }
                if ((c & 0xE0) == 0xC0) q += 2;
                else if ((c & 0xF0) == 0xE0) q += 3;
                else ++q;
            }
        }
    }

    // Blindagem: o texto PRECISA de alpha blending. Qualquer passe que tenha
    // deixado o GL_BLEND desligado (ex.: decals/partículas do 3D) virava os
    // glifos em retângulos brancos opacos — força aqui, no desenho.
    RenderCommand::SetBlending(true);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // Divide o texto em linhas e desenha cada uma — posição é o canto
    // esquerdo-superior da primeira linha; alinhamento desloca cada linha.
    std::vector<std::string> lines;
    std::string current;
    for (char c : text) {
        if (c == '\n') { lines.push_back(current); current.clear(); }
        else current += c;
    }
    lines.push_back(current);

    float scale = ComputeWorldScale(fontSize);
    // Altura da linha: 1.2em (em = 1 unidade de 48px do bake).
    float lineHeight = scale * (float)kPixelHeight * 1.2f;
    if (lineHeight <= 0.0f) lineHeight = 0.01f;
    float penY = position.y;

    const float invW = 1.0f / (float)kAtlasWidth;
    const float invH = 1.0f / (float)kAtlasHeight;

    for (const std::string& line : lines) {
        float lineWidth = MeasureWidth(line, fontSize);
        float penX = position.x;
        if (alignment == TextAlignment::Center) penX -= lineWidth * 0.5f;
        else if (alignment == TextAlignment::Right) penX -= lineWidth;

        const char* p = line.data();
        const char* end = p + line.size();
        while (p < end) {
            if (*p == '\n') { ++p; continue; }
            const char* before = p;
            int idx = DecodeGlyphIndex(p, end);
            if (idx < 0) continue;
            if (p == before) ++p;

            const stbtt_bakedchar* b;
            if (idx < kGlyphCount) b = &s_Baked[idx];
            else continue;
            if (b->x1 <= b->x0 || b->y1 <= b->y0) { penX += b->xadvance * scale; continue; }

            float x = penX + b->xoff * scale;
            float y = penY + b->yoff * scale;
            float w = (b->x1 - b->x0) * scale;
            float h = (b->y1 - b->y0) * scale;

            // Bitmap do stbtt tem y=0 no topo; a textura GL tem v=0 embaixo.
            float u0 = b->x0 * invW;
            float u1 = b->x1 * invW;
            float v0 = 1.0f - (float)b->y1 * invH;
            float v1 = 1.0f - (float)b->y0 * invH;

            glm::mat4 transform = glm::translate(glm::mat4(1.0f), { x + w * 0.5f, y + h * 0.5f, position.z })
                                * glm::scale(glm::mat4(1.0f), { w, h, 1.0f });
            Renderer2D::DrawTransformedQuadUV(transform, s_AtlasTexture, { u0, v0 }, { u1, v1 }, color);

            penX += b->xadvance * scale;
        }
        penY += lineHeight;
    }
}

} // namespace kizuri