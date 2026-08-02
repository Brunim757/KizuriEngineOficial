#include "kizuri/scene/SceneSerializer.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "ComponentSerialization.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <unordered_map>

using json = nlohmann::json;

namespace kizuri {

void SceneSerializer::Serialize(const std::string& filepath) {
    std::ofstream out(filepath);
    out << SerializeToJson().dump(4);
    KZ_CORE_INFO("Cena salva com sucesso em: {0}", filepath);
}

json SceneSerializer::SerializeToJson() {
    json root;
    root["Scene"] = m_Scene->GetName();
    root["Entities"] = json::array();

    m_Scene->GetRegistry().view<IDComponent>().each([&](auto entityHandle, IDComponent&) {
        Entity entity{ entityHandle, m_Scene.get() };
        root["Entities"].push_back(detail::SerializeEntityJson(entity));
    });
    return root;
}

bool SceneSerializer::Deserialize(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        KZ_CORE_ERROR("Não foi possível abrir o arquivo de cena: {0}", filepath);
        return false;
    }

    json root;
    in >> root;
    if (!DeserializeFromJson(root)) return false;

    KZ_CORE_INFO("Cena carregada com sucesso de: {0}", filepath);
    return true;
}

bool SceneSerializer::DeserializeFromJson(const json& root) {
    // Passo 1: cria todas as entidades preservando o UUID salvo, sem ainda
    // resolver hierarquia (o pai pode aparecer depois do filho no array).
    std::unordered_map<uint64_t, uint64_t> parentOf; // uuid filho -> uuid pai
    for (auto& je : root["Entities"]) {
        uint64_t id = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, *m_Scene, id);

        uint64_t parentId = je.value("Parent", (uint64_t)0);
        if (parentId != 0) parentOf[(uint64_t)entity.GetUUID()] = parentId;
    }

    // Passo 2: agora que todo mundo existe, resolve os pais.
    for (auto& [childUUID, parentUUID] : parentOf) {
        Entity child = m_Scene->GetEntityByUUID(UUID(childUUID));
        Entity parent = m_Scene->GetEntityByUUID(UUID(parentUUID));
        if (child && parent) m_Scene->SetParent(child, parent);
    }
    return true;
}

} // namespace kizuri
