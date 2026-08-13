#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/net/NetworkFacade.hpp"
#include "kizuri/renderer/LightmapBaker.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/CSharpBridge.h"
#include "kizuri/scene/SceneSerializer.hpp"
#include "kizuri/scene/Prefab.hpp"
#include "kizuri/project/Project.hpp"
#include "kizuri/renderer/Renderer2D.hpp"
// Detalhe de serialização por entidade (DuplicateEntity) — fica em src/, não
// na API pública; o Prefab.cpp já o usa do mesmo jeito.
#include "../scene/ComponentSerialization.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/core/Log.hpp"
#include "kizuri/audio/AudioEngine.hpp"

#include <box2d/box2d.h>
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btBvhTriangleMeshShape.h>
#include <BulletCollision/CollisionShapes/btTriangleMesh.h>
#include <BulletCollision/CollisionDispatch/btGhostObject.h>
#include <BulletDynamics/Character/btKinematicCharacterController.h>
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstring>

namespace kizuri {

namespace {

// Frustum culling (Gribb-Hartmann): extrai os 6 planos de uma VP (OpenGL,
// clip z em [-1,1]) e testa um AABB usando o "positive vertex" por plano.
bool AABBInFrustum(const glm::mat4& vp, const glm::vec3& min, const glm::vec3& max) {
    auto row = [&](int r) { return glm::vec4(vp[0][r], vp[1][r], vp[2][r], vp[3][r]); };
    glm::vec4 row1 = row(0), row2 = row(1), row3 = row(2), row4 = row(3);
    glm::vec4 planes[6] = {
        row4 + row1, row4 - row1, // esquerda, direita
        row4 + row2, row4 - row2, // baixo, topo
        row4 + row3, row4 - row3  // perto, longe
    };
    for (int i = 0; i < 6; ++i) {
        const glm::vec4& p = planes[i];
        glm::vec3 v(min.x, min.y, min.z);
        if (p.x > 0.0f) v.x = max.x;
        if (p.y > 0.0f) v.y = max.y;
        if (p.z > 0.0f) v.z = max.z;
        if (glm::dot(p, glm::vec4(v, 1.0f)) < 0.0f) return false;
    }
    return true;
}

struct ContactListener2D : public b2ContactListener {
    std::vector<std::pair<entt::entity, entt::entity>>* BeginQueue = nullptr;
    std::vector<std::pair<entt::entity, entt::entity>>* EndQueue = nullptr;

    static entt::entity EntityFromBody(b2Body* body) {
        if (!body) return entt::null;
        uintptr_t ptr = body->GetUserData().pointer;
        if (ptr == 0) return entt::null;
        return static_cast<entt::entity>(static_cast<uint32_t>(ptr));
    }

    void BeginContact(b2Contact* contact) override {
        auto a = EntityFromBody(contact->GetFixtureA()->GetBody());
        auto b = EntityFromBody(contact->GetFixtureB()->GetBody());
        if (a == entt::null && b == entt::null) return;
        BeginQueue->emplace_back(a, b);
    }

    void EndContact(b2Contact* contact) override {
        auto a = EntityFromBody(contact->GetFixtureA()->GetBody());
        auto b = EntityFromBody(contact->GetFixtureB()->GetBody());
        if (a == entt::null && b == entt::null) return;
        EndQueue->emplace_back(a, b);
    }
};

static uint64_t PackEntityPair(entt::entity a, entt::entity b) {
    uint32_t x = static_cast<uint32_t>(a);
    uint32_t y = static_cast<uint32_t>(b);
    if (x > y) std::swap(x, y);
    return (uint64_t(x) << 32) | uint64_t(y);
}

} // namespace

static b2BodyType ToBox2DBody(Rigidbody2DComponent::BodyType type) {
    switch (type) {
        case Rigidbody2DComponent::BodyType::Static:    return b2_staticBody;
        case Rigidbody2DComponent::BodyType::Dynamic:   return b2_dynamicBody;
        case Rigidbody2DComponent::BodyType::Kinematic: return b2_kinematicBody;
    }
    return b2_staticBody;
}

Scene::Scene(std::string name) : m_Name(std::move(name)) {}
Scene::~Scene() {
    if (m_Running) OnRuntimeStop();
}

Ref<Scene> Scene::Copy(const Ref<Scene>& source) {
    KZ_TRACE_SCOPE("Scene::Copy");
    Ref<Scene> copy = CreateRef<Scene>(source->m_Name);
    copy->m_ViewportWidth = source->m_ViewportWidth;
    copy->m_ViewportHeight = source->m_ViewportHeight;

    SceneSerializer srcSerializer(source);
    SceneSerializer dstSerializer(copy);
    dstSerializer.DeserializeFromJson(srcSerializer.SerializeToJson());
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

void Scene::FlushPendingDestroys() {
    if (m_PendingDestroy.empty()) return;
    std::vector<entt::entity> pending;
    pending.swap(m_PendingDestroy);
    for (entt::entity e : pending) {
        Entity entity{ e, this };
        if (entity && m_Registry.valid(e)) DestroyEntityNow(entity);
    }
}

void Scene::DestroyEntity(Entity entity) {
    if (!entity) return;
    // Durante a iteração de scripts (OnUpdate de NativeScript) destruir a
    // entidade imediatamente invalida a view do entt -> UB/crash. Deferido:
    // o flush roda no fim do loop (FlushPendingDestroys em OnUpdateRuntimeLogic).
    if (m_InScriptUpdate) {
        m_PendingDestroy.push_back(entity.GetHandle());
        return;
    }
    DestroyEntityNow(entity);
}

void Scene::DestroyEntityNow(Entity entity) {
    if (!entity) return;

    if (entity.HasComponent<RelationshipComponent>()) {
        auto childrenCopy = entity.GetComponent<RelationshipComponent>().Children;
        for (UUID childId : childrenCopy) {
            Entity child = GetEntityByUUID(childId);
            if (child) DestroyEntity(child);
        }
    }

    SetParent(entity, {});

    // Scripts: OnDestroy antes de sumir com física/registry.
    if (entity.HasComponent<NativeScriptComponent>()) {
        auto& nsc = entity.GetComponent<NativeScriptComponent>();
        if (nsc.Instance && nsc.DestroyScript) {
            nsc.Instance->OnDestroy();
            nsc.DestroyScript(&nsc);
        }
    }

    UnregisterPhysics2DEntity(entity);
    UnregisterPhysics3DEntity(entity);

    if (entity.HasComponent<IDComponent>())
        m_EntityMap.erase(entity.GetComponent<IDComponent>().ID);

    m_Registry.destroy(entity.GetHandle());
}

Entity Scene::Instantiate(const std::string& prefabPath, const glm::vec3& position) {
    Entity root = Prefab::Instantiate(*this, prefabPath, position);
    if (!root) return {};

    // Prefab pode trazer uma subárvore — registra física/scripts em todo mundo.
    std::vector<Entity> stack{ root };
    while (!stack.empty()) {
        Entity e = stack.back();
        stack.pop_back();
        if (m_Running) {
            RegisterPhysics2DEntity(e);
            RegisterPhysics3DEntity(e);
            StartScriptIfNeeded(e);
        }
        for (Entity child : e.GetChildren())
            stack.push_back(child);
    }
    return root;
}

void Scene::RequestLoad(const std::string& scenePath) {
    if (!scenePath.empty()) m_PendingScenePath = scenePath;
}

bool Scene::PollPendingLoad(std::string& outPath) {
    if (m_PendingScenePath.empty()) return false;
    outPath = std::move(m_PendingScenePath);
    m_PendingScenePath.clear();
    return true;
}

namespace {

// Callback do b2World::RayCast — guarda o hit mais próximo (fração menor).
class KizuriRayCastCallback2D : public b2RayCastCallback {
public:
    float BestFraction = 1.0f;
    b2Fixture* HitFixture = nullptr;
    b2Vec2 HitPoint;

    float ReportFixture(b2Fixture* fixture, const b2Vec2& point,
                        const b2Vec2& /*normal*/, float fraction) override {
        if (fraction < BestFraction) {
            BestFraction = fraction;
            HitFixture = fixture;
            HitPoint = point;
        }
        return BestFraction; // segue varrendo, mas só aceita hits mais perto
    }
};

// Callback do b2World::QueryAABB pro OverlapCircle2D. Calcula o gap do
// círculo (Center+Radius) até cada fixture candidato sem depender do
// b2Distance de shapes (que não vem no umbrella do box2d) — manual e exato
// pra círculo e box, o que cobre os colliders da engine.
class OverlapCircleCallback : public b2QueryCallback {
public:
    b2Vec2 Center;
    float Radius = 0.0f;
    b2Fixture* Best = nullptr;
    float BestGap = std::numeric_limits<float>::max();

    static float PointToSegmentDist(const b2Vec2& p, const b2Vec2& a, const b2Vec2& b) {
        b2Vec2 ab = b - a;
        float lenSq = b2Dot(ab, ab);
        float t = lenSq > 0.0f ? b2Clamp(b2Dot(p - a, ab) / lenSq, 0.0f, 1.0f) : 0.0f;
        return b2Distance(p, a + t * ab);
    }

    // Distância da superfície do fixture ao ponto p (0 se p está dentro).
    static float GapToFixture(const b2Fixture* fixture, const b2Vec2& p) {
        const b2Shape* shape = fixture->GetShape();
        const b2Transform& xf = fixture->GetBody()->GetTransform();
        if (shape->GetType() == b2Shape::e_circle) {
            const auto* c = static_cast<const b2CircleShape*>(shape);
            return b2Distance(p, b2Mul(xf, c->m_p)) - c->m_radius;
        }
        if (shape->GetType() == b2Shape::e_polygon) {
            const auto* poly = static_cast<const b2PolygonShape*>(shape);
            if (poly->TestPoint(xf, p)) return 0.0f; // ponto dentro do box
            float best = std::numeric_limits<float>::max();
            for (int32 i = 0; i < poly->m_count; ++i) {
                b2Vec2 a = b2Mul(xf, poly->m_vertices[i]);
                b2Vec2 b = b2Mul(xf, poly->m_vertices[(i + 1) % poly->m_count]);
                float d = PointToSegmentDist(p, a, b);
                if (d < best) best = d;
            }
            return best;
        }
        return std::numeric_limits<float>::max();
    }

    bool ReportFixture(b2Fixture* fixture) override {
        float gap = GapToFixture(fixture, Center) - Radius;
        if (gap < BestGap) { BestGap = gap; Best = fixture; }
        return true;
    }
};

} // namespace

bool Scene::Raycast2D(const glm::vec2& from, const glm::vec2& to,
                      Entity& outEntity, glm::vec2& outPoint, float& outFraction) {
    if (m_PhysicsWorld2D == nullptr) return false;
    KizuriRayCastCallback2D cb;
    m_PhysicsWorld2D->RayCast(&cb, { from.x, from.y }, { to.x, to.y });
    if (cb.HitFixture == nullptr) return false;

    // O userData de cada b2Body guarda o handle entt::entity (ver
    // RegisterPhysics2DEntity) — reconstrói a Entity sem sair do mundo.
    uintptr_t ptr = cb.HitFixture->GetBody()->GetUserData().pointer;
    outEntity = Entity{ static_cast<entt::entity>(ptr), this };
    outPoint = { cb.HitPoint.x, cb.HitPoint.y };
    outFraction = cb.BestFraction;
    return true;
}

bool Scene::OverlapCircle2D(const glm::vec2& center, float radius, Entity& outEntity) {
    if (m_PhysicsWorld2D == nullptr) return false;

    // Varre só os fixtures dentro do AABB do círculo (candidatos) e acha o
    // mais próximo pelo gap até a superfície (<= 0 = o círculo toca o collider).
    b2AABB aabb;
    aabb.lowerBound = { center.x - radius, center.y - radius };
    aabb.upperBound = { center.x + radius, center.y + radius };

    OverlapCircleCallback cb;
    cb.Center = { center.x, center.y };
    cb.Radius = radius;
    m_PhysicsWorld2D->QueryAABB(&cb, aabb);

    if (cb.Best == nullptr || cb.BestGap > 0.0f) return false;
    uintptr_t ptr = cb.Best->GetBody()->GetUserData().pointer;
    outEntity = Entity{ static_cast<entt::entity>(ptr), this };
    return true;
}

bool Scene::Raycast3D(const glm::vec3& from, const glm::vec3& to,
                      Entity& outEntity, glm::vec3& outPoint, float& outFraction) {
    if (m_PhysicsWorld3D == nullptr) return false;
    btVector3 bfrom(from.x, from.y, from.z);
    btVector3 bto(to.x, to.y, to.z);
    btCollisionWorld::ClosestRayResultCallback cb(bfrom, bto);
    m_PhysicsWorld3D->rayTest(bfrom, bto, cb);
    if (!cb.hasHit()) return false;

    outPoint = { cb.m_hitPointWorld.x(), cb.m_hitPointWorld.y(), cb.m_hitPointWorld.z() };
    outFraction = cb.m_closestHitFraction;
    void* up = cb.m_collisionObject->getUserPointer();
    if (!up) return false;
    outEntity = Entity{ static_cast<entt::entity>(reinterpret_cast<uintptr_t>(up)), this };
    return true;
}

// Coleta a primeira entidade que toca a esfera fantasma (Bullet).
struct OverlapSphereCallback : public btCollisionWorld::ContactResultCallback {
    entt::entity Best = entt::null;

    btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper*,
                             int, int, const btCollisionObjectWrapper* otherWrap, int, int) override {
        void* up = otherWrap->getCollisionObject()->getUserPointer();
        if (up) Best = static_cast<entt::entity>(reinterpret_cast<uintptr_t>(up));
        return 0.0f; // para na primeira
    }
};

bool Scene::OverlapSphere3D(const glm::vec3& center, float radius, Entity& outEntity) {
    if (m_PhysicsWorld3D == nullptr) return false;

    // Esfera "fantasma" (btCollisionObject com a forma + sem resposta de
    // contato) adicionada no mundo só pra fazer o contactTest e achar quem
    // toca. btCollisionObject já vem via btBulletDynamicsCommon.h — nada de
    // include extra de GhostObject (layout do Bullet varia entre versões).
    btSphereShape sphere(radius);
    btCollisionObject obj;
    obj.setCollisionShape(&sphere);
    btTransform tr;
    tr.setIdentity();
    tr.setOrigin(btVector3(center.x, center.y, center.z));
    obj.setWorldTransform(tr);
    obj.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    m_PhysicsWorld3D->addCollisionObject(&obj);

    OverlapSphereCallback cb;
    m_PhysicsWorld3D->contactTest(&obj, cb);
    m_PhysicsWorld3D->removeCollisionObject(&obj);

    if (cb.Best == entt::null) return false;
    outEntity = Entity{ cb.Best, this };
    return true;
}

bool Scene::OverlapBox3D(const glm::vec3& center, const glm::vec3& halfExtents, Entity& outEntity) {
    if (m_PhysicsWorld3D == nullptr) return false;

    btBoxShape box(btVector3(halfExtents.x, halfExtents.y, halfExtents.z));
    btCollisionObject obj;
    obj.setCollisionShape(&box);
    btTransform tr;
    tr.setIdentity();
    tr.setOrigin(btVector3(center.x, center.y, center.z));
    obj.setWorldTransform(tr);
    obj.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    m_PhysicsWorld3D->addCollisionObject(&obj);

    OverlapSphereCallback cb;
    m_PhysicsWorld3D->contactTest(&obj, cb);
    m_PhysicsWorld3D->removeCollisionObject(&obj);

    if (cb.Best == entt::null) return false;
    outEntity = Entity{ cb.Best, this };
    return true;
}

// Coleta TODAS as entidades que tocam a forma fantasma (sensor de área).
struct OverlapCollectCallback : public btCollisionWorld::ContactResultCallback {
    std::vector<entt::entity> Hits;

