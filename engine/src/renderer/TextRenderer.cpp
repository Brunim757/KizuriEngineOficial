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

    // Bloco 1: ASCII imprimível 32..126 (95 glifos).
    constexpr int kAsciiFirst = 32;
    constexpr int kAsciiCount = 95;
    // Bloco 2: Latin-1 0xA0..0xFF (96 glifos) — ç, ã, é, í, ó, ô, ê, °, ±…
    constexpr int kLatinFirst = 0xA0;
    constexpr int kLatinCount = 0x100 - 0xA0;

    Ref<Texture2D> s_AtlasTexture;
    stbtt_bakedchar s_AsciiBaked[kAsciiCount];
    stbtt_bakedchar s_LatinBaked[kLatinCount];
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
    int DecodeGlyphIndex(const char*& p, const char* end) {
        unsigned char c = (unsigned char)*p;
        if (c >= 0x20 && c <= 0x7E) { return (int)(c - kAsciiFirst); }
        if ((c & 0xE0) == 0xC0 && end - p >= 2) { // 2-byte: U+0080..U+07FF
            unsigned char c2 = (unsigned char)p[1];
            if ((c2 & 0xC0) == 0x80) {
                unsigned int cp = ((c & 0x1Fu) << 6) | (c2 & 0x3Fu);
                if (cp >= (unsigned int)kLatinFirst && cp <= 0xFF) {
                    p += 2;
                    return kAsciiCount + (int)(cp - kLatinFirst);
                }
            }
        }
        ++p;
        return -1; // fora do conjunto: pula o caractere
    }

    // Retorna true se coube TUDO (ou só uma parte aceitável).
    bool BakeBlock(const unsigned char* font, int pixelHeight, unsigned char* bitmap,
                   int atlasWidth, int atlasHeight, int firstChar, int numChars,
                   stbtt_bakedchar* out, int* bakedCount) {
        int r = stbtt_BakeFontBitmap(font, 0, (float)pixelHeight, bitmap,
                                     atlasWidth, atlasHeight, firstChar, numChars, out);
        // stbtt: positivo = primeira linha livre; negativo = -nº de glifos que couberam.
        if (r >= 0) { *bakedCount = numChars; return true; }
        *bakedCount = -r;
        if (*bakedCount <= 0) {
            KZ_CORE_ERROR("TextRenderer: bake falhou pra faixa [{0}..{1}] (couberam {2}).",
                          firstChar, firstChar + numChars - 1, *bakedCount);
            return false;
        }
        KZ_CORE_WARN("TextRenderer: só {0}/{1} glifos couberam no atlas; o resto fica pendente.",
                     *bakedCount, numChars);
        return true;
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

    s_Bitmap.assign(kAtlasWidth * kAtlasHeight, 0);

    // ASCII na metade SUPERIOR (y = 0..H/2) e Latin na INFERIOR (y = H/2..H):
    // dois bakes independentes no MESMO bitmap, cada um partindo de (1,1)
    // do seu quadrante — sem colisão entre os dois blocos.
    const char* font = (const char*)kizuri::embedded::kFontRegularTTF;
    const size_t fontLen = (size_t)kizuri::embedded::kFontRegularTTF_size;
    if (!font || fontLen < 4) {
        KZ_CORE_ERROR("TextRenderer: fonte embutida vazia — texto desativado.");
        return;
    }
    // stbtt espera o TTF; valida magic via InitFont (feito dentro do bake).

    int bakedAscii = 0;
    bool okAscii = BakeBlock((const unsigned char*)font, kPixelHeight,
                             s_Bitmap.data(), kAtlasWidth, kAtlasHeight,
                             kAsciiFirst, kAsciiCount, s_AsciiBaked, &bakedAscii);
    if (!okAscii) { s_Ready = false; return; }

    // Bloco Latin: aponta o bake pro quadrante de baixo (y offset = H/2).
    // O stbtt sempre começa em (1,1) — então zeramos a metade de baixo e
    // ajustamos as coodenadas Y do bakedchar após o bake.
    std::memset(s_Bitmap.data() + kAtlasHeight / 2 * kAtlasWidth, 0,
                kAtlasHeight / 2 * kAtlasWidth);
    // Simula "começar em H/2+1": usamos um subview: o stbtt precisa do
    // ponteiro do bitmap e do tamanho; fazemos bake numa SEPARADA e colamos.
    std::vector<uint8_t> latinBit(kAtlasWidth * (kAtlasHeight / 2), 0);
    int latinBakedInto = 0;
    {
        int r = stbtt_BakeFontBitmap((const unsigned char*)font, 0, (float)kPixelHeight,
                                     latinBit.data(), kAtlasWidth, kAtlasHeight / 2,
                                     kLatinFirst, kLatinCount, s_LatinBaked);
        if (r >= 0) latinBakedInto = kLatinCount;
        else { latinBakedInto = -r; }
        if (latinBakedInto > 0) {
            std::memcpy(s_Bitmap.data() + kAtlasHeight / 2 * kAtlasWidth,
                        latinBit.data(), kAtlasWidth * (kAtlasHeight / 2));
            // Eleva as coodenadas Y do Latin pro quadrante de baixo.
            for (int i = 0; i < latinBakedInto; ++i) {
                s_LatinBaked[i].y0 = (short)(s_LatinBaked[i].y0 + kAtlasHeight / 2);
                s_LatinBaked[i].y1 = (short)(s_LatinBaked[i].y1 + kAtlasHeight / 2);
            }
        } else {
            KZ_CORE_WARN("TextRenderer: faixa Latin-1 não coube no quadrante; acentos ficam fora.");
        }
    }

    // Expande 1 canal (alfa) → RGBA (branco + alfa), como a engine exige.
    std::vector<uint8_t> rgba(kAtlasWidth * kAtlasHeight * 4);
    for (size_t i = 0; i < s_Bitmap.size(); ++i) {
        rgba[i * 4 + 0] = 255;
        rgba[i * 4 + 1] = 255;
        rgba[i * 4 + 2] = 255;
        rgba[i * 4 + 3] = s_Bitmap[i];
    }

    s_AtlasTexture = Texture2D::Create(kAtlasWidth, kAtlasHeight);
    s_AtlasTexture->SetData(rgba.data(), (uint32_t)rgba.size());
    s_Ready = true;
#ifdef KZ_DEBUG
    // Diagnóstico de desenvolvimento: muda o atlas pra PNG no cwd do editor
    // (só em build Debug) — se o texto sair esquisito, a imagem mostra na
    // hora se o bake/upload está certo.
    Texture2D::SaveToFile(s_AtlasTexture, "kizuri_debug_text_atlas.png");
#endif
    KZ_CORE_INFO("TextRenderer: atlas {0}x{1} pronto (ASCII {2}/{3} + Latin-1 {4}/{5}).",
                 kAtlasWidth, kAtlasHeight, bakedAscii, kAsciiCount,
                 latinBakedInto > 0 ? latinBakedInto : 0, kLatinCount);
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
        if (idx < kAsciiCount) lineWidth += s_AsciiBaked[idx].xadvance * scale;
        else if (idx - kAsciiCount < kLatinCount) lineWidth += s_LatinBaked[idx - kAsciiCount].xadvance * scale;
    }
    return glm::max(maxWidth, lineWidth);
}

bool TextRenderer::IsReady() { return s_Ready; }
Ref<Texture2D> TextRenderer::GetAtlasTexture() { return s_AtlasTexture; }
std::string TextRenderer::GetDiagnostics() {
    if (!s_Ready) return "atlas: NAO PRONTO";
    return "atlas: " + std::to_string(kAtlasWidth) + "x" + std::to_string(kAtlasHeight) +
           " (ASCII " + std::to_string(kAsciiCount) + " + Latin-1 " + std::to_string(kLatinCount) + ")";
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

    // Atalho local pros two tables (evita indexação dupla no loop quente).
    const stbtt_bakedchar* ascii = s_AsciiBaked;
    const stbtt_bakedchar* latin = s_LatinBaked;
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
            if (idx < kAsciiCount) b = &ascii[idx];
            else if (idx - kAsciiCount < kLatinCount) b = &latin[idx - kAsciiCount];
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