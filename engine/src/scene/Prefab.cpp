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
    KZ_CORE_INFO("Prefab salva com sucesso em: {0}", filepath);
}

Entity Prefab::Instantiate(Scene& scene, const std::string& filepath, const glm::vec3& position) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        KZ_CORE_ERROR("Não foi possível abrir o prefab: {0}", filepath);
        return {};
    }

    json root;
    in >> root;

    std::unordered_map<uint64_t, uint64_t> remap;
    std::vector<Entity> created;
    created.reserve(root["Prefab"].size());

    for (auto& je : root["Prefab"]) {
        uint64_t oldId = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, scene, 0 );
        remap[oldId] = (uint64_t)entity.GetUUID();
        created.push_back(entity);
    }

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

    Entity rootEntity = created.front();
    rootEntity.GetComponent<TransformComponent>().Translation = position;

    KZ_CORE_INFO("Prefab instanciada com sucesso de: {0}", filepath);
    return rootEntity;
}

}