    btScalar addSingleResult(btManifoldPoint&, const btCollisionObjectWrapper*,
                             int, int, const btCollisionObjectWrapper* otherWrap, int, int) override {
        void* up = otherWrap->getCollisionObject()->getUserPointer();
        if (up) Hits.push_back(static_cast<entt::entity>(reinterpret_cast<uintptr_t>(up)));
        return 0.0f;
    }
};

bool Scene::OverlapSphereAll3D(const glm::vec3& center, float radius, std::vector<Entity>& outEntities) {
    outEntities.clear();
    if (m_PhysicsWorld3D == nullptr) return false;

    btSphereShape sphere(radius);
    btCollisionObject obj;
    obj.setCollisionShape(&sphere);
    btTransform tr;
    tr.setIdentity();
    tr.setOrigin(btVector3(center.x, center.y, center.z));
    obj.setWorldTransform(tr);
    obj.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    m_PhysicsWorld3D->addCollisionObject(&obj);

    OverlapCollectCallback cb;
    m_PhysicsWorld3D->contactTest(&obj, cb);
    m_PhysicsWorld3D->removeCollisionObject(&obj);

    outEntities.reserve(cb.Hits.size());
    for (auto h : cb.Hits) outEntities.push_back(Entity{ h, this });
    return !outEntities.empty();
}

Entity Scene::DuplicateEntity(Entity source) {
    if (!source) return {};

    // Coleta source + descendentes em pré-ordem (raiz primeiro).
    std::vector<Entity> tree;
    std::vector<Entity> stack{ source };
    while (!stack.empty()) {
        Entity e = stack.back();
        stack.pop_back();
        tree.push_back(e);
        auto kids = e.GetChildren();
        for (auto it = kids.rbegin(); it != kids.rend(); ++it) stack.push_back(*it);
    }

    // Serializa cada uma e recria com UUID novo (uuid=0 força novo — o mesmo
    // truque do Prefab::Instantiate).
    std::unordered_map<UUID, UUID> remap;
    Entity newRoot;
    for (Entity e : tree) {
        UUID oldId = e.GetUUID();
        auto je = detail::SerializeEntityJson(e);
        je["ID"] = static_cast<uint64_t>(0);
        Entity ne = detail::DeserializeEntityJson(je, *this, 0);
        remap[oldId] = ne.GetUUID();
        if (e == source) newRoot = ne;
    }

    // Reparenta com os UUIDs novos (DeserializeEntityJson não aplica Parent).
    for (Entity e : tree) {
        Entity parent = e.GetParent();
        if (!parent) continue;
        auto itP = remap.find(parent.GetUUID());
        auto itE = remap.find(e.GetUUID());
        if (itP == remap.end() || itE == remap.end()) continue;
        Entity newChild = GetEntityByUUID(itE->second);
        Entity newParent = GetEntityByUUID(itP->second);
        if (newChild && newParent) newChild.SetParent(newParent);
    }

    // Leve deslocamento pra não nascer exatamente em cima do original.
    if (newRoot && newRoot.HasComponent<TransformComponent>())
        newRoot.GetComponent<TransformComponent>().Translation += glm::vec3(0.5f, 0.5f, 0.0f);
    return newRoot;
}

void Scene::SetUIMouseNDC(const glm::vec2& ndc, bool leftMouseDown) {
    m_UIMouseNDC = ndc;
    m_UIMouseDown = leftMouseDown;
    m_UIMouseValid = true;
}

void Scene::CollectUIChildren(entt::entity parent, std::vector<entt::entity>& outStack) const {
    if (!m_Registry.valid(parent)) return;
    const auto* rel = m_Registry.try_get<RelationshipComponent>(parent);
    if (!rel) return;
    for (UUID id : rel->Children) {
        auto it = m_EntityMap.find(id);
        if (it != m_EntityMap.end()) outStack.push_back(it->second);
    }
}

void Scene::UpdateUIPointer() {
    // Reinicia hover/clique de todos os botões a cada frame (estado runtime).
    m_Registry.view<UIButtonComponent>().each([](auto& b) {
        b.Hovered = false; b.Pressed = false; b.WasClicked = false;
    });
    m_UIMouseDownPrev = m_UIMouseDown;
    if (!m_UIMouseValid) return;

    float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;
    bool downEdge = m_UIMouseDown && !m_UIMouseDownPrev;

    auto canvases = m_Registry.view<TransformComponent, UICanvasComponent>();
    for (auto ce : canvases) {
        if (!IsEntityActive(Entity{ ce, this })) continue; // canvas inativo não clica
        auto& cv = canvases.get<UICanvasComponent>(ce);
        glm::vec2 uiPoint{ m_UIMouseNDC.x * cv.OrthoSize * aspect, m_UIMouseNDC.y * cv.OrthoSize };

        std::vector<entt::entity> stack;
        CollectUIChildren(ce, stack);
        while (!stack.empty()) {
            entt::entity e = stack.back();
            stack.pop_back();
            if (!IsEntityActive(Entity{ e, this })) continue; // UI inativa não clica
            CollectUIChildren(e, stack);

            auto* btn = m_Registry.try_get<UIButtonComponent>(e);
            if (!btn) continue;
            auto* rect = m_Registry.try_get<UIRectComponent>(e);
            if (!rect) continue;
            glm::vec2 half = rect->Size * 0.5f;
            bool inside = uiPoint.x >= rect->Position.x - half.x && uiPoint.x <= rect->Position.x + half.x &&
                          uiPoint.y >= rect->Position.y - half.y && uiPoint.y <= rect->Position.y + half.y;
            btn->Hovered = inside;
            btn->Pressed = inside && m_UIMouseDown;
            if (inside && downEdge) btn->WasClicked = true;
        }
    }
}

void Scene::RenderUI() {
    auto canvases = m_Registry.view<TransformComponent, UICanvasComponent>();
    if (canvases.begin() == canvases.end()) return;
    float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;

    for (auto ce : canvases) {
        if (!IsEntityActive(Entity{ ce, this })) continue; // canvas inativo não desenha
        auto& cv = canvases.get<UICanvasComponent>(ce);
        OrthographicCamera uiCam(-cv.OrthoSize * aspect, cv.OrthoSize * aspect, -cv.OrthoSize, cv.OrthoSize);
        Renderer2D::BeginScene(uiCam);

        std::vector<entt::entity> stack;
        CollectUIChildren(ce, stack);
        while (!stack.empty()) {
            entt::entity e = stack.back();
            stack.pop_back();
            if (!IsEntityActive(Entity{ e, this })) continue; // UI inativa não desenha
            CollectUIChildren(e, stack);

            auto* rect = m_Registry.try_get<UIRectComponent>(e);
            auto* btn = m_Registry.try_get<UIButtonComponent>(e);
            auto* text = m_Registry.try_get<TextComponent>(e);

            if (rect && (rect->Size.x != 0.0f || rect->Size.y != 0.0f)) {
                glm::vec4 color = rect->Color;
                if (btn) {
                    if (btn->Pressed) {
                        color = glm::vec4(glm::max(color.r * 0.55f, 0.0f), glm::max(color.g * 0.55f, 0.0f),
                                          glm::max(color.b * 0.55f, 0.0f), color.a);
                    } else if (btn->Hovered) {
                        color = glm::vec4(glm::min(color.r * 1.18f, 1.0f), glm::min(color.g * 1.18f, 1.0f),
                                          glm::min(color.b * 1.18f, 1.0f), color.a);
                    }
                }
                Renderer2D::DrawQuad(rect->Position, rect->Size, color);
            }
            if (text && rect) {
                TextRenderer::DrawString(text->Text, glm::vec3(rect->Position.x, rect->Position.y, 0.0f),
                                         text->FontSize, text->Color, TextAlignment::Center);
            }
        }
        Renderer2D::EndScene();
    }
}

void Scene::StartScriptIfNeeded(Entity entity) {
    if (!m_Running || !entity.HasComponent<NativeScriptComponent>()) return;
    auto& nsc = entity.GetComponent<NativeScriptComponent>();
    if (nsc.Instance || !nsc.InstantiateScript) return;
    nsc.Instance = nsc.InstantiateScript();
    if (nsc.Instance) {
        nsc.Instance->BindEntity(entity);
        nsc.Instance->OnCreate();
    }
}

Entity Scene::FindEntityByName(const std::string& tag) {
    auto view = m_Registry.view<TagComponent>();
    for (auto e : view) {
        if (view.get<TagComponent>(e).Tag == tag) return Entity{ e, this };
    }
    return {};
}

Entity Scene::GetEntityByUUID(UUID id) {
    auto it = m_EntityMap.find(id);
    if (it == m_EntityMap.end()) return {};
    return Entity{ it->second, this };
}

bool Scene::IsEntityActive(Entity entity) {
    for (Entity walker = entity; walker; walker = walker.GetParent()) {
        if (!walker.HasComponent<IDComponent>()) continue;
        if (!walker.GetComponent<IDComponent>().Active) return false;
    }
    return true;
}

void Scene::SetParent(Entity child, Entity newParent) {
    if (!child || !child.HasComponent<RelationshipComponent>()) return;

    if (newParent) {
        for (Entity walker = newParent; walker; walker = walker.GetParent()) {
            if (walker.GetUUID() == child.GetUUID()) {
                KZ_CORE_ERROR("SetParent: a operação criaria um ciclo na hierarquia e foi ignorada.");
                return;
            }
        }
    }

    auto& rel = child.GetComponent<RelationshipComponent>();

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

void Scene::RegisterPhysics2DEntity(Entity entity) {
    if (!m_PhysicsWorld2D || !entity.HasComponent<Rigidbody2DComponent>()) return;
    auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
    if (rb2d.RuntimeBody) return;

    auto& transform = entity.GetComponent<TransformComponent>();
    b2BodyDef bodyDef;
    bodyDef.type = ToBox2DBody(rb2d.Type);
    bodyDef.position.Set(transform.Translation.x, transform.Translation.y);
    bodyDef.angle = transform.Rotation.z;

    b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
    body->SetGravityScale(rb2d.GravityScale); // gravidade custom (platformers)
    body->GetUserData().pointer = static_cast<uintptr_t>(static_cast<uint32_t>(entity.GetHandle()));
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
    } else if (entity.HasComponent<CircleCollider2DComponent>()) {
        auto& cc2d = entity.GetComponent<CircleCollider2DComponent>();
        b2CircleShape shape;
        shape.m_p = { cc2d.Offset.x, cc2d.Offset.y };
        shape.m_radius = cc2d.Radius * std::max(transform.Scale.x, transform.Scale.y);

        b2FixtureDef fixtureDef;
        fixtureDef.shape = &shape;
        fixtureDef.density = cc2d.Density;
        fixtureDef.friction = cc2d.Friction;
        fixtureDef.restitution = cc2d.Restitution;
        body->CreateFixture(&fixtureDef);
    }

    // Filtro de colisão por CAMADA (Tags & Layers): a entidade pertence a uma
    // camada (Layer) e colide com as camadas marcadas em CollisionMask.
    if (entity.HasComponent<TagComponent>()) {
        auto& tag = entity.GetComponent<TagComponent>();
        b2Filter filter;
        filter.categoryBits = (uint16)(1u << (uint32_t)std::min(std::max(tag.Layer, 0), 15));
        filter.maskBits = (uint16)tag.CollisionMask;
        for (b2Fixture* f = body->GetFixtureList(); f; f = f->GetNext())
            f->SetFilterData(filter);
    }
}

void Scene::UnregisterPhysics2DEntity(Entity entity) {
    if (!m_PhysicsWorld2D || !entity.HasComponent<Rigidbody2DComponent>()) return;
    auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
    if (!rb2d.RuntimeBody) return;
    m_PhysicsWorld2D->DestroyBody(static_cast<b2Body*>(rb2d.RuntimeBody));
    rb2d.RuntimeBody = nullptr;
}

void Scene::BuildTilemapColliders() {
    if (!m_PhysicsWorld2D) return;

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

                uint32_t runStart = x;
                while (x < tmc.MapWidth) {
                    uint32_t i = row * tmc.MapWidth + x;
                    if (i >= tmc.Tiles.size() || !isSolid(tmc.Tiles[i])) break;
                    ++x;
                }
                uint32_t runEnd = x;

                float cx = mapPos.x + (float)(runStart + runEnd) * 0.5f * tmc.TileSize.x;
                float cy = mapPos.y + ((float)row + 0.5f) * tmc.TileSize.y;
                float hw = (float)(runEnd - runStart) * 0.5f * tmc.TileSize.x;
                float hh = 0.5f * tmc.TileSize.y;

                b2BodyDef bodyDef;
                bodyDef.type = b2_staticBody;
                bodyDef.position.Set(cx, cy);
                b2Body* body = m_PhysicsWorld2D->CreateBody(&bodyDef);
                // Tilemap: userData=0 — scripts recebem Entity inválida no "other".
                body->GetUserData().pointer = 0;
                b2PolygonShape shape;
                shape.SetAsBox(hw, hh);
                body->CreateFixture(&shape, 0.0f);
                m_TilemapBodies2D.push_back(body);
            }
        }
    }
}

