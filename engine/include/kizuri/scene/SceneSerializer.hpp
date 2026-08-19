#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/ecs/Scene.hpp"
#include <nlohmann/json.hpp>
#include <string>
#include <unordered_map>

namespace kizuri {

class SceneSerializer {
public:
    explicit SceneSerializer(const Ref<Scene>& scene) : m_Scene(scene) {}

    void Serialize(const std::string& filepath);
    bool Deserialize(const std::string& filepath);

    nlohmann::json SerializeToJson();
    bool DeserializeFromJson(const nlohmann::json& root);

    bool BeginDeserializeStepwiseFile(const std::string& filepath);
    bool BeginDeserializeStepwise(const nlohmann::json& root);
    bool StepDeserialize(int maxEntities, float& outProgress);
    bool StepDeserializeTime(float maxSeconds, float& outProgress);

private:
    void FinishStepwise();

    Ref<Scene> m_Scene;
    nlohmann::json m_PendingEntities;
    std::size_t m_PendingIndex = 0;
    std::unordered_map<uint64_t, uint64_t> m_ParentOf;
};

}
