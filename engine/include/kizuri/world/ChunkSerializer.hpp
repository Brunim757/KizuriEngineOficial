#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Scene.hpp"
#include <nlohmann/json.hpp>
#include <string>

namespace kizuri {

// Serializa e deserializa um chunk do mundo (.kzchunk).
// Formato versionado: { "Version":1, "Chunk":{cx,cz}, "Entities":[...] }
// O version permite migracao futura sem quebrar cenas antigas.
class ChunkSerializer {
public:
    explicit ChunkSerializer(Scene* scene) : m_Scene(scene) {}

    // Salva todas as entidades com ChunkEntityComponent (cx,cz) num arquivo.
    bool Save(const std::string& filepath, int chunkX, int chunkZ);

    // Carrega entidades do chunk na cena. Retorna false se o arquivo
    // nao existir (chunk vazio — normal num mundo esparso).
    bool Load(const std::string& filepath, int chunkX, int chunkZ);

    // Salva TODOS os chunks carregados numa pasta (batch save).
    // Retorna quantos arquivos foram escritos.
    int SaveAll(const std::string& folder);

    // Gera o path padrao de um chunk: <folder>/chunk_<cx>_<cz>.kzchunk
    static std::string ChunkPath(const std::string& folder, int cx, int cz);

private:
    static constexpr int CHUNK_VERSION = 1;
    Scene* m_Scene;
};

}