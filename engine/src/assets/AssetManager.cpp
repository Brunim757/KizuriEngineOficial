#include "kizuri/assets/AssetManager.hpp"
#include "kizuri/core/Log.hpp"
#include <thread>
#include <mutex>
#include <cstring>

#include <stb_image.h>

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

namespace {

struct CompletedTexture {
    std::string Path;
    AssetManager::TextureCallback Callback;
    int Width = 0, Height = 0, Channels = 0;
    std::vector<unsigned char> Pixels;
};

std::mutex s_CompletedMutex;
std::vector<CompletedTexture> s_Completed;
std::vector<std::thread> s_Workers;
std::mutex s_WorkersMutex;

}

bool AssetManager::LoadTextureAsync(const std::string& path, TextureCallback callback) {
    if (path.empty()) return false;

    auto it = s_Textures.find(path);
    if (it != s_Textures.end()) {
        if (callback) callback(it->second);
        return true;
    }

    std::thread worker([path, callback]() {

        stbi_set_flip_vertically_on_load(1);
        int w = 0, h = 0, ch = 0;
        stbi_uc* data = stbi_load(path.c_str(), &w, &h, &ch, 4);
        CompletedTexture done;
        done.Path = path;
        done.Callback = callback;
        if (data && w > 0 && h > 0) {
            done.Width = w;
            done.Height = h;
            done.Channels = 4;
            done.Pixels.assign(data, data + (size_t)w * h * 4);
        }
        stbi_image_free(data);
        {
            std::lock_guard<std::mutex> lock(s_CompletedMutex);
            s_Completed.push_back(std::move(done));
        }
    });
    {
        std::lock_guard<std::mutex> lock(s_WorkersMutex);
        s_Workers.push_back(std::move(worker));
    }
    return true;
}

void AssetManager::TickAsyncLoads() {

    std::vector<CompletedTexture> ready;
    {
        std::lock_guard<std::mutex> lock(s_CompletedMutex);
        ready.swap(s_Completed);
    }
    for (auto& done : ready) {
        Ref<Texture2D> tex;
        if (!done.Pixels.empty()) {
            tex = Texture2D::Create(done.Width, done.Height);
            tex->SetData(done.Pixels.data(), (uint32_t)done.Pixels.size());
            s_Textures[done.Path] = tex;
        } else {
            KZ_CORE_WARN("AssetManager: falha ao decodificar '{}' (async).", done.Path);
        }
        if (done.Callback) done.Callback(tex);
    }
}

void AssetManager::Shutdown() {
    std::vector<std::thread> workers;
    {
        std::lock_guard<std::mutex> lock(s_WorkersMutex);
        workers.swap(s_Workers);
    }
    for (auto& w : workers)
        if (w.joinable()) w.join();
    {
        std::lock_guard<std::mutex> lock(s_CompletedMutex);
        s_Completed.clear();
    }
}

}
