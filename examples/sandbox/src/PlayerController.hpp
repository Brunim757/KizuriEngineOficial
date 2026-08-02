#pragma once
#include <Kizuri.hpp>

// Exemplo de "script de jogo" na primeira geração da Kizuri Engine: uma
// classe C++ herdando de kizuri::NativeScript. Quando a KZScript (DSL
// própria) amadurecer, o compilador dela vai gerar classes equivalentes,
// então a API de OnCreate/OnUpdate/OnDestroy é o contrato estável.
class PlayerController : public kizuri::NativeScript {
protected:
    void OnCreate() override;
    void OnUpdate(kizuri::Timestep ts) override;

private:
    float m_Speed = 5.0f;
};
