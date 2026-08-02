#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/ecs/Scene.hpp"

namespace kizuri {

Scene* NativeScript::GetScene() {
    return m_Entity ? m_Entity.GetScene() : nullptr;
}

Entity NativeScript::Instantiate(const std::string& prefabPath, const glm::vec3& position) {
    Scene* scene = GetScene();
    if (!scene) return {};
    return scene->Instantiate(prefabPath, position);
}

void NativeScript::LoadScene(const std::string& scenePath) {
    Scene* scene = GetScene();
    if (scene) scene->RequestLoad(scenePath);
}

void NativeScript::DestroyEntity() {
    Scene* scene = GetScene();
    if (scene && m_Entity) scene->DestroyEntity(m_Entity);
}

} // namespace kizuri
