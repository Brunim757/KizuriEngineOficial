#include "kizuri/renderer/Framebuffer.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>

namespace kizuri {

Framebuffer::Framebuffer(const FramebufferSpec& spec) : m_Spec(spec) {
    Invalidate();
}

Framebuffer::~Framebuffer() {
    glDeleteFramebuffers(1, &m_RendererID);
    glDeleteTextures(1, &m_ColorAttachment);
    glDeleteTextures(1, &m_DepthAttachment);
}

void Framebuffer::Invalidate() {
    KZ_TRACE_SCOPE("Framebuffer::Invalidate");
    if (m_RendererID) {
        glDeleteFramebuffers(1, &m_RendererID);
        glDeleteTextures(1, &m_ColorAttachment);
        glDeleteTextures(1, &m_DepthAttachment);
    }

    glGenFramebuffers(1, &m_RendererID);
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);

    glGenTextures(1, &m_ColorAttachment);
    glBindTexture(GL_TEXTURE_2D, m_ColorAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)m_Spec.Width, (GLsizei)m_Spec.Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_ColorAttachment, 0);

    glGenTextures(1, &m_DepthAttachment);
    glBindTexture(GL_TEXTURE_2D, m_DepthAttachment);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH24_STENCIL8, (GLsizei)m_Spec.Width, (GLsizei)m_Spec.Height, 0,
                 GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, m_DepthAttachment, 0);

    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        KZ_CORE_ERROR("Framebuffer incompleto (status inválido).");

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Bind() const {
    KZ_TRACE_SCOPE("Framebuffer::Bind");
    glBindFramebuffer(GL_FRAMEBUFFER, m_RendererID);
    glViewport(0, 0, (GLsizei)m_Spec.Width, (GLsizei)m_Spec.Height);
}

void Framebuffer::Unbind() const {
    KZ_TRACE_SCOPE("Framebuffer::Unbind");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Framebuffer::Resize(uint32_t width, uint32_t height) {
    KZ_TRACE_SCOPE("Framebuffer::Resize");
    if (width == 0 || height == 0 || (width == m_Spec.Width && height == m_Spec.Height))
        return;

    m_Spec.Width = width;
    m_Spec.Height = height;
    Invalidate();
}

Ref<Framebuffer> Framebuffer::Create(const FramebufferSpec& spec) {
    return CreateRef<Framebuffer>(spec);
}

}
