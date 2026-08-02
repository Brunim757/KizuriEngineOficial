#include "kizuri/renderer/Texture.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>

#define STB_IMAGE_IMPLEMENTATION_GUARD
#ifdef STB_IMAGE_IMPLEMENTATION_GUARD
    // stb_image_impl.cpp já define STB_IMAGE_IMPLEMENTATION; aqui só incluímos o header.
#endif
#include <stb_image.h>

namespace kizuri {

Texture2D::Texture2D(uint32_t width, uint32_t height)
    : m_Width(width), m_Height(height), m_InternalFormat(GL_RGBA8), m_DataFormat(GL_RGBA) {
    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)m_InternalFormat, (GLsizei)width, (GLsizei)height, 0, m_DataFormat, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
}

Texture2D::Texture2D(const std::string& path) : m_Path(path) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(1);
    stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
    if (!data) {
        KZ_CORE_ERROR("Falha ao carregar a textura: {0}", path);
        return;
    }

    m_Width = width; m_Height = height;
    GLenum internalFormat = 0, dataFormat = 0;
    if (channels == 4) { internalFormat = GL_RGBA8; dataFormat = GL_RGBA; }
    else if (channels == 3) { internalFormat = GL_RGBA8; dataFormat = GL_RGB; }
    m_InternalFormat = internalFormat; m_DataFormat = dataFormat;

    glGenTextures(1, &m_RendererID);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)internalFormat, width, height, 0, dataFormat, GL_UNSIGNED_BYTE, data);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    stbi_image_free(data);
    KZ_CORE_INFO("Textura carregada: {0} ({1}x{2}).", path, width, height);
}

Texture2D::~Texture2D() { glDeleteTextures(1, &m_RendererID); }

void Texture2D::SetData(void* data, uint32_t size) {
    (void)size;
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
    glTexImage2D(GL_TEXTURE_2D, 0, (GLint)m_InternalFormat, (GLsizei)m_Width, (GLsizei)m_Height, 0, m_DataFormat, GL_UNSIGNED_BYTE, data);
}

void Texture2D::Bind(uint32_t slot) const {
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_RendererID);
}

Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height) { return CreateRef<Texture2D>(width, height); }
Ref<Texture2D> Texture2D::Create(const std::string& path)         { return CreateRef<Texture2D>(path); }

} // namespace kizuri
