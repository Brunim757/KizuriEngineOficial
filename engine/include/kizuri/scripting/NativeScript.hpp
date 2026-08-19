#pragma once
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/core/Timestep.hpp"
#include <glm/glm.hpp>
#include <string>

namespace kizuri {

class Scene;

class NativeScript {
public:
    virtual ~NativeScript() = default;

    template<typename T>
    T& GetComponent() { return m_Entity.GetComponent<T>(); }

    Entity GetEntity() { return m_Entity; }
    Scene* GetScene();

    void BindEntity(Entity entity) { m_Entity = entity; }

    Entity Instantiate(const std::string& prefabPath, const glm::vec3& position = { 0.0f, 0.0f, 0.0f });
    void LoadScene(const std::string& scenePath);
    void DestroyEntity();

protected:
    virtual void OnCreate() {}
    virtual void OnDestroy() {}
    virtual void OnUpdate([[maybe_unused]] Timestep ts) {}

    virtual void OnCollisionBegin(Entity other) { (void)other; }
    virtual void OnCollisionEnd(Entity other) { (void)other; }

    virtual void OnEnemyAttack(float amount) { (void)amount; }

private:
    Entity m_Entity;
    friend class Scene;
};

}
