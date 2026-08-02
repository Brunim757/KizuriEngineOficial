#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scene/SceneSerializer.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/core/Log.hpp"

#include <box2d/box2d.h>
#include <btBulletDynamicsCommon.h>
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <limits>
#include <cmath>

namespace kizuri {

static b2BodyType ToBox2DBody(Rigidbody2DComponent::BodyType type) {
    switch (type) {
        case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
        case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
        case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
    }
    return b2_staticBody;
}

Scene::Scene(std::string name) : m_Name(std::move(name)) {}
Scene::~Scene() = default;

Ref<Scene> Scene::Copy(const Ref<Scene>& source) {
    KZ_TRACE_SCOPE("Scene::Copy");
    Ref<Scene> copy = CreateRef<Scene>(source->m_Name);
    copy->m_ViewportWidth = source->m_ViewportWidth;
    copy->m_ViewportHeight = source->m_ViewportHeight;

    SceneSerializer srcSerializer(source);
    SceneSerializer dstSerializer(copy);
    dstSerializer.DeserializeFromJson(srcSerializer.SerializeToJson());
    // MeshRenderer/Material agora são serializados (ComponentSerialization.hpp),
    // então o roundtrip JSON acima já carrega mesh, material e texturas.

    return copy;
}

Entity Scene::CreateEntity(const std::string& name) {
    return CreateEntityWithUUID(0, name);
}

Entity Scene::CreateEntityWithUUID(uint64_t uuid, const std::string& name) {
    Entity entity{ m_Registry.create(), this };
    UUID id = (uuid == 0) ? UUID() : UUID(uuid);

    entity.AddComponent<IDComponent>(id);
    entity.AddComponent<TransformComponent>();
    entity.AddComponent<RelationshipComponent>();
    auto& tag = entity.AddComponent<TagComponent>();
    tag.Tag = name.empty() ? "Entidade" : name;

    m_EntityMap[id] = entity.GetHandle();
    return entity;
}

void Scene::DestroyEntity(Entity entity) {
    if (!entity) return;

    // Destrói a subárvore inteira primeiro (cópia da lista: DestroyEntity
    // recursivo vai mexer em RelationshipComponent enquanto iteramos).
    if (entity.HasComponent<RelationshipComponent>()) {
        auto childrenCopy = entity.GetComponent<RelationshipComponent>().Children;
        for (UUID childId : childrenCopy) {
            Entity child = GetEntityByUUID(childId);
            if (child) DestroyEntity(child);
        }
    }

    // Desanexa do pai (remove da lista de filhos dele) antes de sumir.
    SetParent(entity, {});

    if (entity.HasComponent<IDComponent>())
        m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);

    m_Registry.destroy(entity.GetHandle());
}

Entity Scene::GetEntityByUUID(UUID id) {
    auto it = m_EntityMap.find(id);
    if (it == m_EntityMap.end()) return {};
    return Entity{ it->second, this };
}

void Scene::SetParent(Entity child, Entity newParent) {
    if (!child || !child.HasComponent<RelationshipComponent>()) return;

    // Anti-ciclo primeiro, ANTES de mexer em qualquer lista — uma operação
    // rejeitada não pode deixar a hierarquia num estado inconsistente
    // (ex: filho removido do pai antigo mas nunca anexado ao novo).
    if (newParent) {
        for (Entity walker = newParent; walker; walker = walker.GetParent()) {
            if (walker.GetUUID() == child.GetUUID()) {
                KZ_CORE_ERROR("SetParent: a operação criaria um ciclo na hierarquia e foi ignorada.");
                return;
            }
        }
    }

    auto& rel = child.GetComponent<RelationshipComponent>();

    // Remove da lista de filhos do pai anterior, se houver.
    if (rel.Parent.IsValid()) {
        Entity oldParent = GetEntityByUUID(rel.Parent);
        if (oldParent && oldParent.HasComponent<RelationshipComponent>()) {
            auto& oldKids = oldParent.GetComponent<RelationshipComponent>().Children;
            oldKids.erase(std::remove(oldKids.begin(), oldKids.end(), child.GetUUID()), oldKids.end());
        }
    }

    if (!newParent) {
        rel.Parent = UUID::Invalid();
        return;
    }

    rel.Parent = newParent.GetUUID();
    newParent.GetComponent<RelationshipComponent>().Children.push_back(child.GetUUID());
}

glm::mat4 Scene::GetWorldTransform(Entity entity) {
    if (!entity || !entity.HasComponent<TransformComponent>()) return glm::mat4(1.0f);

    glm::mat4 local = entity.GetComponent<TransformComponent>().GetTransform();
    Entity parent = entity.GetParent();
    if (parent) return GetWorldTransform(parent) * local;
    return local;
}

