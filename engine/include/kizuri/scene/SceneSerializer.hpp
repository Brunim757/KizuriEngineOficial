#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Scene.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace kizuri {

// Serializa/desserializa uma Scene inteira (entidades + componentes) para
// arquivos .kzscene em formato JSON legível, usados pelo Editor.
class SceneSerializer {
public:
    explicit SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

    void Serialize(const std::string& filepath);
    bool Deserialize(const std::string& filepath);

    // Versões em memória, sem tocar em disco — usadas por Scene::Copy() pro
    // snapshot do Play/Stop do editor (ver EditorLayer::OnScenePlay/OnSceneStop).
    nlohmann::json SerializeToJson();
    bool DeserializeFromJson(const nlohmann::json& root);

    // ---- Carregamento INCREMENTAL (projetos grandes sem travar o editor) ----
    // Deserialize() é síncrono: projetos muito grandes (milhares de entidades,
    // meshes/texturas pesadas) bloqueavam o main thread — a janela congelava
    // e não fechava. Estas funções permitem carregar em LOTE por frame: o
    // editor processa algumas entidades a cada OnUpdate, mantendo a janela
    // responsiva e mostrando uma barra de progresso.
    //   BeginDeserializeStepwiseFile(path)  -> prepara (lê e parseia o JSON)
    //   StepDeserialize(maxEntities, &progress)  -> processa até N entidades
    //   StepDeserializeTime(maxSeconds, &progress) -> processa até esgotar o
    //        orçamento de tempo (entidades baratas carregam aos milhares por
    //        frame; as pesadas são cortadas pelo relógio)
    // Ambas devolvem true quando TERMINOU (e as hierarquias já foram
    // resolvidas). Deserialize() continua funcionando (é o loop síncrono
    // dessas mesmas funções) — Prefab/Scene::Copy não mudam de comportamento.
    bool BeginDeserializeStepwiseFile(const std::string& filepath);
    bool BeginDeserializeStepwise(const nlohmann::json& root);
    bool StepDeserialize(int maxEntities, float& outProgress);
    bool StepDeserializeTime(float maxSeconds, float& outProgress);

private:
    void FinishStepwise(); // resolve pais pendentes no fim do lote final

    Ref<Scene> m_Scene;
    nlohmann::json m_PendingEntities;             // array de entidades do root
    std::size_t m_PendingIndex = 0;               // quantas já foram criadas
    std::unordered_map<uint64_t, uint64_t> m_ParentOf; // uuid filho -> uuid pai
};

} // namespace kizuri
