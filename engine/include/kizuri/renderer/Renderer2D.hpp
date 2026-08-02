#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/Texture.hpp"
#include <glm/glm.hpp>

namespace kizuri {

struct Renderer2DStats {
    uint32_t DrawCalls = 0;
    uint32_t QuadCount = 0;
    uint32_t GetTotalVertexCount() const { return QuadCount * 4; }
    uint32_t GetTotalIndexCount() const { return QuadCount * 6; }
};

// Renderer2D usa batching: acumula quads em um único buffer grande e
// despeja tudo em poucas chamadas de draw, com suporte a até 32 texturas
// simultâneas por batch (técnica clássica de engines 2D de alta performance).
class Renderer2D {
public:
    static void Init();
    static void Shutdown();

    static void BeginScene(const OrthographicCamera& camera);
    static void BeginScene(const glm::mat4& viewProjection);
    static void EndScene();
    static void Flush();

    static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const glm::vec4& color);
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const glm::vec4& color);
    static void DrawQuad(const glm::vec2& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawQuad(const glm::vec3& position, const glm::vec2& size, const Ref<Texture2D>& texture, float tilingFactor = 1.0f, const glm::vec4& tint = glm::vec4(1.0f));
    static void DrawRotatedQuad(const glm::vec3& position, const glm::vec2& size, float rotationDeg, const glm::vec4& color);
    static void DrawTransformedQuad(const glm::mat4& transform, const glm::vec4& color, int entityID = -1);
    static void DrawTransformedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tilingFactor, const glm::vec4& tint, int entityID = -1);

    // Grid de referência no plano XY (Z=0), com os eixos X (vermelho) e Y
    // (azul) destacados — equivalente 2D do Renderer3D::DrawGrid(). Só faz
    // sentido dentro de um BeginScene/EndScene, igual DrawQuad.
    static void DrawGrid();

    static void ResetStats();
    static Renderer2DStats GetStats();

private:
    static void StartBatch();
    static void NextBatch();
};

} // namespace kizuri