// Teste de interseção raio-AABB pelo método dos slabs (padrão, O(1),
// funciona com qualquer direção de raio incluindo eixos paralelos às faces).
static bool RayIntersectsAABB(const glm::vec3& origin, const glm::vec3& dir,
                               const glm::vec3& bmin, const glm::vec3& bmax, float& outT) {
    float tmin = 0.0f, tmax = std::numeric_limits<float>::max();
    for (int i = 0; i < 3; ++i) {
        if (std::abs(dir[i]) < 1e-8f) {
            if (origin[i] < bmin[i] || origin[i] > bmax[i]) return false;
            continue;
        }
        float invD = 1.0f / dir[i];
        float t0 = (bmin[i] - origin[i]) * invD;
        float t1 = (bmax[i] - origin[i]) * invD;
        if (t0 > t1) std::swap(t0, t1);
        tmin = std::max(tmin, t0);
        tmax = std::min(tmax, t1);
        if (tmin > tmax) return false;
    }
    outT = tmin;
    return true;
}

Entity Scene::PickEntity2D(const glm::vec2& worldPoint) {
    Entity closest;
    float closestArea = std::numeric_limits<float>::max();

    // Sprite: ponto dentro do quad (transform mundial).
    auto sprites = m_Registry.view<TransformComponent, SpriteRendererComponent>();
    for (auto e : sprites) {
        Entity entity{ e, this };
        glm::mat4 inv = glm::inverse(GetWorldTransform(entity));
        glm::vec3 local = glm::vec3(inv * glm::vec4(worldPoint, 0.0f, 1.0f));
        if (local.x < -0.5f || local.x > 0.5f || local.y < -0.5f || local.y > 0.5f) continue;
        glm::mat4 world = GetWorldTransform(entity);
        float area = std::abs(glm::length(glm::vec2(world[0])) * glm::length(glm::vec2(world[1])));
        if (area < closestArea) { closestArea = area; closest = entity; }
    }

    // Círculo: ponto dentro do raio.
    auto circles = m_Registry.view<TransformComponent, CircleRendererComponent>();
    for (auto e : circles) {
        Entity entity{ e, this };
        glm::mat4 inv = glm::inverse(GetWorldTransform(entity));
        glm::vec3 local = glm::vec3(inv * glm::vec4(worldPoint, 0.0f, 1.0f));
        if (local.x * local.x + local.y * local.y > 0.25f) continue;
        if (1.0f < closestArea) { closestArea = 1.0f; closest = entity; }
    }

    // Texto: bounding aproximado pela medida da fonte (linha mais larga ×
    // altura total multilinha, incluindo os descenders).
    auto texts = m_Registry.view<TransformComponent, TextComponent>();
    for (auto e : texts) {
        Entity entity{ e, this };
        auto& tc = texts.get<TextComponent>(e);
        glm::vec3 pos = glm::vec3(GetWorldTransform(entity)[3]);
        float w = TextRenderer::MeasureWidth(tc.Text, tc.FontSize);
        uint32_t lineCount = 1;
        for (char c : tc.Text) if (c == '\n') ++lineCount;
        float h = tc.FontSize * 1.2f * (float)lineCount;
        if (worldPoint.x < pos.x || worldPoint.x > pos.x + w ||
            worldPoint.y < pos.y - h || worldPoint.y > pos.y) continue;
        if (w * h < closestArea) { closestArea = w * h; closest = entity; }
    }

    // Tilemap: ponto dentro do retângulo do mapa inteiro. Os tiles são
    // desenhados a partir da origem subindo (+y), então o retângulo vai de
    // pos.y até pos.y + h (diferente do texto, que desce de pos.y).
    auto tilemaps = m_Registry.view<TransformComponent, TilemapComponent>();
    for (auto e : tilemaps) {
        Entity entity{ e, this };
        auto& tmc = tilemaps.get<TilemapComponent>(e);
        if (tmc.MapWidth == 0 || tmc.MapHeight == 0) continue;
        glm::vec3 pos = glm::vec3(GetWorldTransform(entity)[3]);
        float w = (float)tmc.MapWidth * tmc.TileSize.x;
        float h = (float)tmc.MapHeight * tmc.TileSize.y;
        if (worldPoint.x < pos.x || worldPoint.x > pos.x + w ||
            worldPoint.y < pos.y || worldPoint.y > pos.y + h) continue;
        if (w * h < closestArea) { closestArea = w * h; closest = entity; }
    }

    return closest;
}

