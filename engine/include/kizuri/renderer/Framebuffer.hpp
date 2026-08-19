#pragma once
#include "kizuri/Core.hpp"
#include <cstdint>

namespace kizuri {

struct FramebufferSpec {
    uint32_t Width = 1280;
    uint32_t Height = 720;
    uint32_t Samples = 1;
};





class Framebuffer {
public:
    explicit Framebuffer(const FramebufferSpec& spec);
    ~Framebuffer();

    void Bind() const;
    void Unbind() const;

    
    
    
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

} 
