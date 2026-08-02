#include "kizuri/scene/Prefab.hpp"
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "ComponentSerialization.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>
#include <vector>

using json = nlohmann::json;

namespace kizuri {

static void CollectSubtree(Entity entity, json& outArray) {
    outArray.push_back(detail::SerializeEntityJson(entity));
    for (Entity child : entity.GetChildren())
        CollectSubtree(child, outArray);
}

void Prefab::CreateFromEntity(Entity root, const std::string& filepath) {
    json out;
    out["Prefab"] = json::array();
    CollectSubtree(root, out["Prefab"]);

    std::ofstream file(filepath);
    file << out.dump(4);
    KZ_CORE_INFO("Prefab salva em: {0}", filepath);
}

Entity Prefab::Instantiate(Scene& scene, const std::string& filepath, const glm::vec3& position) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        KZ_CORE_ERROR("Não foi possível abrir prefab: {0}", filepath);
        return {};
    }

    json root;
    in >> root;

    // Passo 1: cria todas as entidades com UUIDs novos, guardando o mapa
    // uuid-antigo -> uuid-novo pra resolver a hierarquia depois. Isso é o
    // que permite instanciar a mesma prefab várias vezes sem colisão.
    std::unordered_map<uint64_t, uint64_t> remap;
    std::vector<Entity> created;
    created.reserve(root["Prefab"].size());

    for (auto& je : root["Prefab"]) {
        uint64_t oldId = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, scene, 0 /* força UUID novo */);
        remap[oldId] = (uint64_t)entity.GetUUID();
        created.push_back(entity);
    }

    // Passo 2: resolve hierarquia com os UUIDs remapeados.
    size_t i = 0;
    for (auto& je : root["Prefab"]) {
        uint64_t oldParentId = je.value("Parent", (uint64_t)0);
        auto it = remap.find(oldParentId);
        if (oldParentId != 0 && it != remap.end()) {
            Entity parent = scene.GetEntityByUUID(UUID(it->second));
            if (parent) created[i].SetParent(parent);
        }
        ++i;
    }

    if (created.empty()) return {};

    // A primeira entidade salva por CollectSubtree é sempre a raiz da
    // subárvore (pré-ordem: raiz antes dos filhos).
    Entity rootEntity = created.front();
    rootEntity.GetComponent<TransformComponent>().Translation = position;

    KZ_CORE_INFO("Prefab instanciada de: {0}", filepath);
    return rootEntity;
}

} // namespace kizuri
