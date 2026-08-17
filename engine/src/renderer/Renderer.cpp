#include "kizuri/renderer/Renderer.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/TextRenderer.hpp"

namespace kizuri {

ShaderLibrary Renderer::s_ShaderLibrary;

ShaderLibrary& Renderer::GetShaderLibrary() { return s_ShaderLibrary; }

void Renderer::Init() {
    RenderCommand::Init();
    Renderer2D::Init();
    Renderer3D::Init();
    TextRenderer::Init();
    KZ_CORE_INFO("Renderer inicializado (2D + 3D + texto).");
}

void Renderer::Shutdown() {
    Renderer2D::Shutdown();
    Renderer3D::Shutdown();
    TextRenderer::Shutdown();
}

void Renderer::OnWindowResize(uint32_t width, uint32_t height) {
    RenderCommand::SetViewport(0, 0, width, height);
}

} // namespace kizuri
