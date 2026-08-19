#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Buffer.hpp"
#include <glm/glm.hpp>

namespace kizuri {

class RenderCommand {
public:
    static void Init();
    static void SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
    static void SetClearColor(const glm::vec4& color);
    static void Clear();
    static void DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount = 0);
    static void DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount);
    static void SetDepthTest(bool enabled);
    static void SetBlending(bool enabled);

    static void ResetFrameStats();
    static uint32_t GetFrameDrawCalls();
    static uint32_t GetFrameTriangles();

    static void AddInstancedStats(uint32_t instances);

private:
    static uint32_t s_FrameDrawCalls;
    static uint32_t s_FrameTriangles;
};

}
