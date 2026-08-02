#include "RotatorScript.hpp"

using namespace kizuri;

void RotatorScript::OnUpdate(Timestep ts) {
    auto& transform = GetComponent<TransformComponent>();
    transform.Rotation.y += glm::radians(m_RotationSpeed) * (float)ts;
}
