#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Entity.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

class Scene;

// Prefab é um "carimbo" reutilizável de uma entidade e de toda a sua
// subárvore de filhos, salvo em disco como .kzprefab (JSON). Diferente de
// carregar uma cena inteira via SceneSerializer, aqui só a subárvore
// pedida é salva/instanciada — e cada instanciação gera UUIDs novos (nunca
// reaproveita os da prefab original), então a mesma prefab pode ser
// solta várias vezes na cena sem colidir identidades.
class Prefab {
public:
    static void CreateFromEntity(Entity root, const std::string& filepath);
    static Entity Instantiate(Scene& scene, const std::string& filepath,
                               const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
};

} // namespace kizuri
