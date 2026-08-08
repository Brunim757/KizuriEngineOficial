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
class btDiscreteDynamicsWorld;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btCollisionShape;
class btMotionState;
class btPairCachingGhostObject;
class btKinematicCharacterController;

namespace kizuri {

class Entity;
class PerspectiveCamera;

// Scene é o "mundo" da Kizuri Engine: guarda todas as entidades ECS (EnTT),
// controla física 2D (Box2D) e 3D (Bullet), e expõe callbacks de runtime.
class Scene {
public:
    Scene(std::string name = "Cena sem nome");
    ~Scene();

    Entity CreateEntity(const std::string& name = std::string());
    Entity CreateEntityWithUUID(uint64_t uuid, const std::string& name);
    void DestroyEntity(Entity entity);

    // Instancia um .kzprefab. Em runtime (Play/KizuriGame) também cria
    // corpos de física e dispara OnCreate do NativeScript, se houver.
    Entity Instantiate(const std::string& prefabPath, const glm::vec3& position = { 0.0f, 0.0f, 0.0f });

    // Pedido diferido: a troca real acontece no fim do frame (host chama
    // PollPendingLoad). Evita destruir a cena no meio de OnUpdate/colisão.
    void RequestLoad(const std::string& scenePath);
    bool PollPendingLoad(std::string& outPath);

    // Raycast 2D contra o mundo Box2D (só tem efeito durante o Play, quando
    // o mundo existe). Devolve a primeira entidade atingida (uma que tenha
    // Rigidbody2D + Collider), o ponto do impacto e a fração [0,1] ao longo
    // do segmento. Retorna false se não acertar nada.
    bool Raycast2D(const glm::vec2& from, const glm::vec2& to,
                   Entity& outEntity, glm::vec2& outPoint, float& outFraction);

    // OverlapCircle 2D: true se alguma entidade com collider tocar o círculo
    // (só durante o Play). Devolve a entidade mais próxima.
    bool OverlapCircle2D(const glm::vec2& center, float radius, Entity& outEntity);

    // Raycast 3D (Bullet): da origem até o destino (só durante o Play).
    // Devolve a primeira entidade atingida (Rigidbody3D + collider), o ponto
    // do impacto e a fração [0,1]. false = não acertou nada.
    bool Raycast3D(const glm::vec3& from, const glm::vec3& to,
                   Entity& outEntity, glm::vec3& outPoint, float& outFraction);

    // OverlapSphere 3D: true se alguma entidade com collider tocar a esfera
    // (só durante o Play). Devolve uma entidade atingida.
    bool OverlapSphere3D(const glm::vec3& center, float radius, Entity& outEntity);
    // Caixa (AABB) centrada em 'center' com metades 'halfExtents' tocando algo.
    bool OverlapBox3D(const glm::vec3& center, const glm::vec3& halfExtents, Entity& outEntity);
    // TODAS as entidades tocadas pela esfera (área de dano, sensor).
    bool OverlapSphereAll3D(const glm::vec3& center, float radius, std::vector<Entity>& outEntities);

    // Duplica 'source' e toda a subárvore dela, com UUIDs novos (e um leve
    // deslocamento pra não nascer em cima do original). Devolve a raiz nova.
    Entity DuplicateEntity(Entity source);

    // UI: o host entrega a posição do mouse em NDC relativo ao viewport
    // (x/y em [-1,1], y pra cima) + estado do botão esquerdo. O Scene usa
    // pra hit-test dos UIButtonComponent (hover/clique) e o RenderUI desenha
    // os UIRect/TextComponent descendentes de cada UICanvas em espaço de tela.
    void SetUIMouseNDC(const glm::vec2& ndc, bool leftMouseDown);

    bool IsRuntime() const { return m_Running; }

    // Cópia profunda de toda a cena, preservando UUIDs — é o que o Play do editor usa.
    static Ref<Scene> Copy(const Ref<Scene>& source);

    void SetParent(Entity child, Entity newParent);
    Entity GetEntityByUUID(UUID id);

    // True se a entidade está ATIVA: ela mesma e TODOS os ancestrais têm o
    // flag Active ligado. Inativa não é desenhada nem atualizada.
    bool IsEntityActive(Entity entity);