Entity Scene::PickEntity(const glm::vec3& rayOrigin, const glm::vec3& rayDir) {
    Entity closest;
    float closestDist = std::numeric_limits<float>::max();

    auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();
    for (auto e : meshes) {
        auto& mr = meshes.get<MeshRendererComponent>(e);
        if (!mr.MeshAsset) continue;

        Entity entity{ e, this };
        glm::mat4 world = GetWorldTransform(entity);
        glm::mat4 invWorld = glm::inverse(world);

        // Testa em espaço local do mesh — evita ter que transformar o AABB
        // (que deixaria de ser eixo-alinhado sob rotação) e mantém o teste
        // simples de slab válido.
        glm::vec3 localOrigin = glm::vec3(invWorld * glm::vec4(rayOrigin, 1.0f));
        glm::vec3 localDir = glm::vec3(invWorld * glm::vec4(rayDir, 0.0f));

        float t;
        if (!RayIntersectsAABB(localOrigin, localDir, mr.MeshAsset->GetBoundsMin(), mr.MeshAsset->GetBoundsMax(), t))
            continue;
        if (t < 0.0f) continue;

        // 't' está em espaço local, não comparável entre entidades com
        // escalas diferentes — converte o ponto de impacto de volta pro
        // mundo e compara a distância real até a origem do raio.
        glm::vec3 worldHit = glm::vec3(world * glm::vec4(localOrigin + localDir * t, 1.0f));
        float dist = glm::length(worldHit - rayOrigin);
        if (dist < closestDist) {
            closestDist = dist;
            closest = entity;
        }
    }

    return closest;
}

void Scene::OnPhysics2DStart() {
    m_PhysicsWorld2D = new b2World({ 0.0f, -9.81f });

    auto view = m_Registry.view<Rigidbody2DComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();

        b2BodyDef bodyDef;
        bodyDef.type = ToBox2DBody(rb2d.Type);
        bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
        bodyDef.angle = transform.Rotation.z;

        b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
        body->SetFixedRotation(rb2d.FixedRotation);
        rb2d.RuntimeBody = body;

        if (entity.HasComponent<BoxCollider2DComponent>()) {
            auto& bc2d = entity.GetComponent<BoxCollider2DComponent>();
            b2PolygonShape shape;
            shape.SetAsBox(bc2d.Size.x * transform.Scale.x, bc2d.Size.y * transform.Scale.y,
                            { bc2d.Offset.x, bc2d.Offset.y }, 0.0f);

            b2FixtureDef fixtureDef;
            fixtureDef.shape = &shape;
            fixtureDef.density = bc2d.Density;
            fixtureDef.friction = bc2d.Friction;
            fixtureDef.restitution = bc2d.Restitution;
            fixtureDef.restitutionThreshold = bc2d.RestitutionThreshold;
            body->CreateFixture(&fixtureDef);
        }
    }

    // Tilemaps sólidos: cada tile cujo valor esteja em SolidTileValues vira
    // um corpo estático (mesclando runs horizontais contíguos numa caixa só,
    // pra não explodir a quantidade de corpos em plataformers). Os corpos
    // usam só a translação mundial do mapa (assumem mapa sem rotação/escala
    // — típico pra tilemaps; se hierarquias rotacionadas virarem comuns,
    // isso precisa ser revisitado).
    auto tilemaps = m_Registry.view<TransformComponent, TilemapComponent>();
    for (auto te : tilemaps) {
        auto& tmc = tilemaps.get<TilemapComponent>(te);
        if (tmc.SolidTileValues.empty() || tmc.MapWidth == 0 || tmc.MapHeight == 0) continue;

        glm::vec3 mapPos = glm::vec3(GetWorldTransform(Entity{ te, this })[3]);
        auto isSolid = [&tmc](uint32_t v) {
            return std::find(tmc.SolidTileValues.begin(), tmc.SolidTileValues.end(), v) != tmc.SolidTileValues.end();
        };

        for (uint32_t row = 0; row < tmc.MapHeight; ++row) {
            uint32_t x = 0;
            while (x < tmc.MapWidth) {
                uint32_t idx = row * tmc.MapWidth + x;
                if (idx >= tmc.Tiles.size() || !isSolid(tmc.Tiles[idx])) { ++x; continue; }

                // Mescla o run contíguo de tiles sólidos nesta linha.
                uint32_t runStart = x;
                while (x < tmc.MapWidth) {
                    uint32_t i = row * tmc.MapWidth + x;
                    if (i >= tmc.Tiles.size() || !isSolid(tmc.Tiles[i])) break;
                    ++x;
                }
                uint32_t runEnd = x; // exclusivo

                float cx = mapPos.x + (float)(runStart + runEnd) * 0.5f * tmc.TileSize.x;
                float cy = mapPos.y + ((float)row + 0.5f) * tmc.TileSize.y;
                float hw = (float)(runEnd - runStart) * 0.5f * tmc.TileSize.x;
                float hh = 0.5f * tmc.TileSize.y;

                b2BodyDef bodyDef;
                bodyDef.type = b2_staticBody;
                bodyDef.position.Set(cx, cy);
                b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
                b2PolygonShape shape;
                shape.SetAsBox(hw, hh);
                body->CreateFixture(&shape, 0.0f);
            }
        }
    }
}