// Rebuilda os colliders de tilemap que mudaram em runtime (SetTile/
// AddSolidTile marcam CollidersDirty). Remove os corpos antigos e recria.
void Scene::RebuildDirtyTilemapColliders() {
    if (!m_PhysicsWorld2D) return;
    bool dirty = false;
    m_Registry.view<TilemapComponent>().each([&](auto, TilemapComponent& tm) {
        if (tm.CollidersDirty) dirty = true;
    });
    if (!dirty) return;

    for (auto* b : m_TilemapBodies2D) m_PhysicsWorld2D->DestroyBody(b);
    m_TilemapBodies2D.clear();
    BuildTilemapColliders();
    m_Registry.view<TilemapComponent>().each([&](auto, TilemapComponent& tm) {
        tm.CollidersDirty = false;
    });
}

void Scene::OnPhysics2DStart() {
    m_PhysicsWorld2D = new b2World({ 0.0f, -9.81f });

    auto* listener = new ContactListener2D();
    listener->BeginQueue = &m_CollisionBeginQueue;
    listener->EndQueue = &m_CollisionEndQueue;
    m_PhysicsWorld2D->SetContactListener(listener);
    m_ContactListener2D = listener;

    auto view = m_Registry.view<Rigidbody2DComponent>();
    for (auto e : view)
        RegisterPhysics2DEntity(Entity{ e, this });

    BuildTilemapColliders();
}

void Scene::OnPhysics2DStop() {
    if (m_PhysicsWorld2D) {
        m_PhysicsWorld2D->SetContactListener(nullptr);
        auto view = m_Registry.view<Rigidbody2DComponent>();
        for (auto e : view) {
            auto& rb2d = m_Registry.get<Rigidbody2DComponent>(e);
            rb2d.RuntimeBody = nullptr;
        }
    }
    delete m_PhysicsWorld2D;
    m_PhysicsWorld2D = nullptr;
    delete static_cast<ContactListener2D*>(m_ContactListener2D);
    m_ContactListener2D = nullptr;
    m_CollisionBeginQueue.clear();
    m_CollisionEndQueue.clear();
}

void Scene::UpdatePhysics2D(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::UpdatePhysics2D");
    if (!m_PhysicsWorld2D) return;

    constexpr int32_t velocityIterations = 8, positionIterations = 3;
    RebuildDirtyTilemapColliders(); // tilemaps mudados em runtime (SetTile)
    m_PhysicsWorld2D->Step(ts, velocityIterations, positionIterations);

    auto view = m_Registry.view<Rigidbody2DComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        auto& transform = entity.GetComponent<TransformComponent>();
        auto& rb2d = entity.GetComponent<Rigidbody2DComponent>();
        auto* body = static_cast<b2Body*>(rb2d.RuntimeBody);
        if (!body) {
            // Corpo criado em runtime (AddRigidbody2D/collider por script) —
            // registra agora, no primeiro frame depois da criação.
            RegisterPhysics2DEntity(entity);
            body = static_cast<b2Body*>(rb2d.RuntimeBody);
            if (!body) continue;
        }

        // Entidade inativa: corpo sai da simulação (setEnabled=false); ao
        // reativar, volta sozinho.
        if (!IsEntityActive(entity)) {
            if (body->IsEnabled()) body->SetEnabled(false);
            continue;
        }
        if (!body->IsEnabled()) body->SetEnabled(true);

        // Dynamic: corpo manda. Kinematic/Static: Transform manda (scripts
        // podem mover com SetLinearVelocity ou SetTransform).
        if (rb2d.Type == Rigidbody2DComponent::BodyType::Dynamic) {
            const auto& pos = body->GetPosition();
            transform.Translation.x = pos.x;
            transform.Translation.y = pos.y;
            transform.Rotation.z = body->GetAngle();
        } else {
            body->SetTransform({ transform.Translation.x, transform.Translation.y }, transform.Rotation.z);
        }
    }
}

void Scene::RegisterPhysics3DEntity(Entity entity) {
    if (!m_PhysicsWorld3D || !entity.HasComponent<Rigidbody3DComponent>()) return;
    auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
    if (rb3d.RuntimeBody) return;

    auto& transform = entity.GetComponent<TransformComponent>();

    // Terreno: colisor é a MESMA malha de triângulos do visual (BvhTriangle
    // — não usa heightfield do Bullet: com localScaling + offset ele não
    // colide, malha de triângulo é robusta e bate 1:1 com o relevo).
    bool isTerrain = entity.HasComponent<TerrainComponent>();

    btCollisionShape* shape = nullptr;
    if (isTerrain) {
        auto& terr = entity.GetComponent<TerrainComponent>();
        if (!terr.GeneratedMesh) terr.Regenerate();
        const auto& verts = terr.GeneratedMesh->GetVertices();
        const auto& idxs = terr.GeneratedMesh->GetIndices();
        if (verts.size() >= 3 && idxs.size() >= 3) {
            auto* mesh = new btTriangleMesh();
            for (size_t k = 0; k + 2 < idxs.size(); k += 3) {
                const auto& a = verts[idxs[k]].Position;
                const auto& b = verts[idxs[k + 1]].Position;
                const auto& c = verts[idxs[k + 2]].Position;
                mesh->addTriangle(btVector3(a.x, a.y, a.z), btVector3(b.x, b.y, b.z), btVector3(c.x, c.y, c.z));
            }
            m_PhysicsMeshes3D.push_back(mesh);
            shape = new btBvhTriangleMeshShape(mesh, true, true);
        } else {
            shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        }
    } else if (entity.HasComponent<BoxCollider3DComponent>()) {
        auto& bc3d = entity.GetComponent<BoxCollider3DComponent>();
        shape = new btBoxShape(btVector3(bc3d.HalfExtents.x * transform.Scale.x,
                                          bc3d.HalfExtents.y * transform.Scale.y,
                                          bc3d.HalfExtents.z * transform.Scale.z));
    } else if (entity.HasComponent<SphereCollider3DComponent>()) {
        auto& sc3d = entity.GetComponent<SphereCollider3DComponent>();
        shape = new btSphereShape(sc3d.Radius * transform.Scale.x);
    } else if (entity.HasComponent<MeshColliderComponent>()) {
        // Colisor de MALHA (convexo): envoltório da geometria do MeshRenderer
        // (ou de MeshPath). Amostra até MaxPoints pra custo controlado.
        auto& mc = entity.GetComponent<MeshColliderComponent>();
        Ref<Mesh> mesh = nullptr;
        if (!mc.MeshPath.empty()) mesh = Mesh::FromSource(mc.MeshPath);
        else if (entity.HasComponent<MeshRendererComponent>())
            mesh = entity.GetComponent<MeshRendererComponent>().MeshAsset;
        const auto& verts = mesh ? mesh->GetVertices() : std::vector<Vertex3D>{};
        if (verts.size() >= 4) {
            auto* hull = new btConvexHullShape();
            uint32_t step = std::max(1u, (uint32_t)(verts.size() / std::max(mc.MaxPoints, 4u)));
            for (size_t k = 0; k < verts.size(); k += step) {
                const auto& p = verts[k].Position;
                hull->addPoint(btVector3(p.x * transform.Scale.x, p.y * transform.Scale.y, p.z * transform.Scale.z));
            }
            hull->initializePolyhedralFeatures();
            shape = hull;
        } else {
            shape = new btBoxShape(btVector3(0.5f, 0.5f, 0.5f));
        }
    } else {
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
    // Gravidade escalada + amortecimento (flutuação / movimento mais macio).
    body->setGravity(m_PhysicsWorld3D->getGravity() * rb3d.GravityScale);
    body->setDamping(rb3d.LinearDamping, rb3d.AngularDamping);
    body->setUserPointer(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity.GetHandle()))));

    // Filtro de colisão por CAMADA (Tags & Layers): usa a sobrecarga clássica
    // addRigidBody(body, group, mask) — compatível com qualquer versão do Bullet.
    if (entity.HasComponent<TagComponent>()) {
        auto& tag = entity.GetComponent<TagComponent>();
        short group = (short)(1 << std::min(std::max(tag.Layer, 0), 30));
        short mask = (short)tag.CollisionMask;
        m_PhysicsWorld3D->addRigidBody(body, group, mask);
    } else {
        m_PhysicsWorld3D->addRigidBody(body);
    }
    rb3d.RuntimeBody = body;
}

// Gravidade escalada em runtime (<0 invertida, 0 = flutua).
void Scene::SetRigidbody3DGravityScale(Entity entity, float scale) {
    if (!entity.HasComponent<Rigidbody3DComponent>()) return;
    auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
    rb3d.GravityScale = scale;
    if (m_PhysicsWorld3D) {
        if (auto* body = static_cast<btRigidBody*>(rb3d.RuntimeBody))
            body->setGravity(m_PhysicsWorld3D->getGravity() * scale);
    }
}

// Amortecimento linear/angular em runtime (movimento mais macio).
void Scene::SetRigidbody3DDamping(Entity entity, float linear, float angular) {
    if (!entity.HasComponent<Rigidbody3DComponent>()) return;
    auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
    rb3d.LinearDamping = linear;
    rb3d.AngularDamping = angular;
    if (auto* body = static_cast<btRigidBody*>(rb3d.RuntimeBody))
        body->setDamping(linear, angular);
}

void Scene::UnregisterPhysics3DEntity(Entity entity) {
    if (!m_PhysicsWorld3D || !entity.HasComponent<Rigidbody3DComponent>()) return;
    auto& rb3d = entity.GetComponent<Rigidbody3DComponent>();
    auto* body = static_cast<btRigidBody*>(rb3d.RuntimeBody);
    if (!body) return;

    btCollisionShape* shape = body->getCollisionShape();
    btMotionState* motion = body->getMotionState();
    m_PhysicsWorld3D->removeRigidBody(body);
    delete body;
    rb3d.RuntimeBody = nullptr;

    if (motion) {
        auto it = std::find(m_PhysicsMotionStates3D.begin(), m_PhysicsMotionStates3D.end(), motion);
        if (it != m_PhysicsMotionStates3D.end()) m_PhysicsMotionStates3D.erase(it);
        delete motion;
    }
    if (shape) {
        auto it = std::find(m_PhysicsShapes3D.begin(), m_PhysicsShapes3D.end(), shape);
        if (it != m_PhysicsShapes3D.end()) m_PhysicsShapes3D.erase(it);
        delete shape;
    }
}