    // Avança as câmeras com CameraFollowComponent atrás do alvo (runtime).
    void UpdateCameraFollowers(Timestep ts);

    glm::mat4 GetWorldTransform(Entity entity);

    Entity PickEntity(const glm::vec3& rayOrigin, const glm::vec3& rayDir);
    Entity PickEntity2D(const glm::vec2& worldPoint);

    void OnRuntimeStart();
    void OnRuntimeStop();

    void OnUpdateRuntime(Timestep ts);

    // Só a LÓGICA do runtime (scripts, física, partículas, áudio) — sem
    // renderizar. O editor usa isso no Play pra rodar o jogo UMA vez e
    // desenhar em vários alvos (viewport com câmera do editor + Game View
    // com a câmera do jogador).
    void OnUpdateRuntimeLogic(Timestep ts);

    // Renderiza a cena do runtime (3D → 2D → UI) no framebuffer ATUALMENTE
    // VINCULADO, sem rodar lógica (física/scripts/partículas). Usado pelo
    // painel "Game View" do editor: o Play roda o update uma vez (no
    // viewport) e a aba do jogo só RE-renderiza a mesma cena pro FBO dela.
    void RenderRuntimeView();

    // Renderiza a cena do runtime com uma câmera FORNECIDA (ex.: a câmera do
    // editor) em vez da câmera primária — é o viewport durante o Play:
    // você voa pela cena enquanto o jogo roda.
    void RenderRuntimeWithEditorCamera(class PerspectiveCamera& editorCamera);

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
    void UpdateCharacterControllers(Timestep ts); // cinematico (gravidade + chão)
    void UpdateTimelines(Timestep ts);             // keyframes de transform (cutscene)
    void UpdateParticleSystems(Timestep ts);
    void SubmitParticleSystems();
    void UpdateSpriteAnimations(Timestep ts);
    void UpdateAnimators(Timestep ts); // avança o relógio dos AnimatorComponent (roda em edição p/ preview)
    void UpdateAudio(Timestep ts);
    void RenderUI();
    void UpdateUIPointer();
    void CollectUIChildren(entt::entity parent, std::vector<entt::entity>& outStack) const;
    void OnPhysics2DStart();
    void OnPhysics2DStop();
    void UpdatePhysics2D(Timestep ts);
    void RegisterPhysics2DEntity(Entity entity);
    void UnregisterPhysics2DEntity(Entity entity);
    void BuildTilemapColliders();

    void OnPhysics3DStart();
    void OnPhysics3DStop();
    void UpdatePhysics3D(Timestep ts);
    void RegisterPhysics3DEntity(Entity entity);
    void UnregisterPhysics3DEntity(Entity entity);
    void FlushCollisionEvents();
    void DispatchCollisionBegin(entt::entity self, entt::entity other);
    void DispatchCollisionEnd(entt::entity self, entt::entity other);
    void StartScriptIfNeeded(Entity entity);

    std::string m_Name;
    entt::registry m_Registry;
    std::unordered_map<UUID, entt::entity> m_EntityMap;
    uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;
    bool m_Running = false;
    std::string m_PendingScenePath;

    b2World* m_PhysicsWorld2D = nullptr;
    void* m_ContactListener2D = nullptr; // ContactListener2D* (Box2D)

    // Estado do mouse pra UI (preenchido pelo host via SetUIMouseNDC).
    glm::vec2 m_UIMouseNDC{ 0.0f };
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
    // Dados de altura dos heightfields de terreno (o shape referencia o
    // ponteiro — precisa viver enquanto a cena existir).
    std::vector<std::vector<float>> m_PhysicsHeightfieldData3D;
    // Character controllers (btKinematicCharacterController) por handle de entidade.
    std::unordered_map<uint32_t, btKinematicCharacterController*> m_CharacterControllers3D;
    std::vector<btPairCachingGhostObject*> m_CharacterGhosts3D;

    std::vector<std::pair<entt::entity, entt::entity>> m_CollisionBeginQueue;
    std::vector<std::pair<entt::entity, entt::entity>> m_CollisionEndQueue;
    // Pares ordenados (min,max) de corpos 3D em contato no frame anterior.
    std::unordered_set<uint64_t> m_ActiveContacts3D;

    friend class Entity;
};

} // namespace kizuri
