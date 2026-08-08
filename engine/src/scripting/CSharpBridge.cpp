// CSharpBridge.cpp — ponte ABI C exportada pela engine para o lado managed
// (Kizuri.Scripting). É o ÚNICO ponto onde o assembly .NET toca a engine.
// Aqui dentro TODOS os includes glm/entt/box2d são permitidos — mas não em
// CSharpBridge.h, que só tem PODs.
#include "kizuri/scripting/CSharpBridge.h"
#include "kizuri/scripting/CSharpBridgeInternal.h"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/Input.hpp"
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/project/Project.hpp"
#include <btBulletDynamicsCommon.h>
#include <box2d/box2d.h>

#include <unordered_map>
#include <cstring>

namespace {

kizuri::Scene* s_ActiveScene = nullptr;
double s_DeltaSeconds = 0.0;
float s_TimeScale = 1.0f;
double s_Elapsed = 0.0;        // tempo acumulado (escalado por TimeScale)
double s_UnscaledElapsed = 0.0;

// handle opaco (uint32) -> UUID estável da entidade. Evita expor entt::entity
// ao C#; o UUID também sobrevive se a entidade for recriada.
uint32_t s_NextHandle = 1;
std::unordered_map<uint32_t, kizuri::UUID> s_Handles;
std::unordered_map<kizuri::UUID, uint32_t> s_HandlesByUUID;
uint64_t s_FrameCount = 0;

kizuri::Entity Resolve(uint32_t handle) {
    if (s_ActiveScene == nullptr) return {};
    auto it = s_Handles.find(handle);
    if (it == s_Handles.end()) return {};
    return s_ActiveScene->GetEntityByUUID(it->second);
}

} // namespace

namespace kizuri {
namespace scripting {

uint32_t RegisterEntityHandle(Entity entity) {
    if (!entity) return 0;
    kizuri::UUID id = entity.GetUUID();
    auto it = s_HandlesByUUID.find(id);
    if (it != s_HandlesByUUID.end()) return it->second;
    uint32_t handle = s_NextHandle++;
    s_Handles[handle] = id;
    s_HandlesByUUID[id] = handle;
    return handle;
}

float GetTimeScale() { return s_TimeScale; }
void SetTimeScale(float scale) { s_TimeScale = scale > 0.0f ? scale : 0.0f; }

} // namespace scripting
} // namespace kizuri