void Scene::OnPhysics3DStart() {
    m_CollisionConfig = new btDefaultCollisionConfiguration();
    m_Dispatcher = new btCollisionDispatcher(m_CollisionConfig);
    m_Broadphase = new btDbvtBroadphase();
    m_Solver = new btSequentialImpulseConstraintSolver();
    m_PhysicsWorld3D = new btDiscreteDynamicsWorld(m_Dispatcher, m_Broadphase, m_Solver, m_CollisionConfig);
    m_PhysicsWorld3D->setGravity(btVector3(0, -9.81f, 0));
    m_ActiveContacts3D.clear();

    auto view = m_Registry.view<Rigidbody3DComponent>();
    for (auto e : view)
        RegisterPhysics3DEntity(Entity{ e, this });

    // Character controllers 3D (btKinematicCharacterController): cápsula que
    // colide com paredes/subidas (step), escorrega em rampas e cai com
    // gravidade — substitui o fallback cinemático por raycast.
    auto ccView = m_Registry.view<TransformComponent, CharacterControllerComponent>();
    for (auto e : ccView) {
        Entity entity{ e, this };
        auto& cc = entity.GetComponent<CharacterControllerComponent>();
        auto& cct = entity.GetComponent<TransformComponent>();

        auto* ghost = new btPairCachingGhostObject();
        auto* capsule = new btCapsuleShape(cc.Radius, std::max(cc.Height - 2.0f * cc.Radius, 0.05f));
        m_PhysicsShapes3D.push_back(capsule);
        m_CharacterGhosts3D.push_back(ghost);

        btTransform t;
        t.setIdentity();
        t.setOrigin(btVector3(cct.Translation.x, cct.Translation.y, cct.Translation.z));
        ghost->setWorldTransform(t);
        ghost->setCollisionShape(capsule);
        ghost->setCollisionFlags(btCollisionObject::CF_CHARACTER_OBJECT);

        short group = 1, mask = -1;
        if (entity.HasComponent<TagComponent>()) {
            auto& tag = entity.GetComponent<TagComponent>();
            group = (short)(1 << std::min(std::max(tag.Layer, 0), 30));
            mask = (short)tag.CollisionMask;
        }
        m_PhysicsWorld3D->addCollisionObject(ghost, group, mask);

        auto* controller = new btKinematicCharacterController(ghost, capsule, cc.StepOffset, btVector3(0.0f, 1.0f, 0.0f));
        controller->setGravity(btVector3(0.0f, cc.Gravity, 0.0f));
        controller->setMaxSlope(glm::radians(50.0f));
        m_PhysicsWorld3D->addAction(controller);
        m_CharacterControllers3D[(uint32_t)e] = controller;
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
    for (auto* mesh : m_PhysicsMeshes3D) delete mesh;
    m_PhysicsMotionStates3D.clear();
    m_PhysicsShapes3D.clear();
    m_PhysicsMeshes3D.clear();
    m_ActiveContacts3D.clear();

    // Character controllers (ações do mundo + ghosts) — depois do world? Não:
    // remove as ações ANTES de apagar o world.
    if (m_PhysicsWorld3D) {
        for (auto& [handle, controller] : m_CharacterControllers3D) {
            if (controller) m_PhysicsWorld3D->removeAction(controller);
            delete controller;
        }
        m_CharacterControllers3D.clear();
        for (auto* ghost : m_CharacterGhosts3D) {
            if (ghost) m_PhysicsWorld3D->removeCollisionObject(ghost);
            delete ghost;
        }
        m_CharacterGhosts3D.clear();
    }

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
        if (!body) {
            RegisterPhysics3DEntity(entity); // corpo criado em runtime por script
            body = static_cast<btRigidBody*>(rb3d.RuntimeBody);
            if (!body) continue;
        }
        if (rb3d.Type == Rigidbody3DComponent::BodyType::Static) continue;
        if (!IsEntityActive(entity)) continue; // inativa não sincroniza (corpo dorme)

        btTransform bt;
        body->getMotionState()->getWorldTransform(bt);
        const auto& origin = bt.getOrigin();
        transform.Translation = { origin.x(), origin.y(), origin.z() };

        btScalar yaw, pitch, roll;
        bt.getRotation().getEulerZYX(yaw, pitch, roll);
        transform.Rotation = { roll, pitch, yaw };
    }

    // Manifolds Bullet → begin/end de colisão (comparando com o frame anterior).
    std::unordered_set<uint64_t> current;
    int numManifolds = m_Dispatcher->getNumManifolds();
    for (int i = 0; i < numManifolds; ++i) {
        btPersistentManifold* manifold = m_Dispatcher->getManifoldByIndexInternal(i);
        if (!manifold || manifold->getNumContacts() <= 0) continue;

        auto* objA = const_cast<btCollisionObject*>(manifold->getBody0());
        auto* objB = const_cast<btCollisionObject*>(manifold->getBody1());
        if (!objA || !objB) continue;

        void* upA = objA->getUserPointer();
        void* upB = objB->getUserPointer();
        if (!upA && !upB) continue;

        auto ea = upA
            ? static_cast<entt::entity>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(upA)))
            : entt::null;
        auto eb = upB
            ? static_cast<entt::entity>(static_cast<uint32_t>(reinterpret_cast<uintptr_t>(upB)))
            : entt::null;

        uint64_t key = PackEntityPair(ea, eb);
        current.insert(key);
        if (m_ActiveContacts3D.find(key) == m_ActiveContacts3D.end())
            m_CollisionBeginQueue.emplace_back(ea, eb);
    }
    for (uint64_t key : m_ActiveContacts3D) {
        if (current.find(key) == current.end()) {
            entt::entity a = static_cast<entt::entity>(static_cast<uint32_t>(key >> 32));
            entt::entity b = static_cast<entt::entity>(static_cast<uint32_t>(key & 0xffffffffu));
            m_CollisionEndQueue.emplace_back(a, b);
        }
    }
    m_ActiveContacts3D.swap(current);
}

void Scene::DispatchCollisionBegin(entt::entity self, entt::entity other) {
    if (self == entt::null || !m_Registry.valid(self)) return;
    if (!m_Registry.all_of<NativeScriptComponent>(self)) return;
    auto& nsc = m_Registry.get<NativeScriptComponent>(self);
    if (!nsc.Instance) return;
    Entity otherE = (other != entt::null && m_Registry.valid(other)) ? Entity{ other, this } : Entity{};
    nsc.Instance->OnCollisionBegin(otherE);
}

void Scene::DispatchCollisionEnd(entt::entity self, entt::entity other) {
    if (self == entt::null || !m_Registry.valid(self)) return;
    if (!m_Registry.all_of<NativeScriptComponent>(self)) return;
    auto& nsc = m_Registry.get<NativeScriptComponent>(self);
    if (!nsc.Instance) return;
    Entity otherE = (other != entt::null && m_Registry.valid(other)) ? Entity{ other, this } : Entity{};
    nsc.Instance->OnCollisionEnd(otherE);
}

void Scene::FlushCollisionEvents() {
    auto begins = std::move(m_CollisionBeginQueue);
    auto ends = std::move(m_CollisionEndQueue);
    m_CollisionBeginQueue.clear();
    m_CollisionEndQueue.clear();

    for (auto [a, b] : begins) {
        DispatchCollisionBegin(a, b);
        DispatchCollisionBegin(b, a);
    }
    for (auto [a, b] : ends) {
        DispatchCollisionEnd(a, b);
        DispatchCollisionEnd(b, a);
    }
}

void Scene::OnRuntimeStart() {
    KZ_TRACE_SCOPE("Scene::OnRuntimeStart");
    m_Running = true;
    OnPhysics2DStart();
    OnPhysics3DStart();
    BuildNavGrids(); // IA/navegação: monta as grades + obstacles

    // Diz ao ABI C# qual cena é a ativa (Play do editor / KizuriGame). Sem
    // isso, os handles de entidade resolvidos no CSharpBridge apontam pra
    // lugar nenhum e os scripts C# ficam cegos.
    kz_set_active_scene(this);

    m_Registry.view<NativeScriptComponent>().each([this](auto entityHandle, auto& nsc) {
        (void)nsc;
        StartScriptIfNeeded(Entity{ entityHandle, this });
    });
}

void Scene::OnRuntimeStop() {
    KZ_TRACE_SCOPE("Scene::OnRuntimeStop");
    m_Running = false;
    m_PendingScenePath.clear();

    OnPhysics2DStop();
    OnPhysics3DStop();

    kz_set_active_scene(nullptr);

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
    OnUpdateRuntimeLogic(ts);

    // 3D primeiro, depois 2D por cima, UI por último. O passe 3D termina num
    // composite de tela cheia — se o 2D rodasse antes (ordem antiga), o
    // composite pintava por cima e engolia o 2D. Com 3D→2D→UI, uma cena
    // híbrida (câmera perspectiva + câmera ortográfica primárias) vira o
    // clássico 2.5D: fundo/mundo 3D + camada de jogo 2D + HUD de UI.
    RenderScene3D(nullptr);
    RenderScene2D(nullptr);
    RenderUI();
}

void Scene::OnUpdateRuntimeLogic(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::OnUpdateRuntimeLogic");
    UpdateUIPointer(); // hit-test dos UIButton antes dos scripts (que leem WasClicked)

    m_InScriptUpdate = true;
    m_Registry.view<NativeScriptComponent>().each([this, ts](auto entityHandle, auto& nsc) {
        if (!IsEntityActive(Entity{ entityHandle, this })) return; // inativa não roda script
        if (nsc.Instance) nsc.Instance->OnUpdate(ts);
    });
    m_InScriptUpdate = false;
    FlushPendingDestroys(); // destroi o que os scripts enfileiraram no loop

    UpdatePhysics2D(ts);
    UpdatePhysics3D(ts);
    FlushCollisionEvents();

    kizuri::Network::Update((float)ts); // rede multiplayer (pilar AAA v0.34)
    UpdateTimelines(ts);
    UpdateCameraFollowers(ts);
    UpdateCharacterControllers(ts);
    UpdateEnemyAI(ts);
    UpdateNavAgents(ts);
    UpdateParticleSystems(ts);
    UpdateSpriteAnimations(ts);
    UpdateAnimators(ts);
    UpdateAudio(ts);
}

void Scene::RenderRuntimeView() {
    KZ_TRACE_SCOPE("Scene::RenderRuntimeView");
    // Sem rodar update nenhum: só redesenha o estado atual da cena no FBO
    // que estiver vinculado (a "Game View" do editor). Aspecto segue o
    // m_ViewportWidth/Height setado pelo OnViewportResize do editor.
    RenderScene3D(nullptr);
    RenderScene2D(nullptr);
    RenderUI();
}

void Scene::RenderRuntimeWithEditorCamera(PerspectiveCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::RenderRuntimeWithEditorCamera");
    // Viewport durante o Play: o mundo do jogo (já atualizado pela lógica)
    // visto pela CÂMERA DO EDITOR — você voa pela cena enquanto o jogo roda.
    // O passe 2D usa a câmera primária da própria cena (HUD/overlay).
    RenderScene3D(&editorCamera);
    RenderScene2D(nullptr);
    RenderUI();
}

void Scene::OnUpdateEditor3D(Timestep ts, PerspectiveCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::OnUpdateEditor3D");
    UpdateTimelines(ts);        // preview de timeline no viewport
    UpdateSpriteAnimations(ts); // preview de animação no viewport, mesmo em edição
    UpdateAnimators(ts);        // idem pros esqueletos
    // Modo 3D do viewport: malhas + grid via câmera livre do editor. O
    // passe 2D roda depois (3D→2D→UI), com a câmera primária da PRÓPRIA
    // cena — é o que permite um overlay/HUD 2D aparecer sobre uma cena 3D.
    RenderScene3D(&editorCamera);
    RenderScene2D(nullptr);
    RenderUI();
}

void Scene::OnUpdateEditor2D(Timestep ts, OrthographicCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::OnUpdateEditor2D");
    UpdateSpriteAnimations(ts); // preview de animação no viewport, mesmo em edição
    UpdateAnimators(ts);
    // Modo 2D do viewport: navegação livre (pan/zoom) via a própria
    // câmera do editor, ignorando qualquer CameraComponent da cena — não
    // precisa de uma entidade de câmera só pra poder editar sprites. Sem
    // passe 3D aqui de propósito: grid e malhas 3D só teriam papel de
    // ruído visual enquanto o foco é edição 2D.
    RenderScene2D(&editorCamera);
    RenderUI();
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
        if (!IsEntityActive(Entity{ e, this })) continue; // câmera inativa não renderiza

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
    // Ordenação por SortingLayer: menor desenha primeiro (atrás). Dentro da
    // mesma camada, a ordem é pelo tipo (sprite → círculo → animação →
    // tilemap → texto), estável (stable_sort) pra manter a ordem das views.
    struct Item { int layer; int priority; entt::entity entity; };
    std::vector<Item> items;

    auto sprites = m_Registry.view<TransformComponent, SpriteRendererComponent>();
    for (auto se : sprites)
        items.push_back({ sprites.get<SpriteRendererComponent>(se).SortingLayer, 0, se });

    auto circles = m_Registry.view<TransformComponent, CircleRendererComponent>();
    for (auto ce : circles)
        items.push_back({ circles.get<CircleRendererComponent>(ce).SortingLayer, 1, ce });

    auto anims = m_Registry.view<TransformComponent, SpriteAnimationComponent>();
    for (auto ae : anims)
        items.push_back({ anims.get<SpriteAnimationComponent>(ae).SortingLayer, 2, ae });

    auto tilemaps = m_Registry.view<TransformComponent, TilemapComponent>();
    for (auto te : tilemaps)
        items.push_back({ tilemaps.get<TilemapComponent>(te).SortingLayer, 3, te });

    auto texts = m_Registry.view<TransformComponent, TextComponent>();
    for (auto te : texts)
        items.push_back({ texts.get<TextComponent>(te).SortingLayer, 4, te });

    std::stable_sort(items.begin(), items.end(), [](const Item& a, const Item& b) {
        if (a.layer != b.layer) return a.layer < b.layer;
        return a.priority < b.priority;
    });

    for (const Item& it : items) {
        Entity e{ it.entity, this };
        if (!IsEntityActive(e)) continue; // inativa não desenha (nem filhos)
        switch (it.priority) {
        case 0: {
            auto& sprite = m_Registry.get<SpriteRendererComponent>(it.entity);
            glm::mat4 worldTransform = GetWorldTransform(e);
            if (sprite.FlipX || sprite.FlipY) {
                glm::mat4 flip = glm::scale(glm::mat4(1.0f),
                    glm::vec3(sprite.FlipX ? -1.0f : 1.0f, sprite.FlipY ? -1.0f : 1.0f, 1.0f));
                worldTransform = worldTransform * flip;
            }
            if (sprite.Texture)
                Renderer2D::DrawTransformedQuad(worldTransform, sprite.Texture, sprite.TilingFactor, sprite.Color);
            else
                Renderer2D::DrawTransformedQuad(worldTransform, sprite.Color);
            break;
        }
        case 1: {
            auto& circle = m_Registry.get<CircleRendererComponent>(it.entity);
            Renderer2D::DrawCircle(GetWorldTransform(e), circle.Color, circle.Thickness, circle.Fade);
            break;
        }
        case 2: {
            auto& anim = m_Registry.get<SpriteAnimationComponent>(it.entity);
            if (!anim.SheetTexture && !anim.SheetPath.empty())
                anim.SheetTexture = Texture2D::Create(anim.SheetPath); // carregada sob demanda
            if (!anim.SheetTexture || anim.FramesPerRow == 0) break;
            uint32_t cols = anim.FramesPerRow;
            uint32_t row = anim.CurrentFrame / cols;
            uint32_t col = anim.CurrentFrame % cols;
            uint32_t rows = (anim.TotalFrames + cols - 1) / cols;
            glm::vec2 uvMin{ (float)col / cols, 1.0f - (float)(row + 1) / rows };
            glm::vec2 uvMax{ (float)(col + 1) / cols, 1.0f - (float)row / rows };
            Renderer2D::DrawTransformedQuadUV(GetWorldTransform(e), anim.SheetTexture, uvMin, uvMax, { 1.0f, 1.0f, 1.0f, 1.0f });
            break;
        }
        case 3: {
            auto& tm = m_Registry.get<TilemapComponent>(it.entity);
            if (!tm.AtlasTexture && !tm.AtlasPath.empty())
                tm.AtlasTexture = Texture2D::Create(tm.AtlasPath);
            if (!tm.AtlasTexture || tm.AtlasColumns == 0 || tm.AtlasRows == 0 || tm.MapWidth == 0) break;
            glm::mat4 mapTransform = GetWorldTransform(e);
            for (uint32_t ty = 0; ty < tm.MapHeight; ++ty) {
                for (uint32_t tx = 0; tx < tm.MapWidth; ++tx) {
                    uint32_t idx = ty * tm.MapWidth + tx;
                    if (idx >= tm.Tiles.size() || tm.Tiles[idx] == 0) continue;
                    uint32_t tile = tm.Tiles[idx] - 1;
                    uint32_t tcol = tile % tm.AtlasColumns;
                    uint32_t trow = tile / tm.AtlasColumns;
                    glm::vec2 uvMin{ (float)tcol / tm.AtlasColumns, 1.0f - (float)(trow + 1) / tm.AtlasRows };
                    glm::vec2 uvMax{ (float)(tcol + 1) / tm.AtlasColumns, 1.0f - (float)trow / tm.AtlasRows };
                    glm::mat4 tileTransform = mapTransform *
                        glm::translate(glm::mat4(1.0f), { tx * tm.TileSize.x, ty * tm.TileSize.y, 0.0f }) *
                        glm::scale(glm::mat4(1.0f), { tm.TileSize.x, tm.TileSize.y, 1.0f });
                    Renderer2D::DrawTransformedQuadUV(tileTransform, tm.AtlasTexture, uvMin, uvMax, { 1.0f, 1.0f, 1.0f, 1.0f });
                }
            }
            break;
        }
        case 4: {
            auto& tc = m_Registry.get<TextComponent>(it.entity);
            glm::vec3 pos = glm::vec3(GetWorldTransform(e)[3]);
            TextRenderer::DrawString(tc.Text, pos, tc.FontSize, tc.Color, tc.Alignment);
            break;
        }
        default: break;
        }
    }
}

