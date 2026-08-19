#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

enum class TextAlignment : int { Left = 0, Center = 1, Right = 2 };

class TextRenderer {
public:

    static bool IsReady();
    static Ref<Texture2D> GetAtlasTexture();
    static std::string GetDiagnostics();

public:
    static void Init();
    static void Shutdown();

    static float MeasureWidth(const std::string& text, float fontSize);

    static void DrawString(const std::string& text, const glm::vec3& position,
                           float fontSize, const glm::vec4& color,
                           TextAlignment alignment = TextAlignment::Left);

private:
    static void EnsureAtlas();
};

}