extern "C" {

// ---------------------------------------------------------------------------
// Host control
// ---------------------------------------------------------------------------
KZ_SCRIPT_API void kz_set_active_scene(void* scene) {
    s_ActiveScene = static_cast<kizuri::Scene*>(scene);
    s_Handles.clear();
    s_HandlesByUUID.clear();
    s_NextHandle = 1;
}

KZ_SCRIPT_API void kz_set_time_delta(double seconds) {
    s_DeltaSeconds = seconds;
    s_Elapsed += seconds * s_TimeScale;
    s_UnscaledElapsed += seconds;
    ++s_FrameCount;
}

KZ_SCRIPT_API double kz_time_get_time() { return s_Elapsed; }
KZ_SCRIPT_API uint64_t kz_time_get_frame() { return s_FrameCount; }
KZ_SCRIPT_API double kz_time_get_unscaled_time() { return s_UnscaledElapsed; }

// ---------------------------------------------------------------------------
// Log — level spdlog (0=trace..5=critical); canal 0 = Core, 1 = App.
// ---------------------------------------------------------------------------
KZ_SCRIPT_API void kz_log(int channel, int level, const char* message) {
    // Niveis espelham spdlog: 0=trace,1=debug,2=info,3=warn,4=error,5=critical.
    auto& logger = (channel == 0) ? *kizuri::Log::Core() : *kizuri::Log::AppLog();
    switch (level) {
        case 0: logger.trace("{}", message);  return;
        case 1: logger.debug("{}", message);  return;
        case 2: logger.info("{}", message);   return;
        case 3: logger.warn("{}", message);   return;
        case 4: logger.error("{}", message);  return;
        default: logger.critical("{}", message); return;
    }
}

// ---------------------------------------------------------------------------
// Time
// ---------------------------------------------------------------------------
KZ_SCRIPT_API double kz_time_delta_seconds() {
    return s_DeltaSeconds;
}

KZ_SCRIPT_API void kz_set_time_scale(float scale) {
    kizuri::scripting::SetTimeScale(scale);
}

KZ_SCRIPT_API float kz_get_time_scale() {
    return kizuri::scripting::GetTimeScale();
}

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_input_is_key_pressed(int key) {
    return kizuri::Input::IsKeyPressed(key) ? 1 : 0;
}

KZ_SCRIPT_API int kz_input_is_mouse_button_pressed(int button) {
    return kizuri::Input::IsMouseButtonPressed(button) ? 1 : 0;
}

// Edge-detect de mouse (GetMouseButtonDown): true só no frame do clique.
std::unordered_map<int, bool>& PrevMouseButtonState() {
    static std::unordered_map<int, bool> s_Prev;
    return s_Prev;
}
KZ_SCRIPT_API int kz_input_is_mouse_button_down(int button) {
    bool pressed = kizuri::Input::IsMouseButtonPressed(button);
    bool& prev = PrevMouseButtonState()[button];
    bool down = pressed && !prev;
    prev = pressed;
    return down ? 1 : 0;
}

// Edge-detect de tecla (GetKeyDown): true só no frame em que a tecla foi
// pressionada. Mantém o estado anterior por tecla consultada — scripts que
// consultam todo frame funcionam normalmente.
std::unordered_map<int, bool>& PrevKeyState() {
    static std::unordered_map<int, bool> s_Prev;
    return s_Prev;
}
KZ_SCRIPT_API int kz_input_is_key_down(int key) {
    bool pressed = kizuri::Input::IsKeyPressed(key);
    bool& prev = PrevKeyState()[key];
    bool down = pressed && !prev;
    prev = pressed;
    return down ? 1 : 0;
}

KZ_SCRIPT_API void kz_input_get_mouse_position(float* outX, float* outY) {
    auto [x, y] = kizuri::Input::GetMousePosition();
    if (outX) *outX = x;
    if (outY) *outY = y;
}

// ---- Input Actions (nome -> tecla, com rebind) ----------------------------
KZ_SCRIPT_API int kz_input_is_action_pressed(const char* action) {
    return action ? (kizuri::Input::IsActionPressed(action) ? 1 : 0) : 0;
}
KZ_SCRIPT_API void kz_input_set_action_key(const char* action, int key) {
    if (action) kizuri::Input::SetActionKey(action, key);
}
KZ_SCRIPT_API int kz_input_get_action_key(const char* action) {
    return action ? kizuri::Input::GetActionKey(action) : -1;
}

// ---------------------------------------------------------------------------
// Entities / scene
// ---------------------------------------------------------------------------
KZ_SCRIPT_API uint32_t kz_scene_create_entity(const char* name) {
    if (s_ActiveScene == nullptr) return 0;
    auto entity = s_ActiveScene->CreateEntity(name != nullptr ? name : "");
    if (!entity) return 0;
    return kizuri::scripting::RegisterEntityHandle(entity);
}

KZ_SCRIPT_API void kz_scene_destroy_entity(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return;
    auto it = s_Handles.find(entity);
    if (it != s_Handles.end()) s_HandlesByUUID.erase(it->second);
    s_Handles.erase(entity);
    s_ActiveScene->DestroyEntity(e);
}

KZ_SCRIPT_API int kz_entity_has_component(uint32_t entity, int componentType) {
    auto e = Resolve(entity);
    if (!e) return 0;
    switch (componentType) {
        case 0: return e.HasComponent<kizuri::TransformComponent>() ? 1 : 0;
        case 1: return e.HasComponent<kizuri::Rigidbody2DComponent>() ? 1 : 0;
        case 2: return e.HasComponent<kizuri::SpriteRendererComponent>() ? 1 : 0;
        case 3: return e.HasComponent<kizuri::TextComponent>() ? 1 : 0;
        case 4: return e.HasComponent<kizuri::AudioSourceComponent>() ? 1 : 0;
        case 5: return e.HasComponent<kizuri::CameraComponent>() ? 1 : 0;
        case 6: return e.HasComponent<kizuri::LightComponent>() ? 1 : 0;
        case 7: return e.HasComponent<kizuri::UIRectComponent>() ? 1 : 0;
        case 8: return e.HasComponent<kizuri::UIButtonComponent>() ? 1 : 0;
        case 9: return e.HasComponent<kizuri::UICanvasComponent>() ? 1 : 0;
        case 10: return e.HasComponent<kizuri::CircleCollider2DComponent>() ? 1 : 0;
        case 11: return e.HasComponent<kizuri::MeshRendererComponent>() ? 1 : 0;
        case 12: return e.HasComponent<kizuri::ParticleSystemComponent>() ? 1 : 0;
        case 13: return e.HasComponent<kizuri::AnimatorComponent>() ? 1 : 0;
        case 14: return e.HasComponent<kizuri::Rigidbody3DComponent>() ? 1 : 0;
        case 15: return e.HasComponent<kizuri::BoxCollider3DComponent>() ? 1 : 0;
        case 16: return e.HasComponent<kizuri::SphereCollider3DComponent>() ? 1 : 0;
        default: return 0;
    }
}

KZ_SCRIPT_API void kz_transform_set_position(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return;
    auto& tc = e.GetComponent<kizuri::TransformComponent>();
    tc.Translation = glm::vec3(x, y, z);
}

KZ_SCRIPT_API void kz_entity_set_parent(uint32_t child, uint32_t parent) {
    auto c = Resolve(child);
    if (!c) return;
    kizuri::Entity p;
    if (parent != 0) {
        p = Resolve(parent);
        if (!p) return;
    }
    // p inválido (0) destaca o filho — ver Scene::SetParent.
    c.SetParent(p);
}

// ---------------------------------------------------------------------------
// Cena em runtime (instanciar prefab, trocar cena, câmera primária)
// ---------------------------------------------------------------------------
KZ_SCRIPT_API uint32_t kz_scene_instantiate_prefab(const char* path, float x, float y, float z) {
    if (s_ActiveScene == nullptr || path == nullptr) return 0;
    // Em runtime (Play) o Instantiate também cria corpos de física e dispara
    // OnCreate dos scripts da prefab — o caminho certo pra spawn de gameplay.
    kizuri::Entity entity = s_ActiveScene->Instantiate(kizuri::Project::ResolvePath(path), glm::vec3(x, y, z));
    if (!entity) return 0;
    return kizuri::scripting::RegisterEntityHandle(entity);
}

// Instancing de malhas: desenha a MESMA malha em N transformadas num único
// draw call (floresta, multidão). transformData = 16 floats por matriz
// (coluna-major, como o GL). Chamado todo frame dentro de OnUpdate.
KZ_SCRIPT_API void kz_scene_draw_instanced(const char* meshSource, float r, float g, float b,
                                          const float* transformData, int count) {
    if (s_ActiveScene == nullptr || meshSource == nullptr || transformData == nullptr || count <= 0) return;
    auto mesh = kizuri::Mesh::FromSource(kizuri::Project::ResolvePath(meshSource));
    if (!mesh) return;
    std::vector<glm::mat4> transforms((size_t)count);
    for (int i = 0; i < count; ++i)
        std::memcpy(&transforms[i], transformData + i * 16, sizeof(glm::mat4));
    kizuri::Material mat;
    mat.Albedo = { r, g, b };
    kizuri::Renderer3D::SubmitMeshInstances(mesh, mat, transforms.data(), (uint32_t)count);
}

KZ_SCRIPT_API uint32_t kz_scene_instantiate_prefab_rot(const char* path, float x, float y, float z,
                                                       float rx, float ry, float rz) {
    if (s_ActiveScene == nullptr || path == nullptr) return 0;
    kizuri::Entity entity = s_ActiveScene->Instantiate(kizuri::Project::ResolvePath(path), glm::vec3(x, y, z));
    if (!entity) return 0;
    if (entity.HasComponent<kizuri::TransformComponent>())
        entity.GetComponent<kizuri::TransformComponent>().Rotation = { rx, ry, rz };
    return kizuri::scripting::RegisterEntityHandle(entity);
}

KZ_SCRIPT_API void kz_scene_request_load(const char* path) {
    if (s_ActiveScene == nullptr || path == nullptr) return;
    // Pedido diferido — o host troca a cena no fim do frame (PollPendingLoad).
    s_ActiveScene->RequestLoad(kizuri::Project::ResolvePath(path));
}

KZ_SCRIPT_API uint32_t kz_scene_get_primary_camera() {
    if (s_ActiveScene == nullptr) return 0;
    auto view = s_ActiveScene->GetRegistry().view<kizuri::TransformComponent, kizuri::CameraComponent>();
    for (auto e : view) {
        if (view.get<kizuri::CameraComponent>(e).Primary) {
            return kizuri::scripting::RegisterEntityHandle(kizuri::Entity{ e, s_ActiveScene });
        }
    }
    return 0;
}

KZ_SCRIPT_API uint32_t kz_scene_find_entity(const char* name) {
    if (s_ActiveScene == nullptr || name == nullptr) return 0;
    auto view = s_ActiveScene->GetRegistry().view<kizuri::TagComponent>();
    for (auto e : view) {
        if (view.get<kizuri::TagComponent>(e).Tag == name)
            return kizuri::scripting::RegisterEntityHandle(kizuri::Entity{ e, s_ActiveScene });
    }
    return 0;
}

// Quantas entidades têm o mesmo Tag (pré-consulta pra alocar o buffer).
KZ_SCRIPT_API int kz_scene_count_entities_with_tag(const char* tag) {
    if (s_ActiveScene == nullptr || tag == nullptr) return 0;
    int count = 0;
    auto view = s_ActiveScene->GetRegistry().view<kizuri::TagComponent>();
    for (auto e : view) {
        if (view.get<kizuri::TagComponent>(e).Tag == tag) ++count;
    }
    return count;
}

// Preenche 'outHandles' (até maxCount) com as entidades que têm o Tag dado.
// Devolve quantas foram escritas. 'outHandles' pode ser null quando maxCount==0.
KZ_SCRIPT_API int kz_scene_get_entities_with_tag(const char* tag, uint32_t* outHandles, int maxCount) {
    if (s_ActiveScene == nullptr || tag == nullptr || maxCount <= 0) return 0;
    int written = 0;
    auto view = s_ActiveScene->GetRegistry().view<kizuri::TagComponent>();
    for (auto e : view) {
        if (view.get<kizuri::TagComponent>(e).Tag != tag) continue;
        if (outHandles != nullptr) {
            outHandles[written] = kizuri::scripting::RegisterEntityHandle(kizuri::Entity{ e, s_ActiveScene });
        }
        ++written;
        if (written >= maxCount) break;
    }
    return written;
}

// Posição LOCAL (Translation do TransformComponent) da entidade.
KZ_SCRIPT_API int kz_entity_get_position(uint32_t entity, float* outXYZ) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return 0;
    const auto& tc = e.GetComponent<kizuri::TransformComponent>();
    if (outXYZ != nullptr) {
        outXYZ[0] = tc.Translation.x;
        outXYZ[1] = tc.Translation.y;
        outXYZ[2] = tc.Translation.z;
    }
    return 1;
}