void Scene::UpdateSpriteAnimations(Timestep ts) {
    auto view = m_Registry.view<SpriteAnimationComponent>();
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue; // inativa não anima
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

// Avança o relógio dos animadores esqueléticos. Roda também em modo edição
// (preview no viewport), igual às animações de sprite.
void Scene::UpdateAnimators(Timestep ts) {
    m_Registry.view<AnimatorComponent>().each([&](auto e, auto& ac) {
        if (!IsEntityActive(Entity{ e, this })) return; // inativa não anima
        // Skin carregada sob demanda (a 1ª vez em que a entidade existe).
        if (!ac.Skin) {
            if (ac.MeshPath.empty()) return;
            ac.Skin = SkinData::CreateFromGLTF(Project::ResolvePath(ac.MeshPath));
            if (!ac.Skin) return;
        }
        if (ac.Skin->Joints.empty()) return; // arquivo sem skin
        // Máquina de estados (pilar AAA v0.35): o clip vem do estado atual.
        if (auto* sm = m_Registry.try_get<AnimatorStateMachineComponent>((entt::entity)e)) {
            if (sm->m_TransitionTime < sm->m_TransitionDuration)
                sm->m_TransitionTime += (float)ts;
            else
                sm->m_TransitionFrom = -1; // transição concluída — clip único
            if (sm->CurrentState >= 0 && sm->CurrentState < (int)sm->States.size()) {
                const auto& st = sm->States[sm->CurrentState];
                ac.ClipName = st.Clip;
                ac.Speed = st.Speed;
                ac.Loop = st.Loop;
            }
        }

        if (!ac.Playing || ac.ClipName.empty()) return;

        ac.Time += (float)ts * ac.Speed;
        if (ac.Loop) {
            float dur = ac.Skin->GetClipDuration(ac.ClipName);
            if (dur > 0.0001f) ac.Time = fmodf(ac.Time, dur);
        }
    });
}

// ---------------------------------------------------------------------------
// Animação AAA (pilar v0.34): blend de clips + IK de dois ossos.
// ---------------------------------------------------------------------------

// Quat que leva o vetor 'a' até 'b' (normalizados).
static glm::quat RotationBetweenVectors(const glm::vec3& a, const glm::vec3& b) {
    glm::vec3 na = glm::normalize(a);
    glm::vec3 nb = glm::normalize(b);
    float d = glm::clamp(glm::dot(na, nb), -1.0f, 1.0f);
    if (d > 0.9999f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
    if (d < -0.9999f) {
        glm::vec3 ax = glm::cross(na, glm::vec3(0.0f, 1.0f, 0.0f));
        if (glm::dot(ax, ax) < 1e-6f) ax = glm::cross(na, glm::vec3(1.0f, 0.0f, 0.0f));
        return glm::angleAxis(glm::pi<float>(), glm::normalize(ax));
    }
    glm::vec3 axis = glm::cross(na, nb);
    return glm::angleAxis(std::acos(d), glm::normalize(axis));
}

// IK analítico de dois ossos (triângulo): corrige root+mid pra levar tip ao
// alvo, no espaço GLOBAL. 'weight' 0..1 suaviza a correção (0 = pose original).
static void ApplyTwoBoneIK(const SkinData* skin, glm::mat4* global, const TwoBoneIKComponent& ik, int jointCount) {
    int ri = -1, mi = -1, ti = -1;
    for (int i = 0; i < jointCount; ++i) {
        const std::string& name = skin->Joints[(size_t)i].Name;
        if (name == ik.RootBone && ri < 0) ri = i;
        else if (name == ik.MidBone && mi < 0) mi = i;
        else if (name == ik.TipBone && ti < 0) ti = i;
        if (ri >= 0 && mi >= 0 && ti >= 0) break;
    }
    if (ri < 0 || mi < 0 || ti < 0) return;
    if (ri == mi || mi == ti || ri == ti) return;

    glm::vec3 root = glm::vec3(global[ri][3]);
    glm::vec3 mid = glm::vec3(global[mi][3]);
    glm::vec3 tip = glm::vec3(global[ti][3]);
    glm::vec3 target = ik.Target;

    const float a = glm::length(mid - root);
    const float b = glm::length(tip - mid);
    const float c = glm::clamp(glm::length(target - root), 1e-4f, glm::max(a + b - 1e-4f, 1e-4f));

    const float cosMid = glm::clamp((a * a + b * b - c * c) / (2.0f * a * b), -1.0f, 1.0f);
    const float midAngle = std::acos(cosMid);

    const glm::vec3 dirRootMid = glm::normalize(mid - root);
    const glm::vec3 dirRootTarget = glm::normalize(target - root);
    glm::vec3 bendAxis = glm::cross(dirRootMid, dirRootTarget);
    if (glm::dot(bendAxis, bendAxis) < 1e-6f) bendAxis = glm::vec3(0.0f, 1.0f, 0.0f);
    bendAxis = glm::normalize(bendAxis);

    // Ângulo na raiz (lei dos senos) e nova posição do meio.
    const float alpha = std::asin(glm::clamp(b * std::sin(midAngle) / c, -1.0f, 1.0f));
    glm::quat rootRotFull = glm::angleAxis(alpha, bendAxis);
    glm::vec3 dirRootMidNew = glm::normalize(rootRotFull * dirRootTarget);

    glm::quat qRoot = RotationBetweenVectors(dirRootMid, dirRootMidNew);
    glm::quat qMid = RotationBetweenVectors(tip - mid, target - mid);

    const float w = glm::clamp(ik.Weight, 0.0f, 1.0f);
    if (w <= 0.001f) return;

    // Suaviza a correção pelo peso (rotação parcial).
    auto weighted = [w](const glm::quat& q) {
        float ang = 2.0f * std::acos(glm::clamp(q.w, -1.0f, 1.0f));
        if (ang < 1e-5f) return glm::quat(1.0f, 0.0f, 0.0f, 0.0f);
        glm::vec3 ax = glm::normalize(glm::vec3(q.x, q.y, q.z));
        return glm::angleAxis(ang * w, ax);
    };
    glm::quat qRootW = weighted(qRoot);
    glm::quat qMidW = weighted(qMid);

    glm::mat4 corrRoot = glm::translate(glm::mat4(1.0f), root) * glm::mat4_cast(qRootW) * glm::translate(glm::mat4(1.0f), -root);
    glm::mat4 corrMid = glm::translate(glm::mat4(1.0f), mid) * glm::mat4_cast(qMidW) * glm::translate(glm::mat4(1.0f), -mid);

    // Aplica na ordem pai → filho (matrizes globais absolutas).
    global[ri] = corrRoot * global[ri];
    global[mi] = corrMid * (corrRoot * global[mi]);
    global[ti] = corrMid * (corrRoot * global[ti]);
}

// Pose final de um personagem: blend (se houver) + IK (se houver), no espaço
// global, e aplica o inverseBind no fim. 'count' = nº de juntas da skin.
static bool ComputeSkinnedPose(const SkinData* skin, const AnimatorComponent& ac,
                               const AnimationBlendComponent* blend, const TwoBoneIKComponent* ik,
                               const AnimatorStateMachineComponent* sm,
                               glm::mat4* outJoints, int count) {
    if (!skin || skin->Joints.empty()) return false;

    // Estado atual da máquina de animação (pilar AAA v0.35): clip primário =
    // clip do estado; durante a transição, mistura com o estado de origem.
    std::string clipA = ac.ClipName;
    std::string clipB;
    float w = 0.0f;
    if (sm && sm->CurrentState >= 0 && sm->CurrentState < (int)sm->States.size()) {
        clipA = sm->States[sm->CurrentState].Clip;
        if (sm->m_TransitionFrom >= 0 && sm->m_TransitionFrom < (int)sm->States.size() &&
            sm->m_TransitionTime < sm->m_TransitionDuration) {
            clipB = sm->States[sm->m_TransitionFrom].Clip;
            w = glm::clamp(sm->m_TransitionTime / glm::max(sm->m_TransitionDuration, 0.001f), 0.0f, 1.0f);
        }
    }
    if (clipA.empty()) clipA = ac.ClipName;

    glm::mat4 gA[kMaxSkinJoints];
    if (!skin->EvaluateGlobal(clipA, ac.Time, gA, count)) return false;

    glm::mat4 gB[kMaxSkinJoints];
    bool haveB = false;
    if (!clipB.empty() && skin->EvaluateGlobal(clipB, ac.Time, gB, count)) haveB = true;
    if (!haveB && blend && blend->UseBlend && !blend->ClipB.empty()) {
        if (skin->EvaluateGlobal(blend->ClipB, ac.Time, gB, count)) { haveB = true; clipB = blend->ClipB; }
    }
    if (!haveB) w = 0.0f;

    glm::mat4 global[kMaxSkinJoints];
    for (int i = 0; i < count; ++i) {
        if (haveB && w > 0.0001f) {
            // Mistura TRS correta (lerp posição/escala, slerp rotação).
            glm::vec3 tA = glm::vec3(gA[i][3]);
            glm::vec3 tB = glm::vec3(gB[i][3]);
            glm::vec3 sA(glm::length(glm::vec3(gA[i][0])), glm::length(glm::vec3(gA[i][1])), glm::length(glm::vec3(gA[i][2])));
            glm::vec3 sB(glm::length(glm::vec3(gB[i][0])), glm::length(glm::vec3(gB[i][1])), glm::length(glm::vec3(gB[i][2])));
            glm::mat3 rA3(gA[i]); glm::mat3 rB3(gB[i]);
            for (int c = 0; c < 3; ++c) {
                rA3[c] = glm::normalize(rA3[c]);
                rB3[c] = glm::normalize(rB3[c]);
            }
            glm::quat qA(rA3), qB(rB3);
            glm::vec3 t = glm::mix(tA, tB, w);
            glm::quat q = glm::normalize(glm::slerp(qA, qB, w));
            glm::vec3 s = glm::mix(sA, sB, w);
            global[i] = glm::translate(glm::mat4(1.0f), t) * glm::mat4_cast(q) * glm::scale(glm::mat4(1.0f), s);
        } else {
            global[i] = gA[i];
        }
    }

    if (ik) ApplyTwoBoneIK(skin, global, *ik, count);

    for (int i = 0; i < count; ++i)
        outJoints[i] = global[i] * skin->Joints[(size_t)i].InverseBind;
    return true;
}

void Scene::SubmitLights() {
    auto lights = m_Registry.view<TransformComponent, LightComponent>();
    if (lights.begin() == lights.end()) {
        Renderer3D::SubmitLight(Light{}); // sem nenhuma LightComponent na cena, mantém o sol default de antes
        return;
    }
    // Culling de luzes (pilar render v0.34): só submete luz pontual/spot cuja
    // esfera de alcance toca o frustum da câmera — direcional ilumina sempre.
    glm::mat4 cullVP = Renderer3D::GetLastViewProjection();
    for (auto e : lights) {
        if (!IsEntityActive(Entity{ e, this })) continue; // luz inativa não ilumina
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
        l.CastsShadow = lc.CastsShadow;

        if (l.Type != LightType::Directional) {
            // Esfera do alcance da luz vs frustum (AABB aproximada).
            glm::vec3 r = glm::vec3(l.Range);
            if (!AABBInFrustum(cullVP, l.Position - r, l.Position + r)) continue;
        }
        Renderer3D::SubmitLight(l);
    }
}

void Scene::UpdateParticleSystems(Timestep ts) {
    auto view = m_Registry.view<TransformComponent, ParticleSystemComponent>();
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue; // inativa não emite
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
        if (!IsEntityActive(Entity{ e, this })) continue; // inativa não desenha
        auto& pc = view.get<ParticleSystemComponent>(e);
        if (pc.ActiveParticles.empty()) continue;
        std::vector<ParticleInstance> instances;
        instances.reserve(pc.ActiveParticles.size());
        for (auto& p : pc.ActiveParticles) {
            float t = glm::clamp(p.Age / p.Lifetime, 0.0f, 1.0f);
            instances.push_back({ p.Position, glm::mix(pc.StartSize, pc.EndSize, t), glm::mix(pc.StartColor, pc.EndColor, t) });
        }
        Renderer3D::SubmitParticles(instances, pc.Additive, pc.Texture);
    }
}

void Scene::UpdateAudio(Timestep) {
    // Listener = câmera 3D principal ativa, mesma que RenderScene3D usa pra desenhar.
    auto camView = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto e : camView) {
        auto& camera = camView.get<CameraComponent>(e);
        if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Perspective3D) continue;
        if (!IsEntityActive(Entity{ e, this })) continue; // câmera inativa não é listener
        glm::mat4 world = GetWorldTransform(Entity{ e, this });
        glm::vec3 pos = glm::vec3(world[3]);
        glm::vec3 forward = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
        glm::vec3 up = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 1.0f, 0.0f));
        AudioEngine::SetListenerPosition(pos, forward, up);
        m_LastListenerPos = pos;
        break;
    }

    auto view = m_Registry.view<TransformComponent, AudioSourceComponent>();
    for (auto e : view) {
        auto& ac = view.get<AudioSourceComponent>(e);
        if (ac.ClipPath.empty()) continue;
        if (!IsEntityActive(Entity{ e, this })) {
            if (ac.Handle != kInvalidSound && ac.HasStarted) AudioEngine::Stop(ac.Handle); // inativa não toca
            continue;
        }

        if (ac.Handle == kInvalidSound) {
            ac.Handle = AudioEngine::LoadSound(Project::ResolvePath(ac.ClipPath), ac.ClipPath, false);
            if (ac.Handle == kInvalidSound) continue; // falhou (arquivo não encontrado etc) — tenta de novo no próximo frame
            AudioEngine::SetSoundAttenuation(ac.Handle, ac.MinDistance, ac.MaxDistance);
        }

        if (ac.Spatial)
            AudioEngine::SetSoundPosition3D(ac.Handle, glm::vec3(GetWorldTransform(Entity{ e, this })[3]));

        if (ac.PlayOnStart && !ac.HasStarted) {
            AudioEngine::Play(ac.Handle, ac.Loop, ac.Volume, ac.Group);
            ac.HasStarted = true;
        }

        // Reverb (pilar AAA v0.34): roteia a fonte pelo nó quando marcada.
        if (ac.HasStarted && ac.Handle != kInvalidSound) {
            bool revOn = AudioEngine::IsSoundReverbing(ac.Handle);
            if (ac.Reverb != revOn)
                AudioEngine::SetSoundReverb(ac.Handle, ac.Reverb, 1.0f);
        }

        // Oclusão (pilar AAA v0.34): fonte espacial com um corpo 3D entre ela
        // e o ouvinte é abafada (raycast do Bullet). Volume efetivo por frame.
        if (ac.Spatial && ac.HasStarted && ac.Handle != kInvalidSound) {
            float occlusion = 1.0f;
            glm::vec3 src = glm::vec3(GetWorldTransform(Entity{ e, this })[3]);
            Entity hit; glm::vec3 hp; float frac = 0.0f;
            if (Raycast3D(src, m_LastListenerPos, hit, hp, frac)) {
                // Ignora colisão com a própria entidade.
                if ((uint32_t)e != (uint32_t)hit.GetHandle()) occlusion = 0.3f;
            }
            AudioEngine::SetVolume(ac.Handle, ac.Volume * occlusion);
        }
    }
}