void Scene::OnPhysics2DStop() {
    delete m_PhysicsWorld2D;
    m_PhysicsWorld2D = nullptr;
}

void Scene::UpdatePhysics2D(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::UpdatePhysics2D");
    if (!m_PhysicsWorld2D) return;

    constexpr int32_t velocityIterations = 8, positionIterations = 3;
    m_PhysicsWorld2D->Step(ts, velocityIterations, positionIterations);

    auto view = m_Registry.view<Rigidbody2DComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        auto* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (!body) continue;

        const auto& pos = body->GetPosition();
        transform.Translation.x = pos.x;
        transform.Translation.y = pos.y;
        transform.Rotation.z = body->GetAngle();
    }
}

void Scene::OnPhysics3DStart() {
    m_CollisionConfig = new btDefaultCollisionConfiguration();
    m_Dispatcher = new btCollisionDispatcher(m_CollisionConfig);
    m_Broadphase = new btDbvtBroadphase();
    m_Solver = new btSequentialImpulseConstraintSolver();
    m_PhysicsWorld3D = new btDiscreteDynamicsWorld(m_Dispatcher, m_Broadphase, m_Solver, m_CollisionConfig);
    m_PhysicsWorld3D->setGravity(btVector3(0, -9.81f, 0));

    auto view = m_Registry.view<Rigidbody3DComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();

        btCollisionShape* shape = nullptr;
        if (entity.HasComponent<BoxCollider3DComponent>()) {
            auto& bc3d = entity.GetComponent<BoxCollider3DComponent>();
            shape = new btBoxShape(btVector3(bc3d.HalfExtents.x * transform.Scale.x,
                                              bc3d.HalfExtents.y * transform.Scale.y,
                                              bc3d.HalfExtents.z * transform.Scale.z));
        } else if (entity.HasComponent<SphereCollider3DComponent>()) {
            auto& sc3d = entity.GetComponent<SphereCollider3DComponent>();
            shape = new btSphereShape(sc3d.Radius * transform.Scale.x);
        } else {
            // Sem collider explícito: usa uma caixa unitária como padrão
            // seguro, em vez de silenciosamente não simular a entidade.
            shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        }
        m_PhysicsShapes3D.push_back(shape);

        bool isDynamic = rb3d.Type == Rigidbody3DComponent::BodyType::Dynamic;
        btScalar mass = isDynamic ? rb3d.Mass : 0.0f;
        btVector3 localInertia(0, 0, 0);
        if (isDynamic) shape->calculateLocalInertia(mass, localInertia);

        btQuaternion rotation;
        rotation.setEulerZYX(transform.Rotation.z, transform.Rotation.y, transform.Rotation.x);
        btTransform startTransform;
        startTransform.setIdentity();
        startTransform.setOrigin(btVector3(transform.Translation.x, transform.Translation.y, transform.Translation.z));
        startTransform.setRotation(rotation);

        auto* motionState = new btDefaultMotionState(startTransform);
        m_PhysicsMotionStates3D.push_back(motionState);

        btRigidBody::btRigidBodyConstructionInfo rbInfo(mass, motionState, shape, localInertia);
        auto* body = new btRigidBody(rbInfo);
        if (rb3d.Type == Rigidbody3DComponent::BodyType::Kinematic) {
            body->setCollisionFlags(body->getCollisionFlags() | btCollisionObject::CF_KINEMATIC_OBJECT);
            body->setActivationState(DISABLE_DEACTIVATION);
        }

        m_PhysicsWorld3D->addRigidBody(body);
        rb3d.RuntimeBody = body;
    }
}

void Scene::OnPhysics3DStop() {
    if (m_PhysicsWorld3D) {
        auto view = m_Registry.view<Rigidbody3DComponent>();
        for (auto e : view) {
            Entity entity{ e, this };
            auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
            if (auto* body = static_cast<btRigidBody*>(rb3d.RuntimeBody)) {
                m_PhysicsWorld3D->removeRigidBody(body);
                delete body;
                rb3d.RuntimeBody = nullptr;
            }
        }
    }

    for (auto* motionState : m_PhysicsMotionStates3D) delete motionState;
    for (auto* shape : m_PhysicsShapes3D) delete shape;
    m_PhysicsMotionStates3D.clear();
    m_PhysicsShapes3D.clear();

    delete m_PhysicsWorld3D; m_PhysicsWorld3D = nullptr;
    delete m_Solver; m_Solver = nullptr;
    delete m_Broadphase; m_Broadphase = nullptr;
    delete m_Dispatcher; m_Dispatcher = nullptr;
    delete m_CollisionConfig; m_CollisionConfig = nullptr;
}

