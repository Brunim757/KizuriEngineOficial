#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

class Scene;







class Prefab {
public:
    static void CreateFromEntity(Entity root, const std::string& filepath);
    static Entity Instantiate(Scene& scene, const std::string& filepath,
                               const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
};

} 