void Scene::UpdateTimelines(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::UpdateTimelines");
    auto view = m_Registry.view<TransformComponent, TimelineComponent>();
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue; // inativa não anima timeline
        auto& tc = view.get<TransformComponent>(e);
        auto& tl = view.get<TimelineComponent>(e);
        if (tl.Playing && !tl.Keyframes.empty()) {
            tl.Time += (float)ts * tl.Speed;
            float dur = tl.Duration();
            if (dur > 0.0f && tl.Time > dur) tl.Time = tl.Loop ? glm::mod(tl.Time, dur) : dur;
        }
        if (tl.Keyframes.empty()) continue;

        // Interpolação linear entre os 2 keyframes vizinhos.
        glm::vec3 pos = tl.Keyframes[0].Position;
        glm::vec3 rot = tl.Keyframes[0].Rotation;
        glm::vec3 scl = tl.Keyframes[0].Scale;
        for (size_t i = 0; i + 1 < tl.Keyframes.size(); ++i) {
            const auto& a = tl.Keyframes[i];
            const auto& b = tl.Keyframes[i + 1];
            if (tl.Time >= a.Time && tl.Time <= b.Time) {
                float t = (b.Time - a.Time) > 0.0001f ? (tl.Time - a.Time) / (b.Time - a.Time) : 0.0f;
                t = glm::clamp(t, 0.0f, 1.0f);
                pos = glm::mix(a.Position, b.Position, t);
                rot = glm::mix(a.Rotation, b.Rotation, t);
                scl = glm::mix(a.Scale, b.Scale, t);
                break;
            }
        }
        tc.Translation = pos;
        tc.Rotation = rot;
        tc.Scale = scl;
    }
}

void Scene::UpdateCharacterControllers(Timestep ts) {
    KZ_TRACE_SCOPE("Scene::UpdateCharacterControllers");
    auto view = m_Registry.view<TransformComponent, CharacterControllerComponent>();
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue; // inativo não se move
        auto& tc = view.get<TransformComponent>(e);
        auto& cc = view.get<CharacterControllerComponent>(e);

        // v2: controller físico do Bullet (colide com paredes/terreno/step).
        auto it = m_CharacterControllers3D.find((uint32_t)e);
        if (it != m_CharacterControllers3D.end() && it->second && m_PhysicsWorld3D) {
            auto* controller = it->second;
            // Sincroniza o ghost com o transform (permite SetPosition/teleporte
            // vindo de script) antes de aplicar o movimento do frame.
            btVector3 entPos(tc.Translation.x, tc.Translation.y, tc.Translation.z);
            if (entPos.distance2(controller->getGhostObject()->getWorldTransform().getOrigin()) > 1e-6) {
                btTransform gt;
                gt.setIdentity();
                gt.setOrigin(entPos);
                controller->getGhostObject()->setWorldTransform(gt);
                controller->warp(entPos);
            }
            btVector3 walk(cc.Input.x * cc.Speed, 0.0f, cc.Input.y * cc.Speed);
            controller->setWalkDirection(walk);
            const btVector3& o = controller->getGhostObject()->getWorldTransform().getOrigin();
            tc.Translation = { o.x(), o.y(), o.z() };
            cc.Grounded = controller->onGround();
            cc.Velocity = { walk.x(), walk.y(), walk.z() };
            continue;
        }

        // Fallback cinemático (sem física 3D iniciada / criado em runtime):
        // movimento horizontal + gravidade + chão por raycast.
        cc.Velocity.x = cc.Input.x * cc.Speed;
        cc.Velocity.z = cc.Input.y * cc.Speed;
        cc.Velocity.y += cc.Gravity * (float)ts;
        if (cc.Velocity.y < -40.0f) cc.Velocity.y = -40.0f;

        glm::vec3 newPos = tc.Translation + cc.Velocity * (float)ts;

        // Detecção de chão: raio do topo do corpo até abaixo dos pés.
        Entity hit; glm::vec3 hitPoint; float frac = 0.0f;
        glm::vec3 start = tc.Translation + glm::vec3(0.0f, cc.Height * 0.5f + 0.1f, 0.0f);
        glm::vec3 end   = tc.Translation + glm::vec3(0.0f, -cc.Height * 0.5f - 0.15f, 0.0f);
        if (Raycast3D(start, end, hit, hitPoint, frac)) {
            cc.Grounded = true;
            newPos.y = hitPoint.y + 0.01f;
            cc.Velocity.y = 0.0f;
        } else {
            cc.Grounded = false;
            if (newPos.y < 0.0f) newPos.y = 0.0f;
        }

        tc.Translation = newPos;
    }
}

// ---------------------------------------------------------------------------
// Lightmap (pilar AAA v0.35)
// ---------------------------------------------------------------------------

// Interseção raio-triângulo (Möller-Trumbore) em espaço local da malha.
static bool RayTriangleMT(const glm::vec3& origin, const glm::vec3& dir,
                          const glm::vec3& a, const glm::vec3& b, const glm::vec3& c,
                          float& outT) {
    const float eps = 1e-8f;
    glm::vec3 e1 = b - a, e2 = c - a;
    glm::vec3 pvec = glm::cross(dir, e2);
    float det = glm::dot(e1, pvec);
    if (std::abs(det) < eps) return false;
    float inv = 1.0f / det;
    glm::vec3 tvec = origin - a;
    float u = glm::dot(tvec, pvec) * inv;
    if (u < 0.0f || u > 1.0f) return false;
    glm::vec3 qvec = glm::cross(tvec, e1);
    float v = glm::dot(dir, qvec) * inv;
    if (v < 0.0f || u + v > 1.0f) return false;
    outT = glm::dot(e2, qvec) * inv;
    return outT >= 0.0f;
}

void Scene::BakeLightmap(Entity entity) {
    if (!entity || !entity.HasComponent<MeshRendererComponent>()) return;
    auto& mr = entity.GetComponent<MeshRendererComponent>();
    if (!mr.MeshAsset) return;
    const auto& verts = mr.MeshAsset->GetVertices();
    const auto& idxs = mr.MeshAsset->GetIndices();
    if (verts.empty() || idxs.size() < 3) return;

    // Pré-coleta das malhas da cena em espaço de MUNDO (os raycasts do bake).
    struct TraceMesh { glm::mat4 InvWorld; glm::vec3 Min, Max; const std::vector<Vertex3D>* Pos; const std::vector<uint32_t>* Idx; };
    std::vector<TraceMesh> statics;

    auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();
    for (auto me : meshes) {
        auto& mr2 = meshes.get<MeshRendererComponent>(me);
        if (!mr2.MeshAsset || mr2.MeshAsset->GetVertices().empty()) continue;
        Entity ent{ me, this };
        glm::mat4 world = GetWorldTransform(ent);
        TraceMesh tm;
        tm.InvWorld = glm::inverse(world);
        tm.Pos = &mr2.MeshAsset->GetVertices();
        tm.Idx = &mr2.MeshAsset->GetIndices();
        tm.Min = mr2.MeshAsset->GetBoundsMin();
        tm.Max = mr2.MeshAsset->GetBoundsMax();
        statics.push_back(tm);
    }

    // Função de trace: da origem na direção, devolve distância OU -1.
    auto trace = [&](const glm::vec3& origin, const glm::vec3& dir) -> float {
        float best = -1.0f;
        for (const auto& sm : statics) {
            if (!sm.Pos || !sm.Idx) continue;
            glm::vec3 localO = glm::vec3(sm.InvWorld * glm::vec4(origin, 1.0f));
            glm::vec3 localD = glm::vec3(sm.InvWorld * glm::vec4(dir, 0.0f));
            float len = glm::length(localD);
            if (len < 1e-9f) continue;
            glm::vec3 dirN = localD / len;

            // AABB local rápido.
            glm::vec3 o = localO, d = dirN, mn = sm.Min, mx = sm.Max;
            float t0 = -FLT_MAX, t1 = FLT_MAX;
            bool inside = true;
            for (int a = 0; a < 3; ++a) {
                if (std::abs(d[a]) < 1e-9f) { if (o[a] < mn[a] || o[a] > mx[a]) { inside = false; break; } }
                else {
                    float ta = (mn[a] - o[a]) / d[a], tb = (mx[a] - o[a]) / d[a];
                    if (ta > tb) std::swap(ta, tb);
                    t0 = glm::max(t0, ta); t1 = glm::min(t1, tb);
                    if (t0 > t1) { inside = false; break; }
                }
            }
            if (!inside) continue;
            if (t1 < 0.0f) continue;

            const auto& P = *sm.Pos;
            const auto& I = *sm.Idx;
            for (size_t k = 0; k + 2 < I.size(); k += 3) {
                uint32_t i0 = I[k], i1 = I[k + 1], i2 = I[k + 2];
                if (i0 >= P.size() || i1 >= P.size() || i2 >= P.size()) continue;
                float t;
                if (RayTriangleMT(localO, dirN, P[i0].Position, P[i1].Position, P[i2].Position, t)) {
                    if (t > 0.001f && (best < 0.0f || t < best)) best = t;
                }
            }
        }
        return best;
    };

    // Direção do sol da cena (1ª luz direcional, se existir).
    glm::vec3 sunDir{ 0.3f, -0.9f, -0.25f };
    glm::vec3 sunColor{ 1.0f, 0.95f, 0.85f };
    auto lights = m_Registry.view<TransformComponent, LightComponent>();
    for (auto le : lights) {
        auto& lc = lights.get<LightComponent>(le);
        if (lc.Type != LightType::Directional) continue;
        glm::mat4 lw = GetWorldTransform(Entity{ le, this });
        sunDir = glm::normalize(glm::mat3(lw) * glm::vec3(0.0f, -1.0f, 0.0f));
        sunColor = lc.Color;
        break;
    }

    // Converte pra vetores simples pro baker (posições/normais/UVs).
    struct BakeGeom { std::vector<glm::vec3> Pos, Nrm; std::vector<glm::vec2> UV; };
    BakeGeom geom;
    geom.Pos.reserve(verts.size());
    geom.Nrm.reserve(verts.size());
    geom.UV.reserve(verts.size());
    for (const auto& v : verts) { geom.Pos.push_back(v.Position); geom.Nrm.push_back(v.Normal); geom.UV.push_back(v.TexCoord); }

    LightmapBaker::Input in;
    in.Positions = &geom.Pos;
    in.Normals = &geom.Nrm;
    in.TexCoords = &geom.UV;
    in.Indices = &idxs;
    in.SunDir = sunDir;
    in.SunColor = sunColor;

    auto lightmap = LightmapBaker::Bake(in, trace);
    if (lightmap) {
        mr.LightmapTexture = lightmap;
        KZ_CORE_INFO("Lightmap assada em '{0}' ({1} vértices).", entity.GetName(), verts.size());
    }
}

