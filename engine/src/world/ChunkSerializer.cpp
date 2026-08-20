#include "kizuri/world/ChunkSerializer.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "../scene/ComponentSerialization.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <unordered_map>

using json = nlohmann::json;
namespace fs = std::filesystem;

namespace kizuri {

namespace {

struct PairHash {
    size_t operator()(const std::pair<int,int>& p) const {
        size_t h = std::hash<int>{}(p.first);
        h ^= std::hash<int>{}(p.second) + 0x9e3779b9u + (h << 6) + (h >> 2);
        return h;
    }
};

} // anonymous

std::string ChunkSerializer::ChunkPath(const std::string& folder, int cx, int cz) {
    return folder + "/chunk_" + std::to_string(cx) + "_" + std::to_string(cz) + ".kzchunk";
}

bool ChunkSerializer::Save(const std::string& filepath, int chunkX, int chunkZ) {
    json root;
    root["Version"] = CHUNK_VERSION;
    root["Chunk"] = { { "cx", chunkX }, { "cz", chunkZ } };
    root["Entities"] = json::array();

    int count = 0;
    m_Scene->GetRegistry().view<IDComponent, ChunkEntityComponent>().each(
        [&](auto entityHandle, IDComponent&, ChunkEntityComponent& ce) {
            if (ce.ChunkX != chunkX || ce.ChunkZ != chunkZ) return;
            Entity entity{ entityHandle, m_Scene };
            if (!entity.HasComponent<TransformComponent>()) return;
            root["Entities"].push_back(detail::SerializeEntityJson(entity));
            ++count;
        });

    if (count == 0) return false;

    std::error_code ec;
    fs::create_directories(fs::path(filepath).parent_path(), ec);

    std::ofstream out(filepath);
    if (!out.is_open()) {
        KZ_CORE_ERROR("ChunkSerializer: falha ao salvar chunk ({0},{1}) em {2}",
                      chunkX, chunkZ, filepath);
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
    } catch (const std::exception& e) {
        KZ_CORE_ERROR("ChunkSerializer: JSON invalido em {0}: {1}", filepath, e.what());
        return false;
    } catch (...) {
        KZ_CORE_ERROR("ChunkSerializer: JSON invalido em {0}", filepath);
        return false;
    }

    int version = root.value("Version", 0);
    if (version > CHUNK_VERSION) {
        KZ_CORE_WARN("ChunkSerializer: arquivo {0} e v{1} (engine suporta v{2}). "
                     "Pode ser incompativel.", filepath, version, CHUNK_VERSION);
    }

    if (!root.contains("Chunk") || !root.contains("Entities")) {
        KZ_CORE_ERROR("ChunkSerializer: campos 'Chunk'/'Entities' ausentes em {0}", filepath);
        return false;
    }

    auto& entities = root["Entities"];
    if (!entities.is_array()) {
        KZ_CORE_ERROR("ChunkSerializer: 'Entities' nao e array em {0}", filepath);
        return false;
    }

    int loaded = 0, failed = 0;
    for (const auto& je : entities) {
        if (!je.is_object() || !je.contains("ID")) {
            ++failed;
            continue;
        }
        uint64_t id = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, *m_Scene, id);
        if (!entity) { ++failed; continue; }
        if (!entity.HasComponent<ChunkEntityComponent>())
            entity.AddComponent<ChunkEntityComponent>();
        auto& ce = entity.GetComponent<ChunkEntityComponent>();
        ce.ChunkX = chunkX;
        ce.ChunkZ = chunkZ;
        ++loaded;
    }

    if (failed > 0)
        KZ_CORE_WARN("ChunkSerializer: {0} entidades falharam ao carregar de {1}", failed, filepath);
    return loaded > 0;
}

int ChunkSerializer::SaveAll(const std::string& folder) {
    std::unordered_map<std::pair<int,int>, int, PairHash> chunkCounts;

    m_Scene->GetRegistry().view<IDComponent, ChunkEntityComponent>().each(
        [&](auto, IDComponent&, ChunkEntityComponent& ce) {
            chunkCounts[{ ce.ChunkX, ce.ChunkZ }]++; });

    int saved = 0;
    for (auto& [coords, count] : chunkCounts) {
        if (count == 0) continue;
        if (Save(ChunkPath(folder, coords.first, coords.second), coords.first, coords.second))
            ++saved;
    }
    return saved;
}

} // namespace kizuri