void Scene::UpdatePhysics3D(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::UpdatePhysics3D");
    if (!m_PhysicsWorld3D) return;

    m_PhysicsWorld3D->stepSimulation(ts, 10);

    auto view = m_Registry.view<Rigidbody3DComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
        auto* body = static_cast<btRigidBody*>(rb3d.RuntimeBody);
        if (!body || rb3d.Type == Rigidbody3DComponent::BodyType::Static) continue;

        btTransform bt;
        body->getMotionState()->getWorldTransform(bt);
        const auto& origin = bt.getOrigin();
        transform.Translation = { origin.x(), origin.y(), origin.z() };

        btScalar yaw, pitch, roll;
        bt.getRotation().getEulerZYX(yaw, pitch, roll);
        transform.Rotation = { roll, pitch, yaw };
    }
}

void Scene::OnRuntimeStart() {
    KZ_TRACE_SCOPE("Scene::OnRuntimeStart");
    OnPhysics2DStart();
    OnPhysics3DStart();

    m_Registry.view<NativeScriptComponent>().each([](auto entityHandle, auto& nsc) {
        (void)entityHandle;
        if (!nsc.Instance && nsc.InstantiateScript) {
            nsc.Instance = nsc.InstantiateScript();
            // InstantiateScript() pode retornar nullptr agora que scripts
            // vinculados por nome (BindByName) dependem de um GameModule
            // carregado — sem essa checagem, uma cena referenciando um
            // script ainda não carregado derrubava o Play inteiro.
            if (nsc.Instance) nsc.Instance->OnCreate();
        }
    });
}

void Scene::OnRuntimeStop() {
    KZ_TRACE_SCOPE("Scene::OnRuntimeStop");
    OnPhysics2DStop();
    OnPhysics3DStop();

    // Sem isso, a instância de script sobrevivia entre sessões de Play —
    // na segunda vez que apertava Play, o `if (!nsc.Instance)` em
    // OnRuntimeStart nunca era verdadeiro de novo, então OnCreate() não
    // disparava e o objeto antigo continuava rodando. Agora ficou também
    // uma questão de segurança: se o GameModule for descarregado (recarga
    // de script), uma instância viva apontaria pra memória desmapeada.
    m_Registry.view<NativeScriptComponent>().each([](auto entityHandle, auto& nsc) {
        (void)entityHandle;
        if (nsc.Instance && nsc.DestroyScript) {
            nsc.Instance->OnDestroy();
            nsc.DestroyScript(&nsc);
        }
    });
}

void Scene::OnUpdateRuntime(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::OnUpdateRuntime");
    // 1. Scripts nativos (KZScript C++ v1)
    m_Registry.view<NativeScriptComponent>().each([=](auto entityHandle, auto& nsc) {
        (void)entityHandle;
        if (nsc.Instance) nsc.Instance->OnUpdate(ts);
    });

    // 2. Física
    UpdatePhysics2D(ts);
    UpdatePhysics3D(ts);

    // 2b. Partículas — só simula em Play, mesma convenção da física (não roda em modo edição)
    UpdateParticleSystems(ts);
    UpdateSpriteAnimations(ts); // animação roda em Play e em edição (preview)
    UpdateAudio(ts);

    // 3. Render — jogo rodando de verdade, cada passe usa a câmera
    // primária da própria cena (não a do editor).
    RenderScene2D(nullptr);
    RenderScene3D(nullptr);
}

void Scene::OnUpdateEditor3D(Timestep ts, PerspectiveCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::OnUpdateEditor3D");
    UpdateSpriteAnimations(ts); // preview de animação no viewport, mesmo em edição
    // Modo 3D do viewport: malhas + grid via câmera livre do editor. O
    // passe 2D continua rodando com a câmera primária da PRÓPRIA cena, se
    // houver uma — é o que permite um HUD/overlay 2D aparecer sobre uma
    // cena 3D sem precisar de nenhum tratamento especial.
    RenderScene2D(nullptr);
    RenderScene3D(&editorCamera);
}

void Scene::OnUpdateEditor2D(Timestep ts, OrthographicCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::OnUpdateEditor2D");
    UpdateSpriteAnimations(ts); // preview de animação no viewport, mesmo em edição
    // Modo 2D do viewport: navegação livre (pan/zoom) via a própria
    // câmera do editor, ignorando qualquer CameraComponent da cena — não
    // precisa de uma entidade de câmera só pra poder editar sprites. Sem
    // passe 3D aqui de propósito: grid e malhas 3D só teriam papel de
    // ruído visual enquanto o foco é edição 2D.
    RenderScene2D(&editorCamera);
}

