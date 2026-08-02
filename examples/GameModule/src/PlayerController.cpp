#include "PlayerController.hpp"

using namespace kizuri;

void PlayerController::OnCreate() {
    KZ_INFO("PlayerController: entidade '{0}' pronta.", GetEntity().GetName());
}

void PlayerController::OnUpdate(Timestep ts) {
    auto& transform = GetComponent<TransformComponent>();

    glm::vec3 velocity{ 0.0f };
    if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  velocity.x -= 1.0f;
    if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) velocity.x += 1.0f;
    if (Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up))    velocity.y += 1.0f;
    if (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down))  velocity.y -= 1.0f;

    if (glm::length(velocity) > 0.0f) velocity = glm::normalize(velocity);
    transform.Translation += velocity * m_Speed * (float)ts;
}