// ---------------------------------------------------------------------------
// IA e Navegação (pilar AAA v0.34)
// ---------------------------------------------------------------------------

NavGrid* Scene::FindGridNear(const glm::vec3& pos) const {
    NavGrid* best = nullptr;
    float bestArea = FLT_MAX;
    auto view = m_Registry.view<TransformComponent, NavGridComponent>();
    for (auto e : view) {
        auto& ngc = view.get<NavGridComponent>(e);
        if (!ngc.Grid || ngc.Grid->GetWidth() <= 0 || ngc.Grid->GetDepth() <= 0) continue;
        const NavGrid& g = *ngc.Grid;
        int x, z;
        if (!g.WorldToCell(pos, x, z)) continue; // fora dessa grade
        float area = (float)g.GetWidth() * (float)g.GetDepth();
        if (area < bestArea) { bestArea = area; best = ngc.Grid.get(); }
    }
    return best;
}

void Scene::RebuildNavGrid(Entity gridEntity) {
    if (!gridEntity || !gridEntity.HasComponent<NavGridComponent>()) return;
    auto& ngc = gridEntity.GetComponent<NavGridComponent>();
    if (!ngc.Grid) ngc.Grid = std::make_shared<NavGrid>();
    NavGrid& g = *ngc.Grid;
    g.Build(ngc.Origin.x, ngc.Origin.z, ngc.CellSize, (int)ngc.Width, (int)ngc.Depth);

    // Rasteriza os NavObstacleComponent ativos da cena na grade.
    if (ngc.AutoBuild) {
        auto obsv = m_Registry.view<TransformComponent, NavObstacleComponent>();
        for (auto oe : obsv) {
            Entity obEnt{ oe, this };
            if (!IsEntityActive(obEnt)) continue;
            auto& tc = obsv.get<TransformComponent>(oe);
            auto& ob = obsv.get<NavObstacleComponent>(oe);
            glm::vec3 half = ob.HalfExtents;
            if (half.x <= 0.0f && half.y <= 0.0f && half.z <= 0.0f)
                half = tc.Scale * 0.5f; // vazio = usa a escala do transform
            g.RasterizeBox(tc.Translation, half);
        }
    }
}

void Scene::BuildNavGrids() {
    auto view = m_Registry.view<TransformComponent, NavGridComponent>();
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue;
        RebuildNavGrid(Entity{ e, this });
    }
}

void Scene::SetNavDestination(Entity agent, const glm::vec3& destination) {
    if (!agent || !agent.HasComponent<NavAgentComponent>()) return;
    auto& na = agent.GetComponent<NavAgentComponent>();
    na.Destination = destination;
    na.HasDestination = true;
    na.Path.clear();
    na.PathIndex = 0;
    na.PathTimer = 0.0f;
}

void Scene::StopNavAgent(Entity agent) {
    if (!agent || !agent.HasComponent<NavAgentComponent>()) return;
    auto& na = agent.GetComponent<NavAgentComponent>();
    na.HasDestination = false;
    na.Path.clear();
    na.PathIndex = 0;
}

bool Scene::NavAgentHasPath(Entity agent) const {
    if (!agent) return false;
    auto* na = m_Registry.try_get<NavAgentComponent>(agent.GetHandle());
    return na && na->HasDestination && na->PathIndex < na->Path.size();
}

float Scene::NavAgentRemainingDistance(Entity agent) const {
    if (!agent) return 0.0f;
    auto* na = m_Registry.try_get<NavAgentComponent>(agent.GetHandle());
    if (!na || !na->HasDestination) return 0.0f;
    const TransformComponent* tc = m_Registry.try_get<TransformComponent>(agent.GetHandle());
    float dist = 0.0f;
    glm::vec3 prev = tc ? tc->Translation : glm::vec3(0.0f);
    for (size_t i = na->PathIndex; i < na->Path.size(); ++i) {
        dist += glm::length(na->Path[i] - prev);
        prev = na->Path[i];
    }
    return dist;
}

bool Scene::NavAgentReached(Entity agent) const {
    if (!agent) return true;
    auto* na = m_Registry.try_get<NavAgentComponent>(agent.GetHandle());
    return !na || !na->HasDestination;
}

// Máquina de estados do inimigo: Patrulha → Persegue → Ataca, dirigindo o
// NavAgent da própria entidade (ou de um filho direto).
void Scene::UpdateEnemyAI(Timestep ts) {
    float dt = (float)ts;
    auto view = m_Registry.view<TransformComponent, EnemyAIComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        if (!IsEntityActive(entity)) continue;
        auto& ai = view.get<EnemyAIComponent>(e);
        ai.m_StateTimer += dt;

        // Resolve o alvo pela tag (barato, uma vez por frame).
        ai.m_HasTarget = false;
        if (!ai.TargetTag.empty()) {
            auto tags = m_Registry.view<TransformComponent, TagComponent>();
            for (auto te : tags) {
                if (tags.get<TagComponent>(te).Tag == ai.TargetTag) {
                    auto* id = m_Registry.try_get<IDComponent>(te);
                    if (id && id->Active) { ai.m_TargetHandle = (uint32_t)te; ai.m_HasTarget = true; }
                    break;
                }
            }
        }

        // NavAgent: na própria entidade ou num filho direto.
        Entity navEntity = entity.HasComponent<NavAgentComponent>() ? entity : Entity{};
        if (!navEntity) {
            for (Entity child : entity.GetChildren())
                if (child.HasComponent<NavAgentComponent>()) { navEntity = child; break; }
        }
        if (!navEntity) continue;

        glm::vec3 selfPos = entity.GetComponent<TransformComponent>().Translation;
        float distToTarget = FLT_MAX;
        if (ai.m_HasTarget) {
            entt::entity te = entt::entity(ai.m_TargetHandle);
            if (m_Registry.valid(te)) {
                Entity t{ te, this };
                if (t.HasComponent<TransformComponent>())
                    distToTarget = glm::length(t.GetComponent<TransformComponent>().Translation - selfPos);
            }
        }

        switch (ai.m_State) {
        case EnemyAIComponent::State::Patrol: {
            if (!ai.PatrolPoints.empty()) {
                // Anda de ponto em ponto; espera PatrolWait em cada um.
                if (!NavAgentHasPath(navEntity)) {
                    ai.m_PatrolTimer += dt;
                    if (ai.m_PatrolTimer >= ai.PatrolWait) {
                        ai.m_PatrolTimer = 0.0f;
                        SetNavDestination(navEntity, ai.PatrolPoints[ai.m_PatrolIndex]);
                    }
                } else if (NavAgentReached(navEntity)) {
                    ai.m_PatrolTimer += dt;
                    if (ai.m_PatrolTimer >= ai.PatrolWait) {
                        ai.m_PatrolTimer = 0.0f;
                        ai.m_PatrolIndex = (ai.m_PatrolIndex + 1) % (int)ai.PatrolPoints.size();
                        SetNavDestination(navEntity, ai.PatrolPoints[ai.m_PatrolIndex]);
                    }
                }
            }
            if (ai.m_HasTarget && distToTarget <= ai.SightRange) {
                ai.m_State = EnemyAIComponent::State::Chase;
                ai.m_StateTimer = 0.0f;
            }
            break;
        }
        case EnemyAIComponent::State::Chase: {
            if (!ai.m_HasTarget || distToTarget > ai.LoseRange) { ai.m_State = EnemyAIComponent::State::Patrol; break; }
            if (distToTarget <= ai.ChaseRange) {
                StopNavAgent(navEntity);
                ai.m_State = EnemyAIComponent::State::Attack;
                ai.m_StateTimer = 0.0f;
                break;
            }
            if (ai.m_StateTimer >= 0.25f) { // recalcula o destino (o alvo se move)
                ai.m_StateTimer = 0.0f;
                entt::entity te = entt::entity(ai.m_TargetHandle);
                if (m_Registry.valid(te)) {
                    Entity t{ te, this };
                    if (t.HasComponent<TransformComponent>())
                        SetNavDestination(navEntity, t.GetComponent<TransformComponent>().Translation);
                }
            }
            break;
        }
        case EnemyAIComponent::State::Attack: {
            if (!ai.m_HasTarget || distToTarget > ai.ChaseRange) { ai.m_State = EnemyAIComponent::State::Chase; break; }
            if (ai.m_StateTimer >= ai.AttackCooldown) {
                ai.m_StateTimer = 0.0f;
                if (entity.HasComponent<NativeScriptComponent>() &&
                    entity.GetComponent<NativeScriptComponent>().Instance)
                    entity.GetComponent<NativeScriptComponent>().Instance->OnEnemyAttack(ai.AttackDamage);
            }
            break;
        }
        }
    }
}

// Move os NavAgent ao longo do caminho no plano XZ; altura fica com o
// script/controller. Recalcula o caminho periodicamente (destino mutável).
void Scene::UpdateNavAgents(Timestep ts) {
    float dt = (float)ts;
    auto view = m_Registry.view<TransformComponent, NavAgentComponent>();
    for (auto e : view) {
        Entity entity{ e, this };
        if (!IsEntityActive(entity)) continue;
        auto& tc = view.get<TransformComponent>(e);
        auto& na = view.get<NavAgentComponent>(e);
        if (!na.Enabled || !na.HasDestination) continue;

        NavGrid* grid = FindGridNear(tc.Translation);
        if (!grid) { StopNavAgent(entity); continue; }

        na.PathTimer -= dt;
        if (na.PathTimer <= 0.0f) {
            na.PathTimer = 0.35f;
            na.Path = grid->SmoothPath(grid->FindPath(tc.Translation, na.Destination));
            na.PathIndex = 0;
        }

        if (na.PathIndex >= na.Path.size()) continue;
        glm::vec3 wp = na.Path[na.PathIndex];
        glm::vec3 to = wp - tc.Translation;
        to.y = 0.0f;
        float d = glm::length(to);
        if (d < na.StopDistance) {
            ++na.PathIndex;
            if (na.PathIndex >= na.Path.size()) {
                glm::vec3 dd = na.Destination - tc.Translation;
                dd.y = 0.0f;
                if (glm::length(dd) <= na.StopDistance) StopNavAgent(entity);
                else { na.Path.clear(); na.PathIndex = 0; na.PathTimer = 0.0f; }
            }
            continue;
        }

        glm::vec2 dir{ to.x / d, to.z / d };
        glm::vec2 step = dir * na.Speed * dt;
        glm::vec3 ahead = tc.Translation + glm::vec3(step.x, 0.0f, step.y);
        // Não invade célula bloqueada na frente (margem simples).
        int ax, az;
        if (!grid->WorldToCell(ahead, ax, az) || grid->IsBlocked(ax, az)) {
            // Só gira e tenta recalcular o caminho (obstáculo à frente).
            na.Path.clear(); na.PathIndex = 0; na.PathTimer = 0.0f;
            continue;
        }
        tc.Translation.x += step.x;
        tc.Translation.z += step.y;

        if (na.FaceMovement) {
            float targetYaw = std::atan2(step.y, step.x);
            float cur = tc.Rotation.y;
            float diff = targetYaw - cur;
            const float kPi = 3.14159265358979323846f;
            while (diff > kPi) diff -= 2.0f * kPi;
            while (diff < -kPi) diff += 2.0f * kPi;
            float k = glm::clamp(na.TurnSpeed * dt, 0.0f, 1.0f);
            tc.Rotation.y = cur + diff * k;
        }
    }
}

// Avança as câmeras com CameraFollowComponent: segue o alvo (pelo nome) com
// lerp exponencial. Roda a cada frame no runtime (Play/GameView).
void Scene::UpdateCameraFollowers(Timestep ts) {
    auto view = m_Registry.view<TransformComponent, CameraComponent, CameraFollowComponent>();
    if (view.begin() == view.end()) return;

    // Resolve o alvo pelo nome uma única vez por frame (poucos followers).
    std::unordered_map<std::string, entt::entity> byName;
    auto tags = m_Registry.view<TagComponent>();
    for (auto te : tags) {
        const auto& name = tags.get<TagComponent>(te).Tag;
        if (!name.empty()) byName.emplace(name, te);
    }

    float dt = (float)ts;
    for (auto e : view) {
        if (!IsEntityActive(Entity{ e, this })) continue; // inativa não segue
        auto& tc = view.get<TransformComponent>(e);
        auto& cc = view.get<CameraFollowComponent>(e);

        auto it = byName.find(cc.TargetName);
        if (it == byName.end() || it->second == e) {
            cc.m_HasStart = false;
            continue; // sem alvo — só espera
        }

        Entity target{ it->second, this };
        glm::mat4 targetWorld = GetWorldTransform(target);
        glm::vec3 targetPos = glm::vec3(targetWorld[3]);

        float targetYaw = 0.0f;
        if (cc.FollowRotation) {
            glm::vec3 tpos, teuler;
            DecomposeTransform(targetWorld, tpos, teuler);
            targetYaw = teuler.y;
        }

        glm::vec3 desired;
        if (cc.UseWorldOffset || !cc.FollowRotation) {
            desired = targetPos + cc.Offset;
        } else {
            // Offset gira junto com o alvo (fica sempre "atrás" dele).
            float cy = glm::cos(targetYaw), sy = glm::sin(targetYaw);
            glm::vec3 local = { cc.Offset.x, cc.Offset.y, cc.Offset.z };
            desired = targetPos + glm::vec3(cy * local.x + sy * local.z, local.y, -sy * local.x + cy * local.z);
        }

        float t = cc.Smoothness > 0.0001f ? 1.0f - glm::exp(-cc.Smoothness * dt) : 1.0f;
        if (!cc.m_HasStart) { cc.m_CurrentPos = desired; cc.m_HasStart = true; }
        else cc.m_CurrentPos = glm::mix(cc.m_CurrentPos, desired, glm::clamp(t, 0.0f, 1.0f));

        tc.Translation = cc.m_CurrentPos;
        if (cc.FollowRotation) tc.Rotation.y = targetYaw;
    }
}