void Scene::OnViewportResize(uint32_t width, uint32_t height) {
    m_ViewportWidth = width; m_ViewportHeight = height;
}

// Extrai posição/rotação/escala de um transform mundial já composto. Usado
// para câmeras e para qualquer entidade que possa estar dentro de uma
// hierarquia — GetTransform() local não basta assim que há um pai no meio.
static void DecomposeTransform(const glm::mat4& m, glm::vec3& outPos, glm::vec3& outEuler) {
    outPos = glm::vec3(m[3]);
    glm::vec3 col0 = glm::vec3(m[0]), col1 = glm::vec3(m[1]), col2 = glm::vec3(m[2]);
    glm::mat3 rotScale(glm::normalize(col0), glm::normalize(col1), glm::normalize(col2));
    outEuler = glm::eulerAngles(glm::quat_cast(rotScale));
}

// Desenha todos os renderizadores 2D da cena dentro de um BeginScene já
// aberto: sprites, círculos, animações de sprite (recorte de UV na folha),
// tilemap (grade de quads com recorte de UV) e texto (fonte embutida).
void Scene::RenderScene2D(OrthographicCamera* overrideCamera) {
    KZ_TRACE_SCOPE("Scene::RenderScene2D");
    if (overrideCamera) {
        Renderer2D::BeginScene(*overrideCamera);
        Renderer2D::DrawGrid();
        Render2DEntities();
        Renderer2D::EndScene();
        return;
    }

    auto camView = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto e : camView) {
        auto& camera = camView.get<CameraComponent>(e);
        if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Orthographic2D) continue;

        glm::vec3 pos, euler;
        DecomposeTransform(GetWorldTransform(Entity{ e, this }), pos, euler);

        float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;
        OrthographicCamera cam(-camera.OrthoSize * aspect, camera.OrthoSize * aspect, -camera.OrthoSize, camera.OrthoSize);
        cam.SetPosition(pos);
        cam.SetRotation(glm::degrees(euler.z));

        Renderer2D::BeginScene(cam);
        Render2DEntities();
        Renderer2D::EndScene();
        break;
    }
}

void Scene::Render2DEntities() {
    // Sprites (cor sólida ou textura) + círculos + texto + tilemap.
    auto sprites = m_Registry.view<TransformComponent, SpriteRendererComponent>();
    for (auto se : sprites) {
        auto& sprite = sprites.get<SpriteRendererComponent>(se);
        glm::mat4 worldTransform = GetWorldTransform(Entity{ se, this });
        if (sprite.Texture)
            Renderer2D::DrawTransformedQuad(worldTransform, sprite.Texture, sprite.TilingFactor, sprite.Color);
        else
            Renderer2D::DrawTransformedQuad(worldTransform, sprite.Color);
    }

    auto circles = m_Registry.view<TransformComponent, CircleRendererComponent>();
    for (auto ce : circles) {
        auto& circle = circles.get<CircleRendererComponent>(ce);
        Renderer2D::DrawCircle(GetWorldTransform(Entity{ ce, this }), circle.Color, circle.Thickness, circle.Fade);
    }

    // Animações de sprite: desenham a folha com recorte de UV do frame atual.
    auto anims = m_Registry.view<TransformComponent, SpriteAnimationComponent>();
    for (auto ae : anims) {
        auto& anim = anims.get<SpriteAnimationComponent>(ae);
        if (!anim.SheetTexture && !anim.SheetPath.empty())
            anim.SheetTexture = Texture2D::Create(anim.SheetPath); // carregada sob demanda
        if (!anim.SheetTexture || anim.FramesPerRow == 0) continue;

        uint32_t cols = anim.FramesPerRow;
        uint32_t row = anim.CurrentFrame / cols;
        uint32_t col = anim.CurrentFrame % cols;
        uint32_t rows = (anim.TotalFrames + cols - 1) / cols;
        glm::vec2 uvMin{ (float)col / cols, 1.0f - (float)(row + 1) / rows };
        glm::vec2 uvMax{ (float)(col + 1) / cols, 1.0f - (float)row / rows };
        Renderer2D::DrawTransformedQuadUV(GetWorldTransform(Entity{ ae, this }), anim.SheetTexture, uvMin, uvMax, { 1.0f, 1.0f, 1.0f, 1.0f });
    }

    // Tilemap: um quad por tile não-vazio, recortado do atlas.
    auto tilemaps = m_Registry.view<TransformComponent, TilemapComponent>();
    for (auto te : tilemaps) {
        auto& tm = tilemaps.get<TilemapComponent>(te);
        if (!tm.AtlasTexture && !tm.AtlasPath.empty())
            tm.AtlasTexture = Texture2D::Create(tm.AtlasPath);
        if (!tm.AtlasTexture || tm.AtlasColumns == 0 || tm.AtlasRows == 0 || tm.MapWidth == 0) continue;

        glm::mat4 mapTransform = GetWorldTransform(Entity{ te, this });
        for (uint32_t y = 0; y < tm.MapHeight; ++y) {
            for (uint32_t x = 0; x < tm.MapWidth; ++x) {
                uint32_t idx = y * tm.MapWidth + x;
                if (idx >= tm.Tiles.size() || tm.Tiles[idx] == 0) continue;
                uint32_t tile = tm.Tiles[idx] - 1;
                uint32_t tcol = tile % tm.AtlasColumns;
                uint32_t trow = tile / tm.AtlasColumns;
                glm::vec2 uvMin{ (float)tcol / tm.AtlasColumns, 1.0f - (float)(trow + 1) / tm.AtlasRows };
                glm::vec2 uvMax{ (float)(tcol + 1) / tm.AtlasColumns, 1.0f - (float)trow / tm.AtlasRows };
                glm::mat4 tileTransform = mapTransform *
                    glm::translate(glm::mat4(1.0f), { x * tm.TileSize.x, y * tm.TileSize.y, 0.0f }) *
                    glm::scale(glm::mat4(1.0f), { tm.TileSize.x, tm.TileSize.y, 1.0f });
                Renderer2D::DrawTransformedQuadUV(tileTransform, tm.AtlasTexture, uvMin, uvMax, { 1.0f, 1.0f, 1.0f, 1.0f });
            }
        }
    }

    // Texto de jogo (HUD etc) — posicionado pelo transform da entidade,
    // com alinhamento configurável (esquerda/centro/direita).
    auto texts = m_Registry.view<TransformComponent, TextComponent>();
    for (auto te : texts) {
        auto& tc = texts.get<TextComponent>(te);
        glm::mat4 world = GetWorldTransform(Entity{ te, this });
        glm::vec3 pos = glm::vec3(world[3]);
        TextRenderer::DrawString(tc.Text, pos, tc.FontSize, tc.Color, tc.Alignment);
    }
}

