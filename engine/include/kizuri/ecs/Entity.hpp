#pragma once
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/core/Log.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <vector>

namespace kizuri {



class Entity {
public:
    Entity() = default;
    Entity(entt::entity handle, Scene* scene) : m_EntityHandle(handle), m_Scene(scene) {}

    template<typename T, typename... Args>
    T& AddComponent(Args&&... args) {
        KZ_ASSERT(!HasComponent<T>(), "Entidade já possui esse componente!");
        return m_Scene->GetRegistry().emplace<T>(m_EntityHandle, std::forward<Args>(args)...);
    }

    template<typename T, typename... Args>
    T& AddOrReplaceComponent(Args&&... args) {
        return m_Scene->GetRegistry().emplace_or_replace<T>(m_EntityHandle, std::forward<Args>(args)...);
    }

    template<typename T>
    T& GetComponent() {
        KZ_ASSERT(HasComponent<T>(), "Entidade não possui esse componente!");
        return m_Scene->GetRegistry().get<T>(m_EntityHandle);
    }

    template<typename T>
    bool HasComponent() {
        return m_Scene->GetRegistry().all_of<T>(m_EntityHandle);
    }

    template<typename T>
    void RemoveComponent() {
        m_Scene->GetRegistry().remove<T>(m_EntityHandle);
    }

    entt::entity GetHandle() const { return m_EntityHandle; }
    operator bool() const { return m_EntityHandle != entt::null; }
    operator entt::entity() const { return m_EntityHandle; }
    operator uint32_t() const { return (uint32_t)m_EntityHandle; }

    bool operator==(const Entity& other) const { return m_EntityHandle == other.m_EntityHandle && m_Scene == other.m_Scene; }
    bool operator!=(const Entity& other) const { return !(*this == other); }

    UUID GetUUID() { return GetComponent<IDComponent>().ID; }
    const std::string& GetName() { return GetComponent<TagComponent>().Tag; }
    Scene* GetScene() { return m_Scene; }

    
    
    void SetParent(Entity parent) { m_Scene->SetParent(*this, parent); }

    Entity GetParent() {
        auto& rel = GetComponent<RelationshipComponent>();
        if (!rel.Parent.IsValid()) return {};
        return m_Scene->GetEntityByUUID(rel.Parent);
    }

    std::vector<Entity> GetChildren() {
        std::vector<Entity> result;
        auto& rel = GetComponent<RelationshipComponent>();
        result.reserve(rel.Children.size());
        for (UUID id : rel.Children) {
            Entity child = m_Scene->GetEntityByUUID(id);
            if (child) result.push_back(child);
        }
        return result;
    }

    
    
    glm::mat4 GetLocalTransform() { return GetComponent<TransformComponent>().GetTransform(); }
    glm::mat4 GetWorldTransform() { return m_Scene->GetWorldTransform(*this); }

private:
    entt::entity m_EntityHandle{ entt::null };
    Scene* m_Scene = nullptr;
};

} 