// Pai (handle) da entidade; devolve 0 se for raiz.
KZ_SCRIPT_API int kz_entity_get_parent(uint32_t entity, uint32_t* outParent) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::RelationshipComponent>()) return 0;
    kizuri::UUID parent = e.GetComponent<kizuri::RelationshipComponent>().Parent;
    if (!parent) return 0;
    kizuri::Entity p = s_ActiveScene->GetEntityByUUID(parent);
    if (!p) return 0;
    if (outParent != nullptr) *outParent = kizuri::scripting::RegisterEntityHandle(p);
    return 1;
}

// Quantos filhos diretos a entidade tem.
KZ_SCRIPT_API int kz_entity_get_child_count(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::RelationshipComponent>()) return 0;
    return (int)e.GetComponent<kizuri::RelationshipComponent>().Children.size();
}

// Filho no índice dado; devolve 0 se o índice for inválido.
KZ_SCRIPT_API int kz_entity_get_child(uint32_t entity, int index, uint32_t* outChild) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::RelationshipComponent>() || index < 0) return 0;
    const auto& rel = e.GetComponent<kizuri::RelationshipComponent>();
    if ((size_t)index >= rel.Children.size()) return 0;
    kizuri::Entity child = s_ActiveScene->GetEntityByUUID(rel.Children[(size_t)index]);
    if (!child) return 0;
    if (outChild != nullptr) *outChild = kizuri::scripting::RegisterEntityHandle(child);
    return 1;
}

// Liga/desliga a entidade (estilo GameObject.SetActive). Inativa não é
// desenhada nem atualizada; os filhos herdam o estado.
KZ_SCRIPT_API void kz_entity_set_active(uint32_t entity, int active) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::IDComponent>()) return;
    e.GetComponent<kizuri::IDComponent>().Active = (active != 0);
}

// True se a entidade está ativa NA PRÁTICA (ela e todos os ancestrais).
KZ_SCRIPT_API int kz_entity_is_active(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    return s_ActiveScene->IsEntityActive(e) ? 1 : 0;
}

KZ_SCRIPT_API uint32_t kz_scene_duplicate_entity(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    kizuri::Entity dup = s_ActiveScene->DuplicateEntity(e);
    if (!dup) return 0;
    return kizuri::scripting::RegisterEntityHandle(dup);
}

// ---------------------------------------------------------------------------
// Adicionar componentes em runtime
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_sprite(uint32_t entity, const char* texturePath) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& sc = e.AddOrReplaceComponent<kizuri::SpriteRendererComponent>();
    if (texturePath != nullptr && *texturePath != '\0') {
        sc.TexturePath = texturePath;
        sc.Texture = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(texturePath));
    } else {
        sc.TexturePath.clear();
        sc.Texture = nullptr;
    }
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_text(uint32_t entity, const char* text, float fontSize) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& tc = e.AddOrReplaceComponent<kizuri::TextComponent>();
    tc.Text = text != nullptr ? text : "";
    tc.FontSize = fontSize;
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_audio(uint32_t entity, const char* clipPath, int loop, int playOnStart) {
    auto e = Resolve(entity);
    if (!e || clipPath == nullptr) return 0;
    auto& ac = e.AddOrReplaceComponent<kizuri::AudioSourceComponent>();
    ac.ClipPath = clipPath;
    ac.Loop = loop != 0;
    ac.PlayOnStart = playOnStart != 0;
    if (ac.PlayOnStart) ac.Play();
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_camera(uint32_t entity, int projectionType) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& cc = e.AddOrReplaceComponent<kizuri::CameraComponent>();
    cc.Type = (projectionType == 1) ? kizuri::CameraComponent::ProjectionType::Perspective3D
                                    : kizuri::CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true;
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_camera_follow(uint32_t entity, const char* targetName) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& cf = e.AddOrReplaceComponent<kizuri::CameraFollowComponent>();
    cf.TargetName = (targetName != nullptr) ? targetName : "";
    cf.m_HasStart = false;
    return 1;
}

