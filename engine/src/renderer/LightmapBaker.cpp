#include "kizuri/renderer/LightmapBaker.hpp"
#include "kizuri/core/Log.hpp"
#include <glm/gtc/random.hpp>
#include <cmath>
#include <algorithm>
#include <limits>

namespace kizuri {



static float VertexAO(const glm::vec3& posWorld, const glm::vec3& normalWorld,
                      float radius, uint32_t samples, float seed,
                      const std::function<float(const glm::vec3&, const glm::vec3&)>& trace) {
    int hits = 0;
    glm::vec3 n = glm::normalize(normalWorld);
    
    glm::vec3 t = glm::normalize(glm::cross(n, std::abs(n.y) < 0.99f ? glm::vec3(0.f, 1.f, 0.f) : glm::vec3(1.f, 0.f, 0.f)));
    glm::vec3 b = glm::cross(n, t);

    (void)seed; 
    for (uint32_t i = 0; i < samples; ++i) {
        
        glm::vec2 rnd = glm::diskRand(1.0f);
        float z = glm::sqrt(glm::max(1.0f - rnd.x * rnd.x - rnd.y * rnd.y, 0.0f));
        glm::vec3 dir = glm::normalize(t * rnd.x + b * rnd.y + n * z);
        float d = trace(posWorld + n * 0.02f, dir);
        if (d >= 0.0f && d <= radius) ++hits;
    }
    return 1.0f - (float)hits / (float)std::max(samples, 1u);
}

Ref<Texture2D> LightmapBaker::Bake(const Input& in, const TraceFn& trace) {
    if (!in.Positions || !in.Normals || !in.TexCoords || !in.Indices ||
        in.Positions->empty() || in.Indices->size() < 3 || !trace) {
        return nullptr;
    }
    const auto& pos = *in.Positions;
    const auto& nrm = *in.Normals;
    const auto& uvs = *in.TexCoords;
    const auto& idxs = *in.Indices;

    
    std::vector<float> vertexAO(pos.size(), 1.0f);
    for (size_t v = 0; v < pos.size(); ++v) {
        vertexAO[v] = VertexAO(pos[v], nrm[v], in.AORadius, in.SampleRays,
                               (float)(v + 1) * 0.6180339887f, trace);
    }

    
    
    const uint32_t W = kLightmapSize, H = kLightmapSize;
    std::vector<float> accum(W * H, 0.0f);
    std::vector<float> weight(W * H, 0.0f);

    float sunDotMax = 0.0f;
    for (size_t k = 0; k + 2 < idxs.size(); k += 3) {
        uint32_t i0 = idxs[k], i1 = idxs[k + 1], i2 = idxs[k + 2];
        if (i0 >= pos.size() || i1 >= pos.size() || i2 >= pos.size()) continue;

        glm::vec2 uv0 = uvs[i0], uv1 = uvs[i1], uv2 = uvs[i2];
        
        float minU = std::min({ uv0.x, uv1.x, uv2.x });
        float maxU = std::max({ uv0.x, uv1.x, uv2.x });
        float minV = std::min({ uv0.y, uv1.y, uv2.y });
        float maxV = std::max({ uv0.y, uv1.y, uv2.y });
        int x0 = (int)(minU * W) - 1, x1 = (int)(maxU * W) + 1;
        int y0 = (int)(minV * H) - 1, y1 = (int)(maxV * H) + 1;
        if (x1 - x0 > (int)W) x0 = 0, x1 = (int)W - 1; 
        if (y1 - y0 > (int)H) y0 = 0, y1 = (int)H - 1;
        x0 = glm::clamp(x0, 0, (int)W - 1);
        x1 = glm::clamp(x1, 0, (int)W - 1);
        y0 = glm::clamp(y0, 0, (int)H - 1);
        y1 = glm::clamp(y1, 0, (int)H - 1);

        
        float sun0 = glm::max(glm::dot(glm::normalize(nrm[i0]), -in.SunDir), 0.0f);
        float sun1 = glm::max(glm::dot(glm::normalize(nrm[i1]), -in.SunDir), 0.0f);
        float sun2 = glm::max(glm::dot(glm::normalize(nrm[i2]), -in.SunDir), 0.0f);
        sunDotMax = std::max({ sunDotMax, sun0, sun1, sun2 });

        for (int ty = y0; ty <= y1; ++ty) {
            for (int tx = x0; tx <= x1; ++tx) {
                float u = ((float)tx + 0.5f) / (float)W;
                float v = ((float)ty + 0.5f) / (float)H;
                glm::vec2 p{ u, v };
                
                glm::vec2 d1 = uv1 - uv0, d2 = uv2 - uv0;
                glm::vec2 pp = p - uv0;
                float det = d1.x * d2.y - d2.x * d1.y;
                if (std::abs(det) < 1e-9f) continue;
                float b1 = (pp.x * d2.y - d2.x * pp.y) / det;
                float b2 = (d1.x * pp.y - pp.x * d1.y) / det;
                float b0 = 1.0f - b1 - b2;
                if (b0 < -0.01f || b1 < -0.01f || b2 < -0.01f || b0 > 1.01f || b1 > 1.01f || b2 > 1.01f)
                    continue;

                float ao = b0 * vertexAO[i0] + b1 * vertexAO[i1] + b2 * vertexAO[i2];
                float sun = b0 * sun0 + b1 * sun1 + b2 * sun2;
                float direct = sun * (0.55f + 0.45f * ao); 
                float value = in.SkyAmbient * ao + direct;
                accum[ty * W + tx] += value;
                weight[ty * W + tx] += 1.0f;
            }
        }
    }

    
    std::vector<uint8_t> pixels(W * H * 4);
    float maxV = 0.0f;
    for (size_t i = 0; i < accum.size(); ++i)
        if (weight[i] > 0.0f) maxV = glm::max(maxV, accum[i] / weight[i]);
    if (maxV <= 0.0f) maxV = 1.0f;
    for (size_t i = 0; i < accum.size(); ++i) {
        float value = weight[i] > 0.0f ? accum[i] / weight[i] : in.SkyAmbient;
        uint8_t c = (uint8_t)(glm::clamp(value, 0.0f, 1.0f) * 255.0f);
        pixels[i * 4 + 0] = c;
        pixels[i * 4 + 1] = c;
        pixels[i * 4 + 2] = c;
        pixels[i * 4 + 3] = 255;
    }

    auto tex = Texture2D::Create(W, H);
    tex->SetData(pixels.data(), (uint32_t)pixels.size());
    KZ_CORE_INFO("Lightmap: {0} triângulos rasterizados ({1}x{1}).", (size_t)idxs.size() / 3, W);
    return tex;
}

} 