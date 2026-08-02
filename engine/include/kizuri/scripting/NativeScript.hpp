#pragma once
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/core/Timestep.hpp"

namespace kizuri {

// NativeScript é a interface que todo "script" de jogo implementa nesta
// primeira versão da engine (C++ nativo). Quando a KZScript amadurecer,
// o transpiler dela irá gerar classes que herdam da mesma interface —
// então jogos escritos hoje em C++ continuarão compatíveis.
class NativeScript {
public:
    virtual ~NativeScript() = default;

    template<typename T>
    T& GetComponent() { return m_Entity.GetComponent<T>(); }

    Entity GetEntity() { return m_Entity; }

    // Chamado pela Scene antes de OnCreate(), pra vincular o script à
    // entidade/cena de verdade. Sem isso, m_Entity fica default-construído
    // (m_Scene == nullptr) e qualquer GetComponent()/GetEntity() dentro de
    // OnCreate()/OnUpdate() derruba o processo.
    void BindEntity(Entity entity) { m_Entity = entity; }

protected:
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(Timestep ts) {}

private:
    Entity m_Entity;
    friend class Scene;
};

} // namespace kizuri