KZ_SCRIPT_API void kz_camera_follow_set_target(uint32_t entity, const char* targetName) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::CameraFollowComponent>()) return;
    e.GetComponent<kizuri::CameraFollowComponent>().TargetName = (targetName != nullptr) ? targetName : "";
    e.GetComponent<kizuri::CameraFollowComponent>().m_HasStart = false;
}

KZ_SCRIPT_API void kz_camera_follow_set_offset(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::CameraFollowComponent>()) return;
    e.GetComponent<kizuri::CameraFollowComponent>().Offset = { x, y, z };
}

KZ_SCRIPT_API void kz_camera_follow_set_smoothness(uint32_t entity, float smoothness) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::CameraFollowComponent>()) return;
    e.GetComponent<kizuri::CameraFollowComponent>().Smoothness = smoothness;
}

KZ_SCRIPT_API int kz_entity_add_circle_collider2d(uint32_t entity, float radius,
                                                  float density, float friction, float restitution) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& cc = e.AddOrReplaceComponent<kizuri::CircleCollider2DComponent>();
    cc.Radius = radius;
    cc.Density = density;
    cc.Friction = friction;
    cc.Restitution = restitution;
    return 1;
}

KZ_SCRIPT_API void kz_entity_set_sorting_layer(uint32_t entity, int layer) {
    auto e = Resolve(entity);
    if (!e) return;
    // Aplica a camada de ordenação 2D a qualquer componente 2D que a
    // entidade tenha (sprite/círculo/texto/animacão/tilemap).
    auto* sprite = s_ActiveScene->GetRegistry().try_get<kizuri::SpriteRendererComponent>(e.GetHandle());
    if (sprite) { sprite->SortingLayer = layer; return; }
    auto* circle = s_ActiveScene->GetRegistry().try_get<kizuri::CircleRendererComponent>(e.GetHandle());
    if (circle) { circle->SortingLayer = layer; return; }
    auto* text = s_ActiveScene->GetRegistry().try_get<kizuri::TextComponent>(e.GetHandle());
    if (text) { text->SortingLayer = layer; return; }
    auto* anim = s_ActiveScene->GetRegistry().try_get<kizuri::SpriteAnimationComponent>(e.GetHandle());
    if (anim) { anim->SortingLayer = layer; return; }
    auto* tilemap = s_ActiveScene->GetRegistry().try_get<kizuri::TilemapComponent>(e.GetHandle());
    if (tilemap) { tilemap->SortingLayer = layer; return; }
}

// ---------------------------------------------------------------------------
// Mutação de componentes em runtime (sprites, texto)
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_sprite_set_texture(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::SpriteRendererComponent>() || path == nullptr) return 0;
    auto& sc = e.GetComponent<kizuri::SpriteRendererComponent>();
    sc.TexturePath = path;
    sc.Texture = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

KZ_SCRIPT_API int kz_sprite_set_color(uint32_t entity, float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::SpriteRendererComponent>()) return 0;
    e.GetComponent<kizuri::SpriteRendererComponent>().Color = { r, g, b, a };
    return 1;
}

KZ_SCRIPT_API void kz_sprite_set_flip(uint32_t entity, int flipX, int flipY) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::SpriteRendererComponent>()) return;
    auto& sc = e.GetComponent<kizuri::SpriteRendererComponent>();
    sc.FlipX = flipX != 0;
    sc.FlipY = flipY != 0;
}

KZ_SCRIPT_API void kz_rigidbody2d_set_gravity_scale(uint32_t entity, float scale) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    auto& rb = e.GetComponent<kizuri::Rigidbody2DComponent>();
    rb.GravityScale = scale;
    if (rb.RuntimeBody) static_cast<b2Body*>(rb.RuntimeBody)->SetGravityScale(scale);
}

KZ_SCRIPT_API int kz_text_set_content(uint32_t entity, const char* text) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TextComponent>() || text == nullptr) return 0;
    e.GetComponent<kizuri::TextComponent>().Text = text;
    return 1;
}

KZ_SCRIPT_API int kz_text_set_size(uint32_t entity, float size) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TextComponent>()) return 0;
    e.GetComponent<kizuri::TextComponent>().FontSize = size;
    return 1;
}

KZ_SCRIPT_API int kz_text_set_color(uint32_t entity, float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TextComponent>()) return 0;
    e.GetComponent<kizuri::TextComponent>().Color = { r, g, b, a };
    return 1;
}

