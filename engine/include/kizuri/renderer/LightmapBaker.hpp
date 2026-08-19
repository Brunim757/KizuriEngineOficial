#pragma once
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>
#include <vector>
#include <functional>

namespace kizuri {

class LightmapBaker {
public:

    using TraceFn = std::function<float(const glm::vec3& origin, const glm::vec3& dir)>;

    static constexpr uint32_t kLightmapSize = 128;

    struct Input {
        const std::vector<glm::vec3>* Positions = nullptr;
        const std::vector<glm::vec3>* Normals = nullptr;
        const std::vector<glm::vec2>* TexCoords = nullptr;
        const std::vector<uint32_t>* Indices = nullptr;
        glm::vec3 SunDir{ 0.3f, -0.9f, -0.25f };
        glm::vec3 SunColor{ 1.0f, 0.95f, 0.85f };
        float SkyAmbient = 0.12f;
        uint32_t SampleRays = 12;
        float AORadius = 4.0f;
    };

    static Ref<Texture2D> Bake(const Input& in, const TraceFn& trace);
};

}