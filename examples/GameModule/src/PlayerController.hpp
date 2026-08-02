#pragma once
#include <Kizuri.hpp>

// Exemplo 2 — movimento com WASD/setas. Mostra Input::IsKeyPressed,
// OnCreate() e a posição do Transform. Funciona no 2D e no 3D.
//
// Como testar: crie um sprite 2D (arraste um .png pro viewport) ou um cubo
// 3D, Adicionar Componente > Script Nativo > "PlayerController" e aperte
// Play — WASD/setas movem a entidade.
class PlayerController : public kizuri::NativeScript {
protected:
    void OnCreate() override;
    void OnUpdate(kizuri::Timestep ts) override;

private:
    float m_Speed = 5.0f; // unidades por segundo
};