// ---------------------------------------------------------------------------
// UI — Canvas/Rect/Botão/Texto em espaço de tela (o Scene renderiza e faz o
// hit-test dos botões; os setters aqui só mutam os componentes).
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_ui_canvas(uint32_t entity, float orthoSize) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& uc = e.AddOrReplaceComponent<kizuri::UICanvasComponent>();
    uc.OrthoSize = orthoSize > 0.0f ? orthoSize : 10.0f;
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_ui_rect(uint32_t entity, float x, float y, float w, float h,
                                        float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& ur = e.AddOrReplaceComponent<kizuri::UIRectComponent>();
    ur.Position = { x, y };
    ur.Size = { w, h };
    ur.Color = { r, g, b, a };
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_ui_button(uint32_t entity, float x, float y, float w, float h,
                                          float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& ur = e.AddOrReplaceComponent<kizuri::UIRectComponent>();
    ur.Position = { x, y };
    ur.Size = { w, h };
    ur.Color = { r, g, b, a };
    e.AddOrReplaceComponent<kizuri::UIButtonComponent>();
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_ui_text(uint32_t entity, const char* text, float fontSize,
                                        float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e) return 0;
    // Só garante um UIRect se não houver (entidade só de texto). Se a
    // entidade já é um botão/rect (texto sobre o fundo), NÃO mexe no rect
    // — o texto desenha centrado na posição dele.
    if (!e.HasComponent<kizuri::UIRectComponent>()) {
        auto& ur = e.AddComponent<kizuri::UIRectComponent>();
        ur.Size = { 0.0f, 0.0f };          // sem quad de fundo
        ur.Color = { 0.0f, 0.0f, 0.0f, 0.0f };
    }
    auto& tc = e.AddOrReplaceComponent<kizuri::TextComponent>();
    tc.Text = text != nullptr ? text : "";
    tc.FontSize = fontSize;
    tc.Color = { r, g, b, a };
    return 1;
}

KZ_SCRIPT_API int kz_ui_button_was_clicked(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto* btn = s_ActiveScene->GetRegistry().try_get<kizuri::UIButtonComponent>(e.GetHandle());
    return (btn && btn->WasClicked) ? 1 : 0;
}

KZ_SCRIPT_API int kz_ui_button_is_hovered(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto* btn = s_ActiveScene->GetRegistry().try_get<kizuri::UIButtonComponent>(e.GetHandle());
    return (btn && btn->Hovered) ? 1 : 0;
}

KZ_SCRIPT_API void kz_ui_set_rect(uint32_t entity, float x, float y, float w, float h) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::UIRectComponent>()) return;
    auto& ur = e.GetComponent<kizuri::UIRectComponent>();
    ur.Position = { x, y };
    ur.Size = { w, h };
}

KZ_SCRIPT_API void kz_ui_set_color(uint32_t entity, float r, float g, float b, float a) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::UIRectComponent>()) return;
    e.GetComponent<kizuri::UIRectComponent>().Color = { r, g, b, a };
}

// ---------------------------------------------------------------------------
// Áudio
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_audio_play(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AudioSourceComponent>()) return 0;
    e.GetComponent<kizuri::AudioSourceComponent>().Play();
    return 1;
}

KZ_SCRIPT_API int kz_audio_stop(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AudioSourceComponent>()) return 0;
    e.GetComponent<kizuri::AudioSourceComponent>().Stop();
    return 1;
}

KZ_SCRIPT_API void kz_audio_play_one_shot(const char* path, float volume) {
    if (path == nullptr) return;
    kizuri::AudioEngine::PlayOneShot(kizuri::Project::ResolvePath(path), volume);
}

KZ_SCRIPT_API void kz_audio_play_one_shot_at(const char* path, float volume, float x, float y, float z) {
    if (path == nullptr) return;
    kizuri::AudioEngine::PlayOneShotAt(kizuri::Project::ResolvePath(path), volume, { x, y, z });
}

KZ_SCRIPT_API void kz_audio_stop_all() {
    kizuri::AudioEngine::StopAll();
}

KZ_SCRIPT_API void kz_audio_set_master_volume(float volume) {
    kizuri::AudioEngine::SetMasterVolume(volume);
}

// Audio mixer: grupos 0=SFX, 1=Música, 2=UI.
KZ_SCRIPT_API void kz_audio_set_group_volume(int group, float volume) {
    kizuri::AudioEngine::SetGroupVolume(group, volume);
}
KZ_SCRIPT_API float kz_audio_get_group_volume(int group) {
    return kizuri::AudioEngine::GetGroupVolume(group);
}

KZ_SCRIPT_API int kz_entity_get_transform(uint32_t entity, float* outPosition, float* outRotation, float* outScale) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return 0;
    const auto& tc = e.GetComponent<kizuri::TransformComponent>();
    if (outPosition) { outPosition[0] = tc.Translation.x; outPosition[1] = tc.Translation.y; outPosition[2] = tc.Translation.z; }
    if (outRotation) { outRotation[0] = tc.Rotation.x;    outRotation[1] = tc.Rotation.y;    outRotation[2] = tc.Rotation.z; }
    if (outScale)    { outScale[0]    = tc.Scale.x;       outScale[1]    = tc.Scale.y;       outScale[2]    = tc.Scale.z; }
    return 1;
}

// ---------------------------------------------------------------------------
// Física 2D — note: os corpos só existem durante Play (host chamaria
// CreateEntity apenas no runtime). Fora do Play, os setters são no-op
// (mesmo comportamento do C++ NativeScript atual).
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_get_rigidbody2d(uint32_t entity, int* bodyType, float* outVelXY) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return 0;
    auto& rb = e.GetComponent<kizuri::Rigidbody2DComponent>();
    if (bodyType) *bodyType = (int)rb.Type;
    auto vel = rb.GetLinearVelocity();
    if (outVelXY) { outVelXY[0] = vel.x; outVelXY[1] = vel.y; }
    return 1;
}

KZ_SCRIPT_API void kz_rigidbody2d_set_linear_velocity(uint32_t entity, float vx, float vy) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().SetLinearVelocity({ vx, vy });
}

KZ_SCRIPT_API void kz_rigidbody2d_apply_linear_impulse(uint32_t entity, float ix, float iy, int wake) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().ApplyLinearImpulse({ ix, iy }, wake != 0);
}

KZ_SCRIPT_API void kz_rigidbody2d_set_transform(uint32_t entity, float x, float y, float angle) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().SetTransform({ x, y }, angle);
}

