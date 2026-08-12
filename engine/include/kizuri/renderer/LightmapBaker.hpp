#pragma once
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace kizuri {

// Baked lighting (pilar AAA v0.35): gera uma textura de lightmap por malha
// com AO por vértice (hemisfério de raios vs a geometria da cena) +
// contribuição direta do sol, rasterizada por software no atlas de UVs.
// Multiplicada no fragment shader do PBR — sombras que não mexem (o look
// "assado" dos jogos AAA) a um custo de bake de poucos segundos.
class LightmapBaker {
public:
    // Callback de colisão do bake: 'origin'->'dir' e devolve a distância até
    // a geometria (ou -1 se não acertou nada). A cena fornece.
    using TraceFn = std::function<float(const glm::vec3& origin, const glm::vec3& dir)>;

    // Tamanho da lightmap (texels por eixo do atlas de UVs).
    static constexpr uint32_t kLightmapSize = 128;

    struct Input {
        const std::vector<glm::vec3>* Positions = nullptr; // local
        const std::vector<glm::vec3>* Normals = nullptr;   // local
        const std::vector<glm::vec2>* TexCoords = nullptr;
        const std::vector<uint32_t>* Indices = nullptr;
        glm::vec3 SunDir{ 0.3f, -0.9f, -0.25f }; // direção DA qual a luz vem (normalizada)
        glm::vec3 SunColor{ 1.0f, 0.95f, 0.85f };
        float SkyAmbient = 0.12f;                // luz ambiente do céu (procedural)
        uint32_t SampleRays = 12;                // raios de AO por vértice
        float AORadius = 4.0f;                   // alcance da oclusão
    };

    // Devolve a textura RGBA da lightmap (r=g=b = AO*ambient + sol direto).
    static Ref<Texture2D> Bake(const Input& in, const TraceFn& trace);
};

} // namespace kizuri