void Scene::UpdateSpriteAnimations(Timestep ts) {
    auto view = m_Registry.view<SpriteAnimationComponent>();
    for (auto e : view) {
        auto& anim = view.get<SpriteAnimationComponent>(e);
        if (!anim.Playing || anim.TotalFrames <= 1) continue;
        anim.FrameTimer += (float)ts;
        float frameDuration = 1.0f / glm::max(anim.FPS, 0.001f);
        while (anim.FrameTimer >= frameDuration) {
            anim.FrameTimer -= frameDuration;
            ++anim.CurrentFrame;
            if (anim.CurrentFrame >= anim.TotalFrames) {
                if (anim.Loop) anim.CurrentFrame = 0;
                else { anim.CurrentFrame = anim.TotalFrames - 1; anim.Playing = false; }
            }
        }
    }
}

void Scene::SubmitLights() {
    auto lights = m_Registry.view<TransformComponent, LightComponent>();
    if (lights.begin() == lights.end()) {
        Renderer3D::SubmitLight(Light{}); // sem nenhuma LightComponent na cena, mantém o sol default de antes
        return;
    }
    for (auto e : lights) {
        auto& lc = lights.get<LightComponent>(e);
        glm::mat4 world = GetWorldTransform(Entity{ e, this });
        Light l;
        l.Type = lc.Type;
        l.Position = glm::vec3(world[3]);
        l.Direction = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, -1.0f, 0.0f));
        l.Color = lc.Color;
        l.Intensity = lc.Intensity;
        l.Range = lc.Range;
        l.InnerConeDeg = lc.InnerConeDeg;
        l.OuterConeDeg = lc.OuterConeDeg;
        Renderer3D::SubmitLight(l);
    }
}

void Scene::UpdateParticleSystems(Timestep ts) {
    auto view = m_Registry.view<TransformComponent, ParticleSystemComponent>();
    for (auto e : view) {
        auto& pc = view.get<ParticleSystemComponent>(e);
        if (!pc.Playing) continue;
        glm::vec3 emitterPos = glm::vec3(GetWorldTransform(Entity{ e, this })[3]);

        pc.EmissionAccumulator += (float)ts * pc.EmissionRate;
        int toSpawn = (int)pc.EmissionAccumulator;
        pc.EmissionAccumulator -= (float)toSpawn;
        for (int i = 0; i < toSpawn && pc.ActiveParticles.size() < pc.MaxParticles; ++i) {
            Particle p;
            p.Position = emitterPos;
            p.Velocity = glm::linearRand(pc.VelocityMin, pc.VelocityMax);
            p.Lifetime = glm::linearRand(pc.LifetimeMin, pc.LifetimeMax);
            pc.ActiveParticles.push_back(p);
        }

        for (size_t i = 0; i < pc.ActiveParticles.size();) {
            auto& p = pc.ActiveParticles[i];
            p.Age += (float)ts;
            if (p.Age >= p.Lifetime) {
                p = pc.ActiveParticles.back(); // swap-and-pop — ordem não importa aqui
                pc.ActiveParticles.pop_back();
                continue;
            }
            p.Velocity += pc.Gravity * (float)ts;
            p.Position += p.Velocity * (float)ts;
            ++i;
        }
    }
}

