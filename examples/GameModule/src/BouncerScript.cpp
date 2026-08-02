#include "BouncerScript.hpp"

using namespace kizuri;

void BouncerScript::OnUpdate(Timestep ts) {
    // Sem Rigidbody2D a engine (Release) não asserta — GetComponent vira crash
    // silencioso. Exige o componente; se faltar, só avisa uma vez.
    if (!GetEntity().HasComponent<Rigidbody2DComponent>()) {
        static bool s_Warned = false;
        if (!s_Warned) {
            KZ_WARN("BouncerScript em '{0}': adicione Rigidbody2D + BoxCollider2D.",
                    GetEntity().GetName());
            s_Warned = true;
        }
        (void)ts;
        return;
    }
    auto& rb = GetComponent<Rigidbody2DComponent>();
    if (Input::IsKeyPressed(Key::Space))
        rb.ApplyLinearImpulse({ 0.0f, m_JumpImpulse });
    (void)ts;
}
