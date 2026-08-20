#include "kizuri/world/ChunkSerializer.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "ComponentSerialization.hpp"

#include <nlohmann/json.hpp>
#include <fstream>

using json = nlohmann::json;

namespace kizuri {

bool ChunkSerializer::Save(const std::string& filepath, int chunkX, int chunkZ) {
    json root;
    root["Chunk"] = { { "cx", chunkX }, { "cz", chunkZ } };
    root["Entities"] = json::array();

    m_Scene->GetRegistry().view<IDComponent, ChunkEntityComponent>().each(
        [&](auto entityHandle, IDComponent&, ChunkEntityComponent& ce) {
            if (ce.ChunkX != chunkX || ce.ChunkZ != chunkZ) return;
            Entity entity{ entityHandle, m_Scene };
            root["Entities"].push_back(detail::SerializeEntityJson(entity));
        });

    std::ofstream out(filepath);
    if (!out.is_open()) {
        KZ_CORE_ERROR("ChunkSerializer: não foi possível salvar em {0}", filepath);
        return false;
    }
    out << root.dump(4);
    return true;
}

bool ChunkSerializer::Load(const std::string& filepath, int chunkX, int chunkZ) {
    std::ifstream in(filepath);
    if (!in.is_open()) return false;

    json root;
    try {
        in >> root;
    } catch (...) {
        KZ_CORE_ERROR("ChunkSerializer: JSON inválido em {0}", filepath);
        return false;
    }
    if (!root.contains("Entities") || !root["Entities"].is_array()) {
        KZ_CORE_ERROR("ChunkSerializer: array 'Entities' ausente em {0}", filepath);
        return false;
    }

    std::vector<uint64_t> refs;
    for (const auto& je : root["Entities"]) {
        uint64_t id = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, *m_Scene, id);
        if (!entity) continue;
        if (!entity.HasComponent<ChunkEntityComponent>())
            entity.AddComponent<ChunkEntityComponent>().ChunkX = chunkX;
        auto& ce = entity.GetComponent<ChunkEntityComponent>();
        ce.ChunkX = chunkX;
        ce.ChunkZ = chunkZ;
        refs.push_back(id);
    }

    return true;
}

}