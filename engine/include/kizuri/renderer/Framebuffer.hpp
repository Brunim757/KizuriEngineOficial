#pragma once
#include "kizuri/Core.hpp"
#include <cstdint>

namespace kizuri {

struct FramebufferSpec {
    uint32_t Width = 1280;
    uint32_t Height = 720;
    uint32_t Samples = 1;
};

// Framebuffer de renderização (color attachment RGBA8 + depth/stencil).
// Usado pelo Kizuri Editor para renderizar a cena numa textura e exibi-la
// dentro do painel "Viewport" via ImGui::Image, em vez de desenhar direto
// na tela (que ficaria embaixo/atrás de todos os outros painéis do dockspace).
class Framebuffer {
public:
    explicit Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    void Bind() const;
    void Unbind() const;

    // Recria as texturas se o tamanho mudou (chamado a cada frame pelo
    // editor com o tamanho atual do painel Viewport; é barato quando o
    // tamanho não mudou, então pode ser chamado sem se preocupar).
    void Resize(uint32_t width, uint32_t height);

    uint32_t GetColorAttachmentRendererID() const { return m_ColorAttachment; }
    const FramebufferSpec& GetSpec() const { return m_Spec; }

    static Ref<Framebuffer> Create(const FramebufferSpec& spec);

private:
    void Invalidate();

    uint32_t m_RendererID = 0;
    uint32_t m_ColorAttachment = 0;
    uint32_t m_DepthAttachment = 0;
    FramebufferSpec m_Spec;
};

} // namespace kizuri