KZ_SCRIPT_API int kz_physics2d_raycast(float x0, float y0, float x1, float y1,
                                       float* outHitX, float* outHitY, uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    glm::vec2 point;
    float fraction = 1.0f;
    if (!s_ActiveScene->Raycast2D({ x0, y0 }, { x1, y1 }, hit, point, fraction)) return 0;
    if (outHitX) *outHitX = point.x;
    if (outHitY) *outHitY = point.y;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

KZ_SCRIPT_API int kz_physics2d_overlap_circle(float x, float y, float radius,
                                              uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    if (!s_ActiveScene->OverlapCircle2D({ x, y }, radius, hit)) return 0;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

// ---------------------------------------------------------------------------
// Transform — rotação (euler, radianos) e escala em runtime
// ---------------------------------------------------------------------------
KZ_SCRIPT_API void kz_transform_set_rotation(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return;
    e.GetComponent<kizuri::TransformComponent>().Rotation = { x, y, z };
}

KZ_SCRIPT_API void kz_transform_set_scale(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return;
    e.GetComponent<kizuri::TransformComponent>().Scale = { x, y, z };
}

// ---------------------------------------------------------------------------
// Luz — cria/atualiza LightComponent em runtime (a direção da direcional/
// spot é derivada da rotação do Transform, igual ao editor)
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_light(uint32_t entity, int type, float r, float g, float b,
                                      float intensity, float range, float innerConeDeg, float outerConeDeg,
                                      int castsShadow) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& lc = e.AddOrReplaceComponent<kizuri::LightComponent>();
    lc.Type = (kizuri::LightType)type;
    lc.Color = { r, g, b };
    lc.Intensity = intensity;
    lc.Range = range;
    lc.InnerConeDeg = innerConeDeg;
    lc.OuterConeDeg = outerConeDeg;
    lc.CastsShadow = castsShadow != 0;
    return 1;
}

KZ_SCRIPT_API void kz_light_set_color(uint32_t entity, float r, float g, float b) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::LightComponent>()) return;
    e.GetComponent<kizuri::LightComponent>().Color = { r, g, b };
}

KZ_SCRIPT_API void kz_light_set_intensity(uint32_t entity, float intensity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::LightComponent>()) return;
    e.GetComponent<kizuri::LightComponent>().Intensity = intensity;
}

// ---------------------------------------------------------------------------
// Mesh renderer + material PBR em runtime
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_mesh_renderer(uint32_t entity, const char* meshSource) {
    auto e = Resolve(entity);
    if (!e) return 0;
    std::string src = (meshSource != nullptr && *meshSource != '\0') ? meshSource : "builtin:cube";
    auto& mc = e.AddOrReplaceComponent<kizuri::MeshRendererComponent>();
    mc.MeshSource = src;
    if (src.rfind("builtin:", 0) == 0) mc.MeshAsset = kizuri::Mesh::FromSource(src);
    else mc.MeshAsset = kizuri::Mesh::FromSource(kizuri::Project::ResolvePath(src));
    mc.MeshMaterial = {};
    return 1;
}

// Terreno procedural: adiciona TerrainComponent + MeshRenderer e gera o
// heightmap (Mesh::CreateTerrain). Regenera quando chamado de novo.
KZ_SCRIPT_API int kz_entity_add_terrain(uint32_t entity, uint32_t segments, float size, float heightScale, uint32_t seed) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& tc = e.AddOrReplaceComponent<kizuri::TerrainComponent>();
    tc.Segments = segments; tc.Size = size; tc.HeightScale = heightScale; tc.Seed = seed;
    tc.Regenerate();
    if (!e.HasComponent<kizuri::MeshRendererComponent>()) {
        auto& mc = e.AddComponent<kizuri::MeshRendererComponent>();
        mc.MeshSource = "builtin:plane";
        mc.MeshAsset = tc.GeneratedMesh;
        mc.MeshMaterial = {};
    } else {
        e.GetComponent<kizuri::MeshRendererComponent>().MeshAsset = tc.GeneratedMesh;
    }
    return 1;
}

// Regenera o heightmap de um terreno existente (muda a semente/formato).
KZ_SCRIPT_API int kz_terrain_regenerate(uint32_t entity, uint32_t segments, float size, float heightScale, uint32_t seed) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TerrainComponent>()) return 0;
    auto& tc = e.GetComponent<kizuri::TerrainComponent>();
    tc.Segments = segments; tc.Size = size; tc.HeightScale = heightScale; tc.Seed = seed;
    tc.Regenerate();
    if (e.HasComponent<kizuri::MeshRendererComponent>())
        e.GetComponent<kizuri::MeshRendererComponent>().MeshAsset = tc.GeneratedMesh;
    return 1;
}

KZ_SCRIPT_API void kz_material_set_albedo(uint32_t entity, float r, float g, float b) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>()) return;
    e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.Albedo = { r, g, b };
}

KZ_SCRIPT_API void kz_material_set_metallic(uint32_t entity, float metallic) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>()) return;
    e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.Metallic = metallic;
}

KZ_SCRIPT_API void kz_material_set_roughness(uint32_t entity, float roughness) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>()) return;
    e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.Roughness = roughness;
}

KZ_SCRIPT_API int kz_material_set_albedo_map(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>() || path == nullptr) return 0;
    auto& mat = e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
    mat.AlbedoMapPath = path;
    mat.AlbedoMap = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

KZ_SCRIPT_API int kz_material_set_normal_map(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>() || path == nullptr) return 0;
    auto& mat = e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
    mat.NormalMapPath = path;
    mat.NormalMap = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

KZ_SCRIPT_API int kz_material_set_metallic_roughness_map(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>() || path == nullptr) return 0;
    auto& mat = e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
    mat.MetallicRoughnessMapPath = path;
    mat.MetallicRoughnessMap = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

KZ_SCRIPT_API int kz_material_set_height_map(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>() || path == nullptr) return 0;
    auto& mat = e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
    mat.HeightMapPath = path;
    mat.HeightMap = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

KZ_SCRIPT_API void kz_material_set_height_scale(uint32_t entity, float scale) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>()) return;
    e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.HeightScale = scale;
}

// ---- Character controller (cinemático) ------------------------------------
KZ_SCRIPT_API void kz_entity_add_character_controller(uint32_t entity, float speed, float gravity) {
    auto e = Resolve(entity);
    if (!e) return;
    auto& cc = e.AddComponent<kizuri::CharacterControllerComponent>();
    cc.Speed = speed;
    cc.Gravity = gravity;
}
KZ_SCRIPT_API void kz_entity_move_character(uint32_t entity, float x, float z) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* cc = e.GetScene()->GetRegistry().try_get<kizuri::CharacterControllerComponent>(e.GetHandle()))
        cc->Input = { x, z };
}

