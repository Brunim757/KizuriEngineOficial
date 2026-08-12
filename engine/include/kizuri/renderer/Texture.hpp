#pragma once
#include "kizuri/Core.hpp"
#include <string>

namespace kizuri {

class Texture2D {
public:
    Texture2D(uint32_t width, uint32_t height);
    explicit Texture2D(const std::string& path);
    ~Texture2D();

    uint32_t GetWidth() const  { return m_Width; }
    uint32_t GetHeight() const { return m_Height; }
    uint32_t GetRendererID() const { return m_RendererID; }

    void SetData(void* data, uint32_t size);
    void Bind(uint32_t slot = 0) const;

    bool operator==(const Texture2D& other) const { return m_RendererID == other.m_RendererID; }

    static Ref<Texture2D> Create(uint32_t width, uint32_t height);
    static Ref<Texture2D> Create(const std::string& path);

    // Textura a partir de bytes em memória (ex.: imagens embutidas num .glb).
    // NÃO inverte verticalmente — convenção UV do glTF (v=0 no topo).
    static Ref<Texture2D> CreateFromMemory(const void* data, size_t size, const std::string& debugName = "");

    // Salva a textura num .png (readback GL + stbi_write_png). Usada pelo
    // bake de lightmap do editor pra persistir o resultado em disco.
    static bool SaveToFile(const Ref<Texture2D>& texture, const std::string& path);

private:
    std::string m_Path;
    uint32_t m_Width = 0, m_Height = 0;
    uint32_t m_RendererID = 0;
    uint32_t m_InternalFormat = 0, m_DataFormat = 0;
};

} // namespace kizuri
