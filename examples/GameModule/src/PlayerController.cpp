#include "PlayerController.hpp"

using namespace kizuri;

void PlayerController::OnCreate() {
    KZ_INFO("PlayerController: entidade '{0}' pronta.", GetEntity().GetName());
}

void PlayerController::OnUpdate(Timestep ts) {
    glm::vec2 wish{ 0.0f };
    if (Input::IsKeyPressed(Key::A) || Input::IsKeyPressed(Key::Left))  wish.x -= 1.0f;
    if (Input::IsKeyPressed(Key::D) || Input::IsKeyPressed(Key::Right)) wish.x += 1.0f;
    if (Input::IsKeyPressed(Key::W) || Input::IsKeyPressed(Key::Up))    wish.y += 1.0f;
    if (Input::IsKeyPressed(Key::S) || Input::IsKeyPressed(Key::Down))  wish.y -= 1.0f;

    if (glm::length(wish) > 0.0f) wish = glm::normalize(wish);
    wish *= m_Speed;

    // Com Rigidbody2D Dynamic, mover o Transform é sobrescrito pela física —
    // use SetLinearVelocity. Sem corpo, cai no Transform direto.
    if (GetEntity().HasComponent<Rigidbody2DComponent>()) {
        auto& rb = GetComponent<Rigidbody2DComponent>();
        if (rb.Type == Rigidbody2DComponent::BodyType::Dynamic) {
            glm::vec2 v = rb.GetLinearVelocity();
            rb.SetLinearVelocity({ wish.x, v.y }); // preserva Y (gravidade); pulo = BouncerScript
            return;
        }
    }

    auto& transform = GetComponent<TransformComponent>();
    transform.Translation.x += wish.x * (float)ts;
    transform.Translation.y += wish.y * (float)ts;
}