// ---- Timeline (keyframes de transform / cutscene) -------------------------
KZ_SCRIPT_API void kz_entity_add_timeline(uint32_t entity) {
    auto e = Resolve(entity);
    if (e) e.AddComponent<kizuri::TimelineComponent>();
}
KZ_SCRIPT_API void kz_timeline_play(uint32_t entity, bool play) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* tl = e.GetScene()->GetRegistry().try_get<kizuri::TimelineComponent>(e.GetHandle()))
        tl->Playing = play;
}
KZ_SCRIPT_API void kz_timeline_set_time(uint32_t entity, float time) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* tl = e.GetScene()->GetRegistry().try_get<kizuri::TimelineComponent>(e.GetHandle()))
        tl->Time = time;
}
KZ_SCRIPT_API void kz_timeline_add_keyframe(uint32_t entity, float time, float px, float py, float pz) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* tl = e.GetScene()->GetRegistry().try_get<kizuri::TimelineComponent>(e.GetHandle())) {
        kizuri::TimelineComponent::Keyframe k;
        k.Time = time;
        k.Position = { px, py, pz };
        tl->Keyframes.push_back(k);
        std::sort(tl->Keyframes.begin(), tl->Keyframes.end(),
                  [](auto& a, auto& b) { return a.Time < b.Time; });
    }
}

// ---- Tags & Layers (filtro de colisão por camada) ------------------------
KZ_SCRIPT_API void kz_entity_set_layer(uint32_t entity, int layer) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TagComponent>()) return;
    e.GetComponent<kizuri::TagComponent>().Layer = layer;
}
KZ_SCRIPT_API int kz_entity_get_layer(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TagComponent>()) return 0;
    return e.GetComponent<kizuri::TagComponent>().Layer;
}
KZ_SCRIPT_API void kz_entity_set_collision_mask(uint32_t entity, uint32_t mask) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TagComponent>()) return;
    e.GetComponent<kizuri::TagComponent>().CollisionMask = mask;
}
KZ_SCRIPT_API uint32_t kz_entity_get_collision_mask(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TagComponent>()) return 0xFFFFFFFFu;
    return e.GetComponent<kizuri::TagComponent>().CollisionMask;
}

KZ_SCRIPT_API void kz_material_set_emissive(uint32_t entity, float r, float g, float b, float strength) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::MeshRendererComponent>()) return;
    auto& mat = e.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial;
    mat.Emissive = { r, g, b };
    mat.EmissiveStrength = strength;
}

// ---------------------------------------------------------------------------
// Câmera — parâmetros de perspectiva em runtime
// ---------------------------------------------------------------------------
KZ_SCRIPT_API void kz_camera_set_params(uint32_t entity, float fovDeg, float nearClip, float farClip) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::CameraComponent>()) return;
    auto& cc = e.GetComponent<kizuri::CameraComponent>();
    cc.PerspectiveFOV = fovDeg;
    cc.NearClip = nearClip;
    cc.FarClip = farClip;
}

// ---------------------------------------------------------------------------
// Animação esquelética (skinning via glTF)
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_animator(uint32_t entity, const char* meshPath) {
    auto e = Resolve(entity);
    if (!e || meshPath == nullptr) return 0;
    auto& ac = e.AddOrReplaceComponent<kizuri::AnimatorComponent>();
    ac.MeshPath = meshPath;
    ac.Skin = kizuri::SkinData::CreateFromGLTF(kizuri::Project::ResolvePath(meshPath));
    return ac.Skin ? 1 : 0;
}

KZ_SCRIPT_API int kz_animator_play(uint32_t entity, const char* clipName) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>() || clipName == nullptr) return 0;
    auto& ac = e.GetComponent<kizuri::AnimatorComponent>();
    if (!ac.Skin && !ac.MeshPath.empty())
        ac.Skin = kizuri::SkinData::CreateFromGLTF(kizuri::Project::ResolvePath(ac.MeshPath));
    ac.Play(clipName);
    return ac.ClipName == clipName ? 1 : 0;
}

KZ_SCRIPT_API float kz_animator_get_time(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>()) return 0.0f;
    return e.GetComponent<kizuri::AnimatorComponent>().Time;
}

KZ_SCRIPT_API void kz_animator_set_time(uint32_t entity, float time) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>()) return;
    e.GetComponent<kizuri::AnimatorComponent>().Time = time;
}

KZ_SCRIPT_API void kz_animator_set_speed(uint32_t entity, float speed) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>()) return;
    e.GetComponent<kizuri::AnimatorComponent>().Speed = speed;
}

KZ_SCRIPT_API void kz_animator_set_loop(uint32_t entity, int loop) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>()) return;
    e.GetComponent<kizuri::AnimatorComponent>().Loop = loop != 0;
}

KZ_SCRIPT_API void kz_animator_set_playing(uint32_t entity, int playing) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorComponent>()) return;
    e.GetComponent<kizuri::AnimatorComponent>().Playing = playing != 0;
}

// ---------------------------------------------------------------------------
// Física 3D (Bullet3) — os corpos existem só durante o Play; adicionar o
// componente em runtime é seguro (o UpdatePhysics3D registra sob demanda).
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_entity_add_rigidbody3d(uint32_t entity, int bodyType, float mass) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& rb = e.AddOrReplaceComponent<kizuri::Rigidbody3DComponent>();
    rb.Type = (kizuri::Rigidbody3DComponent::BodyType)bodyType;
    rb.Mass = mass;
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_box_collider3d(uint32_t entity, float hx, float hy, float hz) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& bc = e.AddOrReplaceComponent<kizuri::BoxCollider3DComponent>();
    bc.HalfExtents = { hx, hy, hz };
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_sphere_collider3d(uint32_t entity, float radius) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& sc = e.AddOrReplaceComponent<kizuri::SphereCollider3DComponent>();
    sc.Radius = radius;
    return 1;
}

KZ_SCRIPT_API int kz_rigidbody3d_apply_force(uint32_t entity, float fx, float fy, float fz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return 0;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return 0;
    body->applyCentralForce(btVector3(fx, fy, fz));
    body->activate();
    return 1;
}

KZ_SCRIPT_API int kz_rigidbody3d_apply_impulse(uint32_t entity, float ix, float iy, float iz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return 0;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return 0;
    body->applyCentralImpulse(btVector3(ix, iy, iz));
    body->activate();
    return 1;
}

