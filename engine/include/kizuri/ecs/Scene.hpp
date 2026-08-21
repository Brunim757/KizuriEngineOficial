#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/Timestep.hpp"
#include "kizuri/core/UUID.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>

class b2World;
class b2Body;
class btDiscreteDynamicsWorld;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btCollisionShape;
class btMotionState;
class btPairCachingGhostObject;
class btKinematicCharacterController;
class btTriangleMesh;

namespace kizuri {

class Entity;
class PerspectiveCamera;
class NavGrid;
struct ChunkWorldComponent;

class Scene {
public:
    Scene(std::string name = "Cena sem nome");
    ~Scene();

    bool m_InScriptUpdate = false;
    std::vector<entt::entity> m_PendingDestroy;
    void FlushPendingDestroys();

    Entity CreateEntity(const std::string& name = std::string());
    Entity CreateEntityWithUUID(uint64_t uuid, const std::string& name);
    void DestroyEntity(Entity entity);

    Entity FindEntityByName(const std::string& tag);

    void DestroyEntityNow(Entity entity);

    Entity Instantiate(const std::string& prefabPath, const glm::vec3& position = { 0.0f, 0.0f, 0.0f });

    void RequestLoad(const std::string& scenePath);
    bool PollPendingLoad(std::string& outPath);

    bool Raycast2D(const glm::vec2& from, const glm::vec2& to,
                   Entity& outEntity, glm::vec2& outPoint, float& outFraction);

    bool OverlapCircle2D(const glm::vec2& center, float radius, Entity& outEntity);

    bool Raycast3D(const glm::vec3& from, const glm::vec3& to,
                   Entity& outEntity, glm::vec3& outPoint, float& outFraction);

    bool OverlapSphere3D(const glm::vec3& center, float radius, Entity& outEntity);

    bool OverlapBox3D(const glm::vec3& center, const glm::vec3& halfExtents, Entity& outEntity);

    bool OverlapSphereAll3D(const glm::vec3& center, float radius, std::vector<Entity>& outEntities);

    void SetRigidbody3DGravityScale(Entity entity, float scale);
    void SetRigidbody3DDamping(Entity entity, float linear, float angular);

    void BakeLightmap(Entity entity);

    void RebuildNavGrid(Entity gridEntity);

    void SetNavDestination(Entity agent, const glm::vec3& destination);
    void StopNavAgent(Entity agent);
    bool NavAgentHasPath(Entity agent) const;
    float NavAgentRemainingDistance(Entity agent) const;
    bool NavAgentReached(Entity agent) const;

    Entity DuplicateEntity(Entity source);

    void SetUIMouseNDC(const glm::vec2& ndc, bool leftMouseDown);

    bool IsRuntime() const { return m_Running; }

    static Ref<Scene> Copy(const Ref<Scene>& source);

    void SetParent(Entity child, Entity newParent);
    Entity GetEntityByUUID(UUID id);

    bool IsEntityActive(Entity entity);

    void UpdateCameraFollowers(Timestep ts);

    glm::mat4 GetWorldTransform(Entity entity);

    Entity PickEntity(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    Entity PickEntity2D(const glm::vec2& worldPoint);

    void OnRuntimeStart();
    void OnRuntimeStop();

    void OnUpdateRuntime(Timestep ts);

    void OnUpdateRuntimeLogic(Timestep ts);

    void UpdateChunkWorld(Timestep ts);

    void RenderRuntimeView();

    void RenderRuntimeWithEditorCamera(class PerspectiveCamera& editorCamera);

    bool HasPrimaryCamera();

    void OnUpdateEditor3D(Timestep ts, class PerspectiveCamera& editorCamera);
    void OnUpdateEditor2D(Timestep ts, class OrthographicCamera& editorCamera);
    void OnViewportResize(uint32_t width, uint32_t height);

    entt::registry& GetRegistry() { return m_Registry; }
    const std::string& GetName() const { return m_Name; }

    template<typename... Components>
    auto GetAllEntitiesWith() { return m_Registry.view<Components...>(); }

private:
    void RenderScene2D(class OrthographicCamera* overrideCamera = nullptr);
    void RenderScene3D(class PerspectiveCamera* overrideCamera = nullptr);
    void Render2DEntities();
    void SubmitLights();
    void UpdateCharacterControllers(Timestep ts);
    void UpdateTimelines(Timestep ts);
    void UpdateParticleSystems(Timestep ts);
    void SubmitParticleSystems();
    void UpdateSpriteAnimations(Timestep ts);
    void UpdateAnimators(Timestep ts);
    void UpdateAudio(Timestep ts);
    void UnloadChunk(int cx, int cz);
    bool LoadChunk(int cx, int cz, ChunkWorldComponent& cw);
    void SaveChunkWorld(const ChunkWorldComponent& cw);
    void ReloadAllScripts();
    void RenderUI();
    void UpdateUIPointer();
    void CollectUIChildren(entt::entity parent, std::vector<entt::entity>& outStack) const;
    void OnPhysics2DStart();
    void OnPhysics2DStop();
    void UpdatePhysics2D(Timestep ts);
    void RegisterPhysics2DEntity(Entity entity);
    void UnregisterPhysics2DEntity(Entity entity);
    void BuildTilemapColliders();
    void RebuildDirtyTilemapColliders();

    void OnPhysics3DStart();
    void OnPhysics3DStop();
    void UpdatePhysics3D(Timestep ts);
    void RegisterPhysics3DEntity(Entity entity);
    void UnregisterPhysics3DEntity(Entity entity);
    void FlushCollisionEvents();
    void DispatchCollisionBegin(entt::entity self, entt::entity other);
    void DispatchCollisionEnd(entt::entity self, entt::entity other);
    void StartScriptIfNeeded(Entity entity);

    void BuildNavGrids();
    void UpdateEnemyAI(Timestep ts);
    void UpdateNavAgents(Timestep ts);
    NavGrid* FindGridNear(const glm::vec3& pos) const;

    std::string m_Name;
    entt::registry m_Registry;
    std::unordered_map<UUID, entt::entity> m_EntityMap;
    uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
    bool m_Running = false;
    std::string m_PendingScenePath;

    b2World* m_PhysicsWorld2D = nullptr;
    void* m_ContactListener2D = nullptr;

    std::vector<b2Body*> m_TilemapBodies2D;

    glm::vec2 m_UIMouseNDC{ 0.0f };
    glm::vec3 m_LastListenerPos{ 0.0f };
    bool m_UIMouseValid = false;
    bool m_UIMouseDown = false;
    bool m_UIMouseDownPrev = false;

    btDiscreteDynamicsWorld* m_PhysicsWorld3D = nullptr;
    btDefaultCollisionConfiguration* m_CollisionConfig = nullptr;
    btCollisionDispatcher* m_Dispatcher = nullptr;
    btBroadphaseInterface* m_Broadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_Solver = nullptr;
    std::vector<btCollisionShape*> m_PhysicsShapes3D;
    std::vector<btMotionState*> m_PhysicsMotionStates3D;

    std::vector<btTriangleMesh*> m_PhysicsMeshes3D;

    std::unordered_map<uint32_t, btKinematicCharacterController*> m_CharacterControllers3D;
    std::vector<btPairCachingGhostObject*> m_CharacterGhosts3D;

    std::vector<std::pair<entt::entity, entt::entity>> m_CollisionBeginQueue;
    std::vector<std::pair<entt::entity, entt::entity>> m_CollisionEndQueue;

    std::unordered_set<uint64_t> m_ActiveContacts3D;

    friend class Entity;
};

}
