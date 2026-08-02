#include "BouncerScript.hpp"

using namespace kizuri;

void BouncerScript::OnUpdate(Timestep ts) {
    auto& rb = GetComponent<Rigidbody2DComponent>();
    if (Input::IsKeyPressed(Key::Space))
        rb.ApplyLinearImpulse({ 0.0f, m_JumpImpulse });
    (void)ts;
}
