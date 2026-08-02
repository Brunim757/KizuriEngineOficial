#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/Timestep.hpp"
#include "kizuri/core/UUID.hpp"
#include <entt/entt.hpp>
#include <glm/glm.hpp>
#include <string>
#include <vector>
#include <unordered_map>

class b2World;
class btDiscreteDynamicsWorld;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btBroadphaseInterface;
class btSequentialImpulseConstraintSolver;
class btCollisionShape;
class btMotionState;

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

    // Cópia profunda de toda a cena, preservando UUIDs — é o que o Play do editor usa: entra em
    // Play numa CÓPIA, edição original fica intocada, Stop só descarta a cópia e volta pro original.
    // MeshRendererComponent é copiado direto (Ref<Mesh>/Material compartilhados, não vai por JSON —
    // ainda não existe um Pipeline de Assets com caminho serializável pra mesh/textura, ver ROADMAP).
    static Ref<Scene> Copy(const Ref<Scene>& source);

    // Hierarquia: reparenta 'child' embaixo de 'newParent'. Passar uma
    // Entity inválida desanexa (vira entidade de topo). Rejeita operações
    // que criariam ciclos (ex: tornar um ancestral filho do próprio
    // descendente).
    void SetParent(Entity child, Entity newParent);
    Entity GetEntityByUUID(UUID id);

    // Transform mundial acumulado subindo a cadeia de pais — é o que
    // Renderer2D/3D e física devem usar, não TransformComponent::GetTransform()
    // isolado (que é só o transform local em relação ao pai).
    glm::mat4 GetWorldTransform(Entity entity);

    // Picking por raio (usado pelo editor pra selecionar clicando no
    // viewport): testa contra o AABB local de cada MeshRendererComponent,
    // transformado pro espaço do mundo. Retorna a entidade cujo AABB é
    // atingido mais perto de rayOrigin, ou uma Entity inválida se nenhuma
    // for atingida. É uma aproximação por caixa, não por triângulo — mais
    // barato e preciso o suficiente pra clicar em objetos na cena.
    Entity PickEntity(const glm::vec3& rayOrigin, const glm::vec3& rayDir);

    // Picking 2D (editor, modo 2D do viewport): ponto do mundo -> entidade
    // sob ele. Testa sprite (quad do transform), círculo e texto (bounding
    // do texto medido pela fonte). Retorna a entidade mais próxima ou
    // inválida. Animação de sprite e tilemap são amostrados pelo próprio
    // quad/sprite se tiverem o componente base correspondente.
    Entity PickEntity2D(const glm::vec2& worldPoint);

    void OnRuntimeStart();
    void OnRuntimeStop();

    void OnUpdateRuntime(Timestep ts);

    // Modo editor: duas variantes, uma pra cada modo do viewport (ver
    // EditorLayer — botão 2D/3D na toolbar). Nenhuma mexe na
    // CameraComponent da própria cena; "o jogo visto pela câmera do jogo"
    // é a Game View, ainda não implementada (docs/NOTAS_INTERNAS.md).
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
    void Render2DEntities(); // desenha sprites/círculos/animações/tilemap/texto dentro de um BeginScene aberto
    void SubmitLights(); // junta LightComponent da cena; sem nenhuma, cai num sol default
    void UpdateParticleSystems(Timestep ts); // emissão + integração (só roda em Play, como a física)
    void SubmitParticleSystems(); // monta os lotes de ParticleInstance e chama Renderer3D::SubmitParticles
    void UpdateSpriteAnimations(Timestep ts); // avança FrameTimer dos SpriteAnimationComponent
    void UpdateAudio(Timestep ts); // toca/posiciona AudioSourceComponent + atualiza o listener (câmera 3D ativa)
    void OnPhysics2DStart();
    void OnPhysics2DStop();
    void UpdatePhysics2D(Timestep ts);

    void OnPhysics3DStart();
    void OnPhysics3DStop();
    void UpdatePhysics3D(Timestep ts);

    std::string m_Name;
    entt::registry m_Registry;
    std::unordered_map<UUID, entt::entity> m_EntityMap;
    uint32_t m_ViewportWidth = 0, m_ViewportHeight = 0;

    b2World* m_PhysicsWorld2D = nullptr;
    btDiscreteDynamicsWorld* m_PhysicsWorld3D = nullptr;
    btDefaultCollisionConfiguration* m_CollisionConfig = nullptr;
    btCollisionDispatcher* m_Dispatcher = nullptr;
    btBroadphaseInterface* m_Broadphase = nullptr;
    btSequentialImpulseConstraintSolver* m_Solver = nullptr;
    std::vector<btCollisionShape*> m_PhysicsShapes3D;
    std::vector<btMotionState*> m_PhysicsMotionStates3D;

    friend class Entity;
};

} // namespace kizuri
