#include "kizuri/renderer/Texture.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/EmbeddedContent.hpp"
#include <glad/gl.h>
#include <cstring>
#include <vector>
#include <stb_image_write.h>

#define STB_IMAGE_IMPLEMENTATION_GUARD
#ifdef STB_IMAGE_IMPLEMENTATION_GUARD
    // stb_image_impl.cpp já define STB_IMAGE_IMPLEMENTATION; aqui só incluímos o header.
#endif
#include <stb_image.h>

namespace kizuri {

// Filtragem anisotrópica (GL_TEXTURE_MAX_ANISOTROPY = 0x84FE): texturas
// vistas em ângulo ficam nítidas em vez de borradas. Extensão ARB (presente
// na prática em qualquer driver 3.3+) e núcleo no GL 4.6. Checado uma vez.
static bool SupportsAnisotropy() {
    static bool s_checked = false, s_supported = false;
    if (!s_checked) {
        s_checked = true;
        const char* exts = (const char*)glGetString(GL_EXTENSIONS);
        if (exts && strstr(exts, "GL_ARB_texture_filter_anisotropic")) s_supported = true;
        if (!s_supported) {
            const char* v = (const char*)glGetString(GL_VERSION);
            int major = 0;
            if (v) {
                while (*v && !(*v >= '0' && *v <= '9')) ++v;
                if (*v >= '0' && *v <= '9') major = *v - '0';
            }
            if (major >= 4) s_supported = true; // em 4.x todos os drivers práticos têm
        }
    }
    return s_supported;
}

static void ApplyAnisotropy() {
    if (SupportsAnisotropy()) glTexParameterf(GL_TEXTURE_2D, 0x84FE /*GL_TEXTURE_MAX_ANISOTROPY*/, 8.0f);
}

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
    ApplyAnisotropy();

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
bool Texture2D::SaveToFile(const Ref<Texture2D>& texture, const std::string& path) {
#if defined(KZ_PLATFORM_ANDROID)
    // glGetTexImage não existe em GLES — export/editor é recurso de desktop.
    (void)texture; (void)path;
    return false;
#else
    if (!texture || path.empty()) return false;
    const uint32_t w = texture->GetWidth(), h = texture->GetHeight();
    if (w == 0 || h == 0) return false;
    std::vector<uint8_t> pixels((size_t)w * h * 4);
    glBindTexture(GL_TEXTURE_2D, texture->GetRendererID());
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    // stbi_write_png espera y=0 no topo; a textura GL tem y=0 embaixo — inverte.
    std::vector<uint8_t> flipped((size_t)w * h * 4);
    for (uint32_t y = 0; y < h; ++y)
        std::memcpy(flipped.data() + (size_t)(h - 1 - y) * w * 4, pixels.data() + (size_t)y * w * 4, (size_t)w * 4);
    return stbi_write_png(path.c_str(), (int)w, (int)h, 4, flipped.data(), (int)w * 4) != 0;
#endif
}

Ref<Texture2D> Texture2D::Create(const std::string& path) {
    if (IsEmbeddedPath(path)) {
        EmbeddedBuffer buf;
        if (GetEmbeddedResource(EmbeddedNameFromPath(path), buf))
            return CreateFromMemory(buf.Data, buf.Size, path);
        KZ_CORE_ERROR("Recurso embutido não encontrado: {0}", path);
        return CreateRef<Texture2D>(1, 1);
    }
    return CreateRef<Texture2D>(path);
}

Ref<Texture2D> Texture2D::CreateFromMemory(const void* data, size_t size, const std::string& debugName) {
    int width, height, channels;
    stbi_set_flip_vertically_on_load(0); // UV glTF: v=0 no topo — nada de flip
    stbi_uc* pixels = stbi_load_from_memory((const stbi_uc*)data, (int)size, &width, &height, &channels, 0);
    if (!pixels) {
        KZ_CORE_ERROR("Falha ao carregar textura em memória: {0}", debugName.empty() ? "(sem nome)" : debugName);
        return CreateRef<Texture2D>(1, 1);
    }

    auto tex = CreateRef<Texture2D>(width, height);
    tex->m_InternalFormat = (channels == 4) ? GL_RGBA8 : GL_RGBA8;
    tex->m_DataFormat = (channels == 4) ? GL_RGBA : GL_RGB;
    glBindTexture(GL_TEXTURE_2D, tex->m_RendererID);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, tex->m_DataFormat, GL_UNSIGNED_BYTE, pixels);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    ApplyAnisotropy();
    stbi_image_free(pixels);
    return tex;
}

} // namespace kizuri
