#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include <string>
#include <unordered_map>
#include <functional>
#include <vector>

namespace kizuri {

class AssetManager {
public:

    using TextureCallback = std::function<void(Ref<Texture2D>)>;

    static Ref<Texture2D> GetTexture(const std::string& path);
    static Ref<Mesh> GetMesh(const std::string& path);
    static void Clear();

    static bool LoadTextureAsync(const std::string& path, TextureCallback callback);

    static void TickAsyncLoads();

    static void Shutdown();

private:

    static std::unordered_map<std::string, Ref<Texture2D>> s_Textures;
    static std::unordered_map<std::string, Ref<Mesh>> s_Meshes;
};

}
