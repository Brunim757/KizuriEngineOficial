#pragma once
#include <Kizuri.hpp>

// Exemplo 3 — física 2D. Precisa de Rigidbody2DComponent +
// BoxCollider2DComponent na MESMA entidade. Aperta ESPAÇO no Play pra
// aplicar um impulso pra cima; a gravidade e um chão com colisor fazem o
// resto. Mostra a API de física que a engine expõe pros scripts
// (Rigidbody2DComponent::ApplyLinearImpulse).
//
// Como testar: crie um quadrado 2D (sprite), adicione Rigidbody2D
// (Dinâmico) + Box Collider 2D + Script Nativo > "BouncerScript". Crie
// outro sprite maior embaixo como chão com Rigidbody2D Estático + colisor.
// Aperte Play e ESPAÇO.
class BouncerScript : public kizuri::NativeScript {
protected:
    void OnUpdate(kizuri::Timestep ts) override;

private:
    float m_JumpImpulse = 6.0f; // impulso vertical aplicado no centro
};
