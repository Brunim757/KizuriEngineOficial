#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Scene.hpp"
#include <nlohmann/json.hpp>
#include <string>

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

private:
    Ref<Scene> m_Scene;
};

} // namespace kizuri
