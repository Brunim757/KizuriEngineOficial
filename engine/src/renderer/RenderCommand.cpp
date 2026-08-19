#include "kizuri/renderer/RenderCommand.hpp"
#include <glad/gl.h>

namespace kizuri {

void RenderCommand::Init() {
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnable(GL_DEPTH_TEST);
}

void RenderCommand::SetViewport(uint32_t x, uint32_t y, uint32_t width, uint32_t height) {
    glViewport((GLint)x, (GLint)y, (GLsizei)width, (GLsizei)height);
}

void RenderCommand::SetClearColor(const glm::vec4& color) { glClearColor(color.r, color.g, color.b, color.a); }
void RenderCommand::Clear() { glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); }

void RenderCommand::DrawIndexed(const Ref<VertexArray>& vertexArray, uint32_t indexCount) {
    uint32_t count = indexCount ? indexCount : vertexArray->GetIndexBuffer()->GetCount();
    vertexArray->Bind();
    glDrawElements(GL_TRIANGLES, (GLsizei)count, GL_UNSIGNED_INT, nullptr);
    s_FrameDrawCalls += 1;
    s_FrameTriangles += (uint32_t)count / 3;
}

void RenderCommand::DrawLines(const Ref<VertexArray>& vertexArray, uint32_t vertexCount) {
    vertexArray->Bind();
    glDrawArrays(GL_LINES, 0, (GLsizei)vertexCount);
    s_FrameDrawCalls += 1;
}

void RenderCommand::SetDepthTest(bool enabled) { enabled ? glEnable(GL_DEPTH_TEST) : glDisable(GL_DEPTH_TEST); }
void RenderCommand::SetBlending(bool enabled)  { enabled ? glEnable(GL_BLEND) : glDisable(GL_BLEND); }

uint32_t RenderCommand::s_FrameDrawCalls = 0;
uint32_t RenderCommand::s_FrameTriangles = 0;

void RenderCommand::ResetFrameStats() { s_FrameDrawCalls = 0; s_FrameTriangles = 0; }
uint32_t RenderCommand::GetFrameDrawCalls() { return s_FrameDrawCalls; }
uint32_t RenderCommand::GetFrameTriangles() { return s_FrameTriangles; }
void RenderCommand::AddInstancedStats(uint32_t instances) {
    s_FrameDrawCalls += 1;
    s_FrameTriangles += instances * 2; 
}

} 
