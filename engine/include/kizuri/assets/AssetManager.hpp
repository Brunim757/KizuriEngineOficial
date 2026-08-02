#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include <string>
#include <unordered_map>

namespace kizuri {

// AssetManager evita recarregar o mesmo arquivo do disco várias vezes:
// mantém um cache indexado por caminho para texturas e meshes.
class AssetManager {
public:
    static Ref<Texture2D> GetTexture(const std::string& path);
    static Ref<Mesh> GetMesh(const std::string& path);
    static void Clear();

private:
    // Sem 'inline' de propósito — mesmo bug do Application::s_Instance
    // (ver Application.cpp). GetTexture/GetMesh/Clear já eram funções de
    // verdade (não inline), então isso já funcionava certo na prática,
    // mas o "static inline" aqui ainda duplicava a variável por binário
    // à toa — corrigido por consistência com o resto da engine.
    static std::unordered_map<std::string, Ref<Texture2D>> s_Textures;
    static std::unordered_map<std::string, Ref<Mesh>> s_Meshes;
};

} // namespace kizuri