void Scene::RenderScene3D(PerspectiveCamera* overrideCamera) {
    KZ_TRACE_SCOPE("Scene::RenderScene3D");

    // VP usada pro FRUSTUM CULLING (não desenhar o que está fora da câmera —
    // sistema de engine de verdade). Esqueletos (skinned) NÃO são culled
    // (a pose animada pode sair da AABB de repouso).
    glm::mat4 cullVP = glm::mat4(0.0f);
    glm::vec3 camPos(0.0f);
    bool cullCam = false;
    if (overrideCamera) {
        cullVP = overrideCamera->GetViewProjectionMatrix();
        camPos = overrideCamera->GetPosition();
        cullCam = true;
    } else {
        auto camView = m_Registry.view<TransformComponent, CameraComponent>();
        for (auto e : camView) {
            auto& camera = camView.get<CameraComponent>(e);
            if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Perspective3D) continue;
            if (!IsEntityActive(Entity{ e, this })) continue;
            glm::vec3 pos = glm::vec3(GetWorldTransform(Entity{ e, this })[3]);
            const auto& tc = camView.get<TransformComponent>(e);
            float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;
            PerspectiveCamera cam(camera.PerspectiveFOV, aspect, camera.NearClip, camera.FarClip);
            cam.SetPosition(pos);
            cam.SetRotation(glm::degrees(tc.Rotation.y), glm::degrees(tc.Rotation.x));
            cullVP = cam.GetViewProjectionMatrix();
            camPos = pos;
            cullCam = true;
            break;
        }
    }

    // Culling de OCULSÃO por occluders (pilar AAA v0.35): um objeto é
    // descartado quando a projeção da AABB dele cabe inteira DENTRO da
    // projeção de um OccluderComponent E está mais fundo que ele (margens
    // de segurança pra não abrir furos nas bordas).
    struct OccluderProj { float MinX, MinY, MaxX, MaxY; float NearZ; };
    std::vector<OccluderProj> occluders;
    if (cullCam) {
        auto occView = m_Registry.view<TransformComponent, OccluderComponent>();
        for (auto oe : occView) {
            Entity oEnt{ oe, this };
            if (!IsEntityActive(oEnt)) continue;
            auto& occ = occView.get<OccluderComponent>(oe);
            glm::mat4 ow = GetWorldTransform(oEnt);
            glm::vec3 half = occ.HalfExtents;
            if (half.x <= 0.0f && half.y <= 0.0f && half.z <= 0.0f)
                half = oEnt.GetComponent<TransformComponent>().Scale * 0.5f;
            glm::vec3 center = glm::vec3(ow[3]);
            if (glm::length(center - camPos) > occ.MaxOcclusionDistance) continue;

            OccluderProj op;
            op.MinX = op.MinY = 1e30f;
            op.MaxX = op.MaxY = -1e30f;
            op.NearZ = 1e30f;
            bool ok = true;
            for (int i = 0; i < 8; ++i) {
                glm::vec3 sgn((i & 1) ? half.x : -half.x, (i & 2) ? half.y : -half.y, (i & 4) ? half.z : -half.z);
                glm::vec4 wp = ow * glm::vec4(sgn, 1.0f);
                glm::vec4 clip = cullVP * wp;
                if (clip.w <= 0.001f) { ok = false; break; }
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                op.MinX = std::min(op.MinX, ndc.x); op.MaxX = std::max(op.MaxX, ndc.x);
                op.MinY = std::min(op.MinY, ndc.y); op.MaxY = std::max(op.MaxY, ndc.y);
                op.NearZ = std::min(op.NearZ, ndc.z);
            }
            if (!ok) continue;
            occluders.push_back(op);
        }
    }
    auto IsOccluded = [&](const glm::vec3& wmin, const glm::vec3& wmax) {
        if (occluders.empty()) return false;
        for (const auto& op : occluders) {
            float oMinX = 1e30f, oMinY = 1e30f, oMaxX = -1e30f, oMaxY = -1e30f, farZ = -1e30f;
            bool ok = true;
            glm::vec3 c[8];
            for (int i = 0; i < 8; ++i) {
                glm::vec3 sgn((i & 1) ? wmax.x : wmin.x, (i & 2) ? wmax.y : wmin.y, (i & 4) ? wmax.z : wmin.z);
                c[i] = sgn;
                glm::vec4 clip = cullVP * glm::vec4(c[i], 1.0f);
                if (clip.w <= 0.001f) { ok = false; break; }
                glm::vec3 ndc = glm::vec3(clip) / clip.w;
                oMinX = std::min(oMinX, ndc.x); oMaxX = std::max(oMaxX, ndc.x);
                oMinY = std::min(oMinY, ndc.y); oMaxY = std::max(oMaxY, ndc.y);
                farZ = std::max(farZ, ndc.z);
            }
            if (!ok) continue;
            const float margin = 0.02f; // ~2% da tela nas bordas
            if (oMinX >= op.MinX + margin && oMaxX <= op.MaxX - margin &&
                oMinY >= op.MinY + margin && oMaxY <= op.MaxY - margin &&
                farZ >= op.NearZ + 0.001f)
                return true;
        }
        return false;
    };

    auto SubmitMeshes = [&]() {
        auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();

        // Foliage (pilar AAA v0.35): vegetação instanciada (1 draw call).
        auto foliage = m_Registry.view<TransformComponent, FoliageComponent>();
        for (auto fe : foliage) {
            Entity ent{ fe, this };
            if (!IsEntityActive(ent)) continue;
            auto& fc = foliage.get<FoliageComponent>(fe);
            if (fc.Instances.empty()) fc.Regenerate();
            if (fc.Instances.empty()) continue;
            if (!fc.MeshAsset) fc.MeshAsset = Mesh::FromSource(fc.MeshSource);
            if (!fc.MeshAsset) continue;
            Material mat;
            mat.Albedo = { fc.Color.r, fc.Color.g, fc.Color.b };
            mat.Roughness = 0.85f;
            // A origem da entidade desloca todo o bloco de vegetação.
            glm::mat4 base = GetWorldTransform(ent);
            std::vector<glm::mat4> worldInsts;
            worldInsts.reserve(fc.Instances.size());
            for (auto& inst : fc.Instances) worldInsts.push_back(base * inst);
            Renderer3D::SubmitMeshInstances(fc.MeshAsset, mat, worldInsts.data(), (uint32_t)worldInsts.size());
        }

        // Decals (pilar AAA v0.35): caixas projetoras com textura.
        auto decals = m_Registry.view<TransformComponent, DecalComponent>();
        for (auto de : decals) {
            Entity ent{ de, this };
            if (!IsEntityActive(ent)) continue;
            auto& dc = decals.get<DecalComponent>(de);
            if (!dc.Texture && !dc.TexturePath.empty())
                dc.Texture = Texture2D::Create(Project::ResolvePath(dc.TexturePath));
            if (!dc.Texture) continue;
            Renderer3D::SubmitDecal(GetWorldTransform(ent), dc.Texture, dc.Color);
        }
        for (auto me : meshes) {
            if (!IsEntityActive(Entity{ me, this })) continue; // inativa não desenha
            auto& mr = meshes.get<MeshRendererComponent>(me);
            if (!mr.MeshAsset) continue;
            glm::mat4 world = GetWorldTransform(Entity{ me, this });
            auto* animator = m_Registry.try_get<AnimatorComponent>(me);
            if (animator && animator->Skin && !animator->Skin->Joints.empty()) {
                glm::mat4 joints[kMaxSkinJoints];
                auto* blend = m_Registry.try_get<AnimationBlendComponent>(me);
                auto* ik = m_Registry.try_get<TwoBoneIKComponent>(me);
                auto* sm = m_Registry.try_get<AnimatorStateMachineComponent>(me);
                int jointCount = (int)std::min((size_t)animator->Skin->Joints.size(), (size_t)kMaxSkinJoints);
                if (ComputeSkinnedPose(animator->Skin.get(), *animator, blend, ik, sm, joints, jointCount))
                    Renderer3D::SubmitSkinned(mr.MeshAsset, mr.MeshMaterial, world, joints,
                                              (uint32_t)jointCount);
                else
                    Renderer3D::Submit(mr.MeshAsset, mr.MeshMaterial, world, mr.LightmapTexture);
                continue;
            }

            // AABB da malha em espaço de MUNDO (base dos dois cullings).
            glm::vec3 wmin(1e30f), wmax(-1e30f);
            if (cullCam) {
                glm::vec3 mn = mr.MeshAsset->GetBoundsMin(), mx = mr.MeshAsset->GetBoundsMax();
                glm::vec3 corners[8] = {
                    {mn.x,mn.y,mn.z},{mx.x,mn.y,mn.z},{mn.x,mx.y,mn.z},{mx.x,mx.y,mn.z},
                    {mn.x,mn.y,mx.z},{mx.x,mn.y,mx.z},{mn.x,mx.y,mx.z},{mx.x,mx.y,mx.z}
                };
                for (auto& c : corners) {
                    glm::vec3 w = glm::vec3(world * glm::vec4(c, 1.0f));
                    wmin = glm::min(wmin, w);
                    wmax = glm::max(wmax, w);
                }
            }

            // Culling de oclusão: totalmente atrás de um occluder => pula.
            if (cullCam && IsOccluded(wmin, wmax)) continue;

            // Frustum culling: AABB fora da câmera => não submete.
            if (cullCam) {
                if (!AABBInFrustum(cullVP, wmin, wmax)) continue;

                // Culling por TAMANHO DE TELA: objeto cuja AABB projetada cabe
                // em ~1px não é desenhado (detalhe irrelevante a essa distância).
                glm::vec4 cmin = cullVP * glm::vec4(wmin, 1.0f);
                glm::vec4 cmax = cullVP * glm::vec4(wmax, 1.0f);
                if (cmin.w > 0.0f && cmax.w > 0.0f) {
                    glm::vec2 p0 = glm::vec2(cmin) / cmin.w;
                    glm::vec2 p1 = glm::vec2(cmax) / cmax.w;
                    glm::vec2 screenPx = (p1 - p0) * 0.5f *
                        glm::vec2((float)m_ViewportWidth, (float)m_ViewportHeight);
                    if (glm::dot(screenPx, screenPx) < 1.0f) continue; // < ~1px
                }
            }

            // LOD (Level of Detail): troca a malha pela distância à câmera.
            Ref<Mesh> mesh = mr.MeshAsset;
            if (auto* lod = m_Registry.try_get<LODComponent>(me); lod && !lod->Levels.empty()) {
                float dist = glm::distance(glm::vec3(world[3]), camPos) * lod->DistanceMultiplier;
                int idx = 0;
                for (int i = 0; i < (int)lod->Levels.size(); ++i) {
                    if (dist >= lod->Levels[i].Distance) idx = i;
                    else break;
                }
                if (idx < (int)lod->Levels.size() && lod->Levels[idx].MeshAsset)
                    mesh = lod->Levels[idx].MeshAsset;
            }

            // Terreno: mesh de heightmap gerado sob demanda.
            if (auto* terr = m_Registry.try_get<TerrainComponent>(me); terr) {
                if (!terr->GeneratedMesh) terr->Regenerate();
                if (terr->GeneratedMesh) mesh = terr->GeneratedMesh;
            }
            Renderer3D::Submit(mesh, mr.MeshMaterial, world, mr.LightmapTexture);
        }
    };

    if (overrideCamera) {
        Renderer3D::BeginScene(*overrideCamera);
        SubmitLights();
        SubmitParticleSystems();
        Renderer3D::DrawGrid();
        SubmitMeshes();
        Renderer3D::EndScene();
        return;
    }

    auto camView = m_Registry.view<TransformComponent, CameraComponent>();
    for (auto e : camView) {
        auto& camera = camView.get<CameraComponent>(e);
        if (!camera.Primary || camera.Type != CameraComponent::ProjectionType::Perspective3D) continue;
        if (!IsEntityActive(Entity{ e, this })) continue; // câmera inativa não renderiza

        // Posição pelo transform MUNDIAL (respeita pai). Orientação pelo
        // euler LOCAL do TransformComponent — NÃO decompor a matriz composta
        // (gimbal lock: em yaw = ±90° o glm::eulerAngles devolve pitch ±180°,
        // virando a câmera de cabeça pra baixo/trás no Play — era o bug da
        // cena de demonstração sumir ao apertar Play).
        glm::vec3 pos = glm::vec3(GetWorldTransform(Entity{ e, this })[3]);
        const auto& tc = camView.get<TransformComponent>(e);

        float aspect = m_ViewportHeight ? (float)m_ViewportWidth / (float)m_ViewportHeight : 16.0f / 9.0f;
        PerspectiveCamera cam(camera.PerspectiveFOV, aspect, camera.NearClip, camera.FarClip);
        cam.SetPosition(pos);
        cam.SetRotation(glm::degrees(tc.Rotation.y), glm::degrees(tc.Rotation.x));

        Renderer3D::BeginScene(cam);
        SubmitLights();
        SubmitParticleSystems();
        SubmitMeshes();
        Renderer3D::EndScene();
        return;
    }
    // Sem câmera primária de perspectiva é NORMAL em cena 2D (só ortho).
    // Só reclama se a cena tem meshes 3D de verdade e mesmo assim nenhuma
    // câmera — aí o Play não renderiza nada (uma vez por cena).
    if (m_Registry.view<MeshRendererComponent>().begin() != m_Registry.view<MeshRendererComponent>().end()) {
        static Scene* s_WarnedFor = nullptr;
        if (s_WarnedFor != this) {
            s_WarnedFor = this;
            KZ_CORE_WARN("RenderScene3D: há meshes 3D mas nenhuma câmera primária (Perspective3D) — render 3D vazio.");
        }
    }
}

} // namespace kizuri
