#pragma once
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/core/Timestep.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

class Scene;

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
    Scene* GetScene();

    // Chamado pela Scene antes de OnCreate(), pra vincular o script à
    // entidade/cena de verdade. Sem isso, m_Entity fica default-construído
    // (m_Scene == nullptr) e qualquer GetComponent()/GetEntity() dentro de
    // OnCreate()/OnUpdate() derruba o processo.
    void BindEntity(Entity entity) { m_Entity = entity; }

    // API de gameplay — wrappers pra Scene, pra o script não precisar
    // carregar headers internos. Prefab gera UUIDs novos; física/scripts
    // da entidade nascem se o runtime já estiver ativo (Play / KizuriGame).
    Entity Instantiate(const std::string& prefabPath, const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
    void LoadScene(const std::string& scenePath);
    void DestroyEntity();

protected:
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate(Timestep ts) {}

    // other pode ser inválido (ex.: colisão com tilemap estático sem entidade).
    virtual void OnCollisionBegin(Entity other) { (void)other; }
    virtual void OnCollisionEnd(Entity other) { (void)other; }

private:
    Entity m_Entity;
    friend class Scene;
};

} // namespace kizuri
