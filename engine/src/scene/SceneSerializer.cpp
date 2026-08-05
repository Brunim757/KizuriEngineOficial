#include "kizuri/scene/SceneSerializer.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include "ComponentSerialization.hpp"

#include <nlohmann/json.hpp>
#include <fstream>
#include <chrono>
#include <climits>
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

// ---- Carregamento incremental (projetos grandes) ----

bool SceneSerializer::BeginDeserializeStepwiseFile(const std::string& filepath) {
    std::ifstream in(filepath);
    if (!in.is_open()) {
        KZ_CORE_ERROR("Não foi possível abrir o arquivo de cena: {0}", filepath);
        return false;
    }
    json root;
    in >> root;
    return BeginDeserializeStepwise(root);
}

bool SceneSerializer::BeginDeserializeStepwise(const json& root) {
    if (!root.contains("Entities") || !root["Entities"].is_array()) {
        KZ_CORE_ERROR("Cena inválida: array 'Entities' ausente ou malformado.");
        return false;
    }
    m_PendingEntities = root["Entities"];
    m_PendingIndex = 0;
    m_ParentOf.clear();
    return true;
}

void SceneSerializer::FinishStepwise() {
    // Agora que todo mundo existe, resolve os pais (O(1) por entidade).
    for (auto& [childUUID, parentUUID] : m_ParentOf) {
        Entity child = m_Scene->GetEntityByUUID(UUID(childUUID));
        Entity parent = m_Scene->GetEntityByUUID(UUID(parentUUID));
        if (child && parent) m_Scene->SetParent(child, parent);
    }
    m_ParentOf.clear();
    m_PendingIndex = 0;
    m_PendingEntities = json();
}

// Processa até maxEntities entidades; devolve true ao terminar tudo.
bool SceneSerializer::StepDeserialize(int maxEntities, float& outProgress) {
    int done = 0;
    while (m_PendingIndex < m_PendingEntities.size() && done < maxEntities) {
        const auto& je = m_PendingEntities[m_PendingIndex];
        uint64_t id = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, *m_Scene, id);

        uint64_t parentId = je.value("Parent", (uint64_t)0);
        if (parentId != 0) m_ParentOf[(uint64_t)entity.GetUUID()] = parentId;

        ++m_PendingIndex;
        ++done;
    }

    if (m_PendingIndex >= m_PendingEntities.size()) {
        FinishStepwise();
        outProgress = 1.0f;
        return true;
    }
    outProgress = (float)m_PendingIndex / (float)m_PendingEntities.size();
    return false;
}

// Processa entidades até esgotar maxSeconds de trabalho (medido em tempo
// real): entidades leves (sprites, texto) passam em lote grande, as pesadas
// (glb/textura grande) são cortadas pelo relógio — a janela nunca trava.
bool SceneSerializer::StepDeserializeTime(float maxSeconds, float& outProgress) {
    auto start = std::chrono::steady_clock::now();
    const float budgetSeconds = maxSeconds > 0.0f ? maxSeconds : 0.004f;

    while (m_PendingIndex < m_PendingEntities.size()) {
        const auto& je = m_PendingEntities[m_PendingIndex];
        uint64_t id = je.value("ID", (uint64_t)0);
        Entity entity = detail::DeserializeEntityJson(je, *m_Scene, id);

        uint64_t parentId = je.value("Parent", (uint64_t)0);
        if (parentId != 0) m_ParentOf[(uint64_t)entity.GetUUID()] = parentId;

        ++m_PendingIndex;

        // Checa o orçamento depois de cada entidade (uma entidade pesada que
        // estoure o orçamento sozinha encerra o frame mesmo assim — melhor do
        // que congelar o editor inteiro).
        auto elapsed = std::chrono::duration_cast<std::chrono::microseconds>(
            std::chrono::steady_clock::now() - start).count() / 1'000'000.0;
        if (elapsed >= budgetSeconds) break;
    }

    if (m_PendingIndex >= m_PendingEntities.size()) {
        FinishStepwise();
        outProgress = 1.0f;
        return true;
    }
    outProgress = (float)m_PendingIndex / (float)m_PendingEntities.size();
    return false;
}

bool SceneSerializer::DeserializeFromJson(const json& root) {
    // Loop síncrono dos mesmos passos — mantém o comportamento antigo para
    // quem chama de uma vez (Prefab, Scene::Copy do Play/Stop).
    if (!BeginDeserializeStepwise(root)) return false;
    float progress = 0.0f;
    while (!StepDeserialize(INT_MAX, progress)) { /* sem limite de entidades */ }
    return true;
}

} // namespace kizuri
