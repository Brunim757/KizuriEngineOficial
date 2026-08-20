#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Scene.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace kizuri {

class ChunkSerializer {
public:
    explicit ChunkSerializer(Scene* scene) : m_Scene(scene) {}

    bool Save(const std::string& filepath, int chunkX, int chunkZ);

    bool Load(const std::string& filepath, int chunkX, int chunkZ);

private:
    Scene* m_Scene;
};

}