KZ_SCRIPT_API int kz_rigidbody3d_get_linear_velocity(uint32_t entity, float* outVX, float* outVY, float* outVZ) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return 0;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return 0;
    const btVector3& v = body->getLinearVelocity();
    if (outVX) *outVX = v.x();
    if (outVY) *outVY = v.y();
    if (outVZ) *outVZ = v.z();
    return 1;
}

KZ_SCRIPT_API void kz_rigidbody3d_set_linear_velocity(uint32_t entity, float vx, float vy, float vz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return;
    body->setLinearVelocity(btVector3(vx, vy, vz));
    body->activate();
}

KZ_SCRIPT_API int kz_rigidbody3d_apply_torque(uint32_t entity, float tx, float ty, float tz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return 0;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return 0;
    body->applyTorque(btVector3(tx, ty, tz));
    body->activate();
    return 1;
}

KZ_SCRIPT_API int kz_rigidbody3d_get_angular_velocity(uint32_t entity, float* outWX, float* outWY, float* outWZ) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return 0;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return 0;
    const btVector3& w = body->getAngularVelocity();
    if (outWX) *outWX = w.x();
    if (outWY) *outWY = w.y();
    if (outWZ) *outWZ = w.z();
    return 1;
}

KZ_SCRIPT_API void kz_rigidbody3d_set_angular_velocity(uint32_t entity, float wx, float wy, float wz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody3DComponent>()) return;
    auto* body = static_cast<btRigidBody*>(e.GetComponent<kizuri::Rigidbody3DComponent>().RuntimeBody);
    if (!body) return;
    body->setAngularVelocity(btVector3(wx, wy, wz));
    body->activate();
}

// --- Queries 3D (Bullet) — só durante o Play, como as de 2D ---
KZ_SCRIPT_API int kz_physics3d_raycast(float x0, float y0, float z0,
                                       float x1, float y1, float z1,
                                       float* outX, float* outY, float* outZ,
                                       float* outFraction, uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    glm::vec3 point;
    float fraction = 1.0f;
    if (!s_ActiveScene->Raycast3D({ x0, y0, z0 }, { x1, y1, z1 }, hit, point, fraction)) return 0;
    if (outX) *outX = point.x;
    if (outY) *outY = point.y;
    if (outZ) *outZ = point.z;
    if (outFraction) *outFraction = fraction;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

KZ_SCRIPT_API int kz_physics3d_overlap_sphere(float x, float y, float z, float radius,
                                              uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    if (!s_ActiveScene->OverlapSphere3D({ x, y, z }, radius, hit)) return 0;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

// Overlap BOX 3D (AABB centrada em xyz com metades hx/hy/hz).
KZ_SCRIPT_API int kz_physics3d_overlap_box(float x, float y, float z,
                                           float hx, float hy, float hz,
                                           uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    if (!s_ActiveScene->OverlapBox3D({ x, y, z }, { hx, hy, hz }, hit)) return 0;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

// Quantas entidades a esfera toca (pré-consulta pra alocar o buffer).
KZ_SCRIPT_API int kz_physics3d_overlap_sphere_count(float x, float y, float z, float radius) {
    if (s_ActiveScene == nullptr) return 0;
    std::vector<kizuri::Entity> hits;
    s_ActiveScene->OverlapSphereAll3D({ x, y, z }, radius, hits);
    return (int)hits.size();
}

// Preenche o buffer com as entidades tocadas pela esfera (até maxCount).
KZ_SCRIPT_API int kz_physics3d_overlap_sphere_fill(float x, float y, float z, float radius,
                                                  uint32_t* outHandles, int maxCount) {
    if (s_ActiveScene == nullptr || maxCount <= 0) return 0;
    std::vector<kizuri::Entity> hits;
    s_ActiveScene->OverlapSphereAll3D({ x, y, z }, radius, hits);
    int written = 0;
    for (auto& e : hits) {
        if (written >= maxCount) break;
        outHandles[written++] = kizuri::scripting::RegisterEntityHandle(e);
    }
    return written;
}

// --- Nome da entidade (Tag) ---
KZ_SCRIPT_API int kz_entity_get_name(uint32_t entity, char* outBuffer, int bufferSize) {
    auto e = Resolve(entity);
    if (!e || outBuffer == nullptr || bufferSize <= 0) return 0;
    std::string name;
    if (e.HasComponent<kizuri::TagComponent>()) name = e.GetComponent<kizuri::TagComponent>().Tag;
    strncpy(outBuffer, name.c_str(), (size_t)bufferSize - 1);
    outBuffer[bufferSize - 1] = '\0';
    return 1;
}

KZ_SCRIPT_API void kz_entity_set_name(uint32_t entity, const char* name) {
    auto e = Resolve(entity);
    if (!e || name == nullptr || !e.HasComponent<kizuri::TagComponent>()) return;
    e.GetComponent<kizuri::TagComponent>().Tag = name;
}

KZ_SCRIPT_API int kz_particle_set_texture(uint32_t entity, const char* path) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>() || path == nullptr) return 0;
    auto& pc = e.GetComponent<kizuri::ParticleSystemComponent>();
    pc.TexturePath = path;
    pc.Texture = pc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(kizuri::Project::ResolvePath(path));
    return 1;
}

// --- Controle 3D ---
KZ_SCRIPT_API int kz_entity_get_world_position(uint32_t entity, float* outX, float* outY, float* outZ) {
    auto e = Resolve(entity);
    if (!e || s_ActiveScene == nullptr) return 0;
    glm::vec3 p = glm::vec3(s_ActiveScene->GetWorldTransform(e)[3]);
    if (outX) *outX = p.x;
    if (outY) *outY = p.y;
    if (outZ) *outZ = p.z;
    return 1;
}

// Aponta a rotação da entidade (euler fps: pitch=x, yaw=y) pra encarar um ponto.
KZ_SCRIPT_API void kz_entity_look_at(uint32_t entity, float tx, float ty, float tz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>() || s_ActiveScene == nullptr) return;
    glm::vec3 pos = glm::vec3(s_ActiveScene->GetWorldTransform(e)[3]);
    glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - pos);
    float yaw = glm::atan(d.z, d.x);
    float pitch = glm::asin(glm::clamp(d.y, -1.0f, 1.0f));
    e.GetComponent<kizuri::TransformComponent>().Rotation = { pitch, yaw, 0.0f };
}

} // extern "C"