#include "kizuri/assets/AssetManager.hpp"

namespace kizuri {

std::unordered_map<std::string, Ref<Texture2D>> AssetManager::s_Textures;
std::unordered_map<std::string, Ref<Mesh>> AssetManager::s_Meshes;

Ref<Texture2D> AssetManager::GetTexture(const std::string& path) {
    auto it = s_Textures.find(path);
    if (it != s_Textures.end()) return it->second;
    auto tex = Texture2D::Create(path);
    s_Textures[path] = tex;
    return tex;
}

Ref<Mesh> AssetManager::GetMesh(const std::string& path) {
    auto it = s_Meshes.find(path);
    if (it != s_Meshes.end()) return it->second;
    auto mesh = Mesh::LoadFromOBJ(path);
    s_Meshes[path] = mesh;
    return mesh;
}

void AssetManager::Clear() {
    s_Textures.clear();
    s_Meshes.clear();
}

} // namespace kizuri