void Scene::SubmitParticleSystems() {
    auto view = m_Registry.view<ParticleSystemComponent>();
    for (auto e : view) {
        auto& pc = view.get<ParticleSystemComponent>(e);
        if (pc.ActiveParticles.empty()) continue;
        std::vector<ParticleInstance> instances;
        instances.reserve(pc.ActiveParticles.size());
        for (auto& p : pc.ActiveParticles) {
            float t = glm::clamp(p.Age / p.Lifetime, 0.0f, 1.0f);
            instances.push_back({ p.Position, glm::mix(pc.StartSize, pc.EndSize, t), glm::mix(pc.StartColor, pc.EndColor, t) });
        }
        Renderer3D::SubmitParticles(instances, pc.Additive);
    }
}

void Scene::UpdateAudio(Timestep) {
    // Listener = câmera 3D principal ativa, mesma que RenderScene3D usa pra desenhar.
    auto camView = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto e : camView) {
        auto& camera = camView.get<CameraComponent>(e);
        if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Perspective3D) continue;
        glm::mat4 world = GetWorldTransform(Entity{ e, this });
        glm::vec3 pos = glm::vec3(world[3]);
        glm::vec3 forward = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 up = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 1.0f, 0.0f));
        AudioEngine::SetListenerPosition(pos, forward, up);
        break;
    }

    auto view = m_Registry.view<TransformComponent, AudioSourceComponent>();
    for (auto e : view) {
        auto& ac = view.get<AudioSourceComponent>(e);
        if (ac.ClipPath.empty()) continue;

        if (ac.Handle == kInvalidSound) {
            ac.Handle = AudioEngine::LoadSound(ac.ClipPath, ac.ClipPath, false);
            if (ac.Handle == kInvalidSound) continue; // falhou (arquivo não encontrado etc) — tenta de novo no próximo frame
            AudioEngine::SetSoundAttenuation(ac.Handle, ac.MinDistance, ac.MaxDistance);
        }

        if (ac.Spatial)
            AudioEngine::SetSoundPosition3D(ac.Handle, glm::vec3(GetWorldTransform(Entity{ e, this })[3]));

        if (ac.PlayOnStart && !ac.HasStarted) {
            AudioEngine::Play(ac.Handle, ac.Loop, ac.Volume);
            ac.HasStarted = true;
        }
    }
}

void Scene::RenderScene3D(PerspectiveCamera* overrideCamera) {
    KZ_TRACE_SCOPE("Scene::RenderScene3D");
    if (overrideCamera) {
        Renderer3D::BeginScene(*overrideCamera);
        SubmitLights();
        SubmitParticleSystems();
        Renderer3D::DrawGrid();
        auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();
        for (auto me : meshes) {
            auto& mr = meshes.get<MeshRendererComponent>(me);
            if (mr.MeshAsset) Renderer3D::Submit(mr.MeshAsset, mr.MeshMaterial, GetWorldTransform(Entity{ me, this }));
        }
        Renderer3D::EndScene();
        return;
    }

    auto camView = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto e : camView) {
        auto& camera = camView.get<CameraComponent>(e);
        if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Perspective3D) continue;

        glm::vec3 pos, euler;
        DecomposeTransform(GetWorldTransform(Entity{ e, this }), pos, euler);

        float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;
        PerspectiveCamera cam(camera.PerspectiveFOV, aspect, camera.NearClip, camera.FarClip);
        cam.SetPosition(pos);
        cam.SetRotation(glm::degrees(euler.y), glm::degrees(euler.x));

        Renderer3D::BeginScene(cam);
        SubmitLights();
        SubmitParticleSystems();
        auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();
        for (auto me : meshes) {
            auto& mr = meshes.get<MeshRendererComponent>(me);
            if (mr.MeshAsset) Renderer3D::Submit(mr.MeshAsset, mr.MeshMaterial, GetWorldTransform(Entity{ me, this }));
        }
        Renderer3D::EndScene();
        break;
    }
}

} // namespace kizuri
