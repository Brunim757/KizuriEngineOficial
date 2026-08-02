#pragma once
#include <Kizuri.hpp>

// Exemplo 1 — rotação contínua no eixo Y. Mostra o essencial de qualquer
// script: OnUpdate(), TransformComponent e o Timestep (tempo do frame em
// segundos, serve pra velocidade não depender do FPS).
//
// Como testar: crie um cubo 3D (ou adicione MeshRenderer a uma entidade),
// Adicionar Componente > Script Nativo > "RotatorScript" e aperte Play.
class RotatorScript : public kizuri::NativeScript {
protected:
    void OnUpdate(kizuri::Timestep ts) override;

private:
    float m_RotationSpeed = 90.0f; // graus por segundo
};
