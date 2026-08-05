#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
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
#include <glm/gtc/random.hpp>
#include <algorithm>
#include <limits>
#include <cmath>
#include <cstring>

namespace kizuri {

namespace {

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

void Scene::DestroyEntity(Entity entity) {
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

    btSphereShape sphere(radius);
    btPairCachingGhostObject ghost;
    ghost.setCollisionShape(&sphere);
    btTransform tr;
    tr.setIdentity();
    tr.setOrigin(btVector3(center.x, center.y, center.z));
    ghost.setWorldTransform(tr);
    ghost.setCollisionFlags(btCollisionObject::CF_NO_CONTACT_RESPONSE);
    m_PhysicsWorld3D->addCollisionObject(&ghost, btBroadphaseProxy::SensorTrigger);

    OverlapSphereCallback cb;
    m_PhysicsWorld3D->contactTest(&ghost, cb);
    m_PhysicsWorld3D->removeCollisionObject(&ghost);

    if (cb.Best == entt::null) return false;
    outEntity = Entity{ cb.Best, this };
    return true;
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
        auto& cv = canvases.get<UICanvasComponent>(ce);
        glm::vec2 uiPoint{ m_UIMouseNDC.x * cv.OrthoSize * aspect, m_UIMouseNDC.y * cv.OrthoSize };

        std::vector<entt::entity> stack;
        CollectUIChildren(ce, stack);
        while (!stack.empty()) {
            entt::entity e = stack.back();
            stack.pop_back();
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
        auto& cv = canvases.get<UICanvasComponent>(ce);
        OrthographicCamera uiCam(-cv.OrthoSize * aspect, cv.OrthoSize * aspect, -cv.OrthoSize, cv.OrthoSize);
        Renderer2D::BeginScene(uiCam);

        std::vector<entt::entity> stack;
        CollectUIChildren(ce, stack);
        while (!stack.empty()) {
            entt::entity e = stack.back();
            stack.pop_back();
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

Entity Scene::GetEntityByUUID(UUID id) {
    auto it = m_EntityMap.find(id);
    if (it == m_EntityMap.end()) return {};
    return Entity{ it->second, this };
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
            }
        }
    }
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
    body->setUserPointer(reinterpret_cast<void*>(static_cast<uintptr_t>(static_cast<uint32_t>(entity.GetHandle()))));

    m_PhysicsWorld3D->addRigidBody(body);
    rb3d.RuntimeBody = body;
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
    m_ActiveContacts3D.clear();

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
    UpdateUIPointer(); // hit-test dos UIButton antes dos scripts (que leem WasClicked)

    m_Registry.view<NativeScriptComponent>().each([=](auto entityHandle, auto& nsc) {
        (void)entityHandle;
        if (nsc.Instance) nsc.Instance->OnUpdate(ts);
    });

    UpdatePhysics2D(ts);
    UpdatePhysics3D(ts);
    FlushCollisionEvents();

    UpdateParticleSystems(ts);
    UpdateSpriteAnimations(ts);
    UpdateAnimators(ts);
    UpdateAudio(ts);

    // 3D primeiro, depois 2D por cima, UI por último. O passe 3D termina num
    // composite de tela cheia — se o 2D rodasse antes (ordem antiga), o
    // composite pintava por cima e engolia o 2D. Com 3D→2D→UI, uma cena
    // híbrida (câmera perspectiva + câmera ortográfica primárias) vira o
    // clássico 2.5D: fundo/mundo 3D + camada de jogo 2D + HUD de UI.
    RenderScene3D(nullptr);
    RenderScene2D(nullptr);
    RenderUI();
}

void Scene::OnUpdateEditor3D(Timestep ts, PerspectiveCamera& editorCamera) {
    KZ_TRACE_SCOPE("Scene::OnUpdateEditor3D");
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
        switch (it.priority) {
        case 0: {
            auto& sprite = m_Registry.get<SpriteRendererComponent>(it.entity);
            glm::mat4 worldTransform = GetWorldTransform(e);
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
    m_Registry.view<AnimatorComponent>().each([=](auto, auto& ac) {
        // Skin carregada sob demanda (a 1ª vez em que a entidade existe).
        if (!ac.Skin) {
            if (ac.MeshPath.empty()) return;
            ac.Skin = SkinData::CreateFromGLTF(Project::ResolvePath(ac.MeshPath));
            if (!ac.Skin) return;
        }
        if (ac.Skin->Joints.empty()) return; // arquivo sem skin
        if (!ac.Playing || ac.ClipName.empty()) return;

        ac.Time += (float)ts * ac.Speed;
        if (ac.Loop) {
            float dur = ac.Skin->GetClipDuration(ac.ClipName);
            if (dur > 0.0001f) ac.Time = fmodf(ac.Time, dur);
        }
    });
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
        l.CastsShadow = lc.CastsShadow;
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
            ac.Handle = AudioEngine::LoadSound(Project::ResolvePath(ac.ClipPath), ac.ClipPath, false);
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

    auto SubmitMeshes = [&]() {
        auto meshes = m_Registry.view<TransformComponent, MeshRendererComponent>();
        for (auto me : meshes) {
            auto& mr = meshes.get<MeshRendererComponent>(me);
            if (!mr.MeshAsset) continue;
            glm::mat4 world = GetWorldTransform(Entity{ me, this });
            auto* animator = m_Registry.try_get<AnimatorComponent>(me);
            if (animator && animator->Skin && !animator->Skin->Joints.empty()) {
                glm::mat4 joints[kMaxSkinJoints];
                if (animator->Skin->Evaluate(animator->ClipName, animator->Time, joints, kMaxSkinJoints))
                    Renderer3D::SubmitSkinned(mr.MeshAsset, mr.MeshMaterial, world, joints,
                                              (uint32_t)animator->Skin->Joints.size());
                else
                    Renderer3D::Submit(mr.MeshAsset, mr.MeshMaterial, world);
            } else {
                Renderer3D::Submit(mr.MeshAsset, mr.MeshMaterial, world);
            }
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
