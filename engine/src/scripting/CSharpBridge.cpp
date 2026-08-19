
#include "kizuri/scripting/CSharpBridge.h"
#include "kizuri/scripting/CSharpBridgeInternal.h"
#include "kizuri/core/Log.hpp"
#include "kizuri/core/Input.hpp"
#include "kizuri/ecs/Scene.hpp"
#include "kizuri/ecs/Entity.hpp"
#include "kizuri/ecs/Components.hpp"
#include "kizuri/net/NetworkFacade.hpp"
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
double s_Elapsed = 0.0;
double s_UnscaledElapsed = 0.0;

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

}

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

}
}

namespace {
std::unordered_map<int, bool>& PrevMouseButtonState() {
    static std::unordered_map<int, bool> s_Prev;
    return s_Prev;
}
std::unordered_map<int, bool>& PrevKeyState() {
    static std::unordered_map<int, bool> s_Prev;
    return s_Prev;
}
}

extern "C" {

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

KZ_SCRIPT_API void kz_log(int channel, int level, const char* message) {

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

KZ_SCRIPT_API double kz_time_delta_seconds() {
    return s_DeltaSeconds;
}

KZ_SCRIPT_API void kz_set_time_scale(float scale) {
    kizuri::scripting::SetTimeScale(scale);
}

KZ_SCRIPT_API float kz_get_time_scale() {
    return kizuri::scripting::GetTimeScale();
}

KZ_SCRIPT_API int kz_input_is_key_pressed(int key) {
    return kizuri::Input::IsKeyPressed(key) ? 1 : 0;
}

KZ_SCRIPT_API int kz_input_is_mouse_button_pressed(int button) {
    return kizuri::Input::IsMouseButtonPressed(button) ? 1 : 0;
}

KZ_SCRIPT_API int kz_input_is_mouse_button_down(int button) {
    bool pressed = kizuri::Input::IsMouseButtonPressed(button);
    bool& prev = PrevMouseButtonState()[button];
    bool down = pressed && !prev;
    prev = pressed;
    return down ? 1 : 0;
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

KZ_SCRIPT_API int kz_input_is_action_pressed(const char* action) {
    return action ? (kizuri::Input::IsActionPressed(action) ? 1 : 0) : 0;
}
KZ_SCRIPT_API void kz_input_set_action_key(const char* action, int key) {
    if (action) kizuri::Input::SetActionKey(action, key);
}
KZ_SCRIPT_API int kz_input_get_action_key(const char* action) {
    return action ? kizuri::Input::GetActionKey(action) : -1;
}

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

KZ_SCRIPT_API void kz_entity_set_world_position(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return;
    glm::vec3 desired(x, y, z);
    auto* rel = s_ActiveScene->GetRegistry().try_get<kizuri::RelationshipComponent>(e.GetHandle());
    if (rel && rel->Parent) {
        kizuri::Entity parent = s_ActiveScene->GetEntityByUUID(rel->Parent);
        if (parent) {
            glm::mat4 parentWorld = s_ActiveScene->GetWorldTransform(parent);
            glm::vec4 local = glm::inverse(parentWorld) * glm::vec4(desired, 1.0f);
            e.GetComponent<kizuri::TransformComponent>().Translation = glm::vec3(local);
            return;
        }
    }
    e.GetComponent<kizuri::TransformComponent>().Translation = desired;
}

KZ_SCRIPT_API void kz_entity_set_parent(uint32_t child, uint32_t parent) {
    auto c = Resolve(child);
    if (!c) return;
    kizuri::Entity p;
    if (parent != 0) {
        p = Resolve(parent);
        if (!p) return;
    }

    c.SetParent(p);
}

KZ_SCRIPT_API uint32_t kz_scene_instantiate_prefab(const char* path, float x, float y, float z) {
    if (s_ActiveScene == nullptr || path == nullptr) return 0;

    kizuri::Entity entity = s_ActiveScene->Instantiate(kizuri::Project::ResolvePath(path), glm::vec3(x, y, z));
    if (!entity) return 0;
    return kizuri::scripting::RegisterEntityHandle(entity);
}

KZ_SCRIPT_API void kz_scene_draw_instanced(const char* meshSource, float r, float g, float b,
                                          const float* transformData, int count) {
    if (s_ActiveScene == nullptr || meshSource == nullptr || transformData == nullptr || count <= 0) return;
    auto mesh = kizuri::Mesh::FromSource(kizuri::Project::ResolvePath(meshSource));
    if (!mesh) return;
    std::vector<glm::mat4> transforms((size_t)count);
    for (int i = 0; i < count; ++i) {
        const float* src = transformData + i * 16;
        transforms[i] = glm::mat4(src[0], src[1], src[2], src[3],
                                  src[4], src[5], src[6], src[7],
                                  src[8], src[9], src[10], src[11],
                                  src[12], src[13], src[14], src[15]);
    }
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

KZ_SCRIPT_API int kz_scene_count_entities_with_tag(const char* tag) {
    if (s_ActiveScene == nullptr || tag == nullptr) return 0;
    int count = 0;
    auto view = s_ActiveScene->GetRegistry().view<kizuri::TagComponent>();
    for (auto e : view) {
        if (view.get<kizuri::TagComponent>(e).Tag == tag) ++count;
    }
    return count;
}

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

KZ_SCRIPT_API int kz_entity_get_child_count(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::RelationshipComponent>()) return 0;
    return (int)e.GetComponent<kizuri::RelationshipComponent>().Children.size();
}

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

KZ_SCRIPT_API void kz_entity_set_active(uint32_t entity, int active) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::IDComponent>()) return;
    e.GetComponent<kizuri::IDComponent>().Active = (active != 0);
}

KZ_SCRIPT_API int kz_entity_is_active(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    return s_ActiveScene->IsEntityActive(e) ? 1 : 0;
}

KZ_SCRIPT_API int kz_scene_get_entity_count() {
    if (s_ActiveScene == nullptr) return 0;
    int count = 0;
    for (auto e : s_ActiveScene->GetRegistry().storage<entt::entity>()) { (void)e; ++count; }
    return count;
}

KZ_SCRIPT_API uint32_t kz_scene_get_entity_at(int index) {
    if (s_ActiveScene == nullptr || index < 0) return 0;
    int i = 0;
    uint32_t handle = 0;
    for (auto e : s_ActiveScene->GetRegistry().storage<entt::entity>()) {
        if (i++ == index) handle = kizuri::scripting::RegisterEntityHandle(kizuri::Entity{ e, s_ActiveScene });
    }
    return handle;
}

KZ_SCRIPT_API uint32_t kz_scene_duplicate_entity(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    kizuri::Entity dup = s_ActiveScene->DuplicateEntity(e);
    if (!dup) return 0;
    return kizuri::scripting::RegisterEntityHandle(dup);
}

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

    if (!e.HasComponent<kizuri::UIRectComponent>()) {
        auto& ur = e.AddComponent<kizuri::UIRectComponent>();
        ur.Size = { 0.0f, 0.0f };
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

KZ_SCRIPT_API void kz_audio_set_volume(uint32_t entity, float volume) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AudioSourceComponent>()) return;
    auto& ac = e.GetComponent<kizuri::AudioSourceComponent>();
    ac.Volume = volume;
    if (ac.Handle != kizuri::kInvalidSound) kizuri::AudioEngine::SetVolume(ac.Handle, volume);
}

KZ_SCRIPT_API void kz_audio_set_spatial(uint32_t entity, int spatial, float minDist, float maxDist) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AudioSourceComponent>()) return;
    auto& ac = e.GetComponent<kizuri::AudioSourceComponent>();
    ac.Spatial = (spatial != 0);
    ac.MinDistance = minDist;
    ac.MaxDistance = std::max(maxDist, minDist);
    if (ac.Handle != kizuri::kInvalidSound)
        kizuri::AudioEngine::SetSoundAttenuation(ac.Handle, ac.MinDistance, ac.MaxDistance);
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

KZ_SCRIPT_API void kz_rigidbody2d_apply_force(uint32_t entity, float fx, float fy, int wake) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().ApplyForce({ fx, fy }, wake != 0);
}

KZ_SCRIPT_API void kz_rigidbody2d_set_angular_velocity(uint32_t entity, float w) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().SetAngularVelocity(w);
}

KZ_SCRIPT_API float kz_rigidbody2d_get_angular_velocity(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return 0.0f;
    return e.GetComponent<kizuri::Rigidbody2DComponent>().GetAngularVelocity();
}

KZ_SCRIPT_API void kz_rigidbody2d_set_fixed_rotation(uint32_t entity, int fixed) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::Rigidbody2DComponent>()) return;
    e.GetComponent<kizuri::Rigidbody2DComponent>().SetFixedRotation(fixed != 0);
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

KZ_SCRIPT_API int kz_entity_get_rotation(uint32_t entity, float* outXYZ) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return 0;
    const auto& r = e.GetComponent<kizuri::TransformComponent>().Rotation;
    if (outXYZ) { outXYZ[0] = r.x; outXYZ[1] = r.y; outXYZ[2] = r.z; }
    return 1;
}

KZ_SCRIPT_API int kz_entity_get_scale(uint32_t entity, float* outXYZ) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return 0;
    const auto& s = e.GetComponent<kizuri::TransformComponent>().Scale;
    if (outXYZ) { outXYZ[0] = s.x; outXYZ[1] = s.y; outXYZ[2] = s.z; }
    return 1;
}

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

KZ_SCRIPT_API void kz_audio_set_global_reverb(float wet, float roomSize, float damp) {
    kizuri::AudioEngine::SetGlobalReverb(wet, roomSize, damp);
}
KZ_SCRIPT_API void kz_audio_set_reverb(uint32_t entity, int enabled) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* ac = e.GetScene()->GetRegistry().try_get<kizuri::AudioSourceComponent>(e.GetHandle()))
        ac->Reverb = enabled != 0;
}

KZ_SCRIPT_API int kz_net_host(uint16_t port) { return kizuri::Network::Host(port) ? 1 : 0; }
KZ_SCRIPT_API int kz_net_connect(const char* addr, uint16_t port) {
    return (addr && kizuri::Network::Connect(addr, port)) ? 1 : 0;
}
KZ_SCRIPT_API void kz_net_shutdown() { kizuri::Network::Shutdown(); }
KZ_SCRIPT_API int kz_net_is_host() { return kizuri::Network::IsHost() ? 1 : 0; }
KZ_SCRIPT_API int kz_net_send(uint32_t peer, const uint8_t* data, uint32_t size) {
    return kizuri::Network::Send(peer, data, size) ? 1 : 0;
}
KZ_SCRIPT_API int kz_net_poll_event(int* outType, uint32_t* outPeer,
                                    uint8_t* outData, uint32_t maxData, uint32_t* outSize) {
    kizuri::net::Event ev;
    if (!kizuri::Network::PollEvent(ev)) return 0;
    if (outType) *outType = (int)ev.Type;
    if (outPeer) *outPeer = ev.Peer;
    if (outSize) *outSize = (uint32_t)std::min<size_t>(ev.Data.size(), maxData);
    if (outData && maxData > 0) std::memcpy(outData, ev.Data.data(), (uint32_t)std::min<size_t>(ev.Data.size(), maxData));
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_animation_blend(uint32_t entity, const char* clipA, const char* clipB, float weight) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& ab = e.AddComponent<kizuri::AnimationBlendComponent>();
    ab.ClipA = clipA ? clipA : "";
    ab.ClipB = clipB ? clipB : "";
    ab.BlendWeight = weight;
    return 1;
}
KZ_SCRIPT_API void kz_animation_set_blend(uint32_t entity, float weight) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* ab = e.GetScene()->GetRegistry().try_get<kizuri::AnimationBlendComponent>(e.GetHandle()))
        ab->BlendWeight = weight;
}
KZ_SCRIPT_API int kz_entity_add_two_bone_ik(uint32_t entity, const char* root, const char* mid, const char* tip) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& ik = e.AddComponent<kizuri::TwoBoneIKComponent>();
    ik.RootBone = root ? root : "";
    ik.MidBone = mid ? mid : "";
    ik.TipBone = tip ? tip : "";
    return 1;
}
KZ_SCRIPT_API void kz_ik_set_target(uint32_t entity, float x, float y, float z, float weight) {
    auto e = Resolve(entity);
    if (!e) return;
    if (auto* ik = e.GetScene()->GetRegistry().try_get<kizuri::TwoBoneIKComponent>(e.GetHandle())) {
        ik->Target = { x, y, z };
        ik->Weight = weight;
    }
}

KZ_SCRIPT_API int kz_entity_add_nav_grid(uint32_t entity, float ox, float oz, uint32_t width, uint32_t depth, float cellSize) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& ng = e.AddComponent<kizuri::NavGridComponent>();
    ng.Origin = { ox, 0.0f, oz };
    ng.Width = width;
    ng.Depth = depth;
    ng.CellSize = cellSize;
    return 1;
}
KZ_SCRIPT_API int kz_entity_add_nav_obstacle(uint32_t entity, float hx, float hy, float hz) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& no = e.AddComponent<kizuri::NavObstacleComponent>();
    no.HalfExtents = { hx, hy, hz };
    return 1;
}
KZ_SCRIPT_API int kz_entity_add_nav_agent(uint32_t entity, float speed, float turnSpeed) {
    auto e = Resolve(entity);
    if (!e) return 0;
    auto& na = e.AddComponent<kizuri::NavAgentComponent>();
    na.Speed = speed;
    na.TurnSpeed = turnSpeed;
    return 1;
}
KZ_SCRIPT_API void kz_navagent_set_destination(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e) return;
    e.GetScene()->SetNavDestination(e, { x, y, z });
}
KZ_SCRIPT_API void kz_navagent_stop(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return;
    e.GetScene()->StopNavAgent(e);
}
KZ_SCRIPT_API int kz_navagent_has_path(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0;
    return e.GetScene()->NavAgentHasPath(e) ? 1 : 0;
}
KZ_SCRIPT_API float kz_navagent_remaining_distance(uint32_t entity) {
    auto e = Resolve(entity);
    if (!e) return 0.0f;
    return e.GetScene()->NavAgentRemainingDistance(e);
}

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

KZ_SCRIPT_API void kz_camera_set_params(uint32_t entity, float fovDeg, float nearClip, float farClip) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::CameraComponent>()) return;
    auto& cc = e.GetComponent<kizuri::CameraComponent>();
    cc.PerspectiveFOV = fovDeg;
    cc.NearClip = nearClip;
    cc.FarClip = farClip;
}

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

KZ_SCRIPT_API int kz_animator_set_state(uint32_t entity, const char* stateName, float blendTime) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorStateMachineComponent>() || stateName == nullptr) return 0;
    auto& sm = e.GetComponent<kizuri::AnimatorStateMachineComponent>();

    if (!e.HasComponent<kizuri::AnimatorComponent>()) return 0;
    return sm.SetState(stateName, blendTime) ? 1 : 0;
}

KZ_SCRIPT_API const char* kz_animator_get_state_ptr(uint32_t entity) {
    static std::string cached;
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::AnimatorStateMachineComponent>()) return "";
    const auto& sm = e.GetComponent<kizuri::AnimatorStateMachineComponent>();
    if (sm.IsInState("")) return "";
    if (sm.CurrentState >= 0 && sm.CurrentState < (int)sm.States.size())
        cached = sm.States[(size_t)sm.CurrentState].Name;
    else cached.clear();
    return cached.c_str();
}

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
    return;
}

KZ_SCRIPT_API void kz_rigidbody3d_set_gravity_scale(uint32_t entity, float scale) {
    auto e = Resolve(entity);
    if (!e) return;
    s_ActiveScene->SetRigidbody3DGravityScale(e, scale);
}

KZ_SCRIPT_API void kz_rigidbody3d_set_damping(uint32_t entity, float linear, float angular) {
    auto e = Resolve(entity);
    if (!e) return;
    s_ActiveScene->SetRigidbody3DDamping(e, linear, angular);
}

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

KZ_SCRIPT_API int kz_physics3d_overlap_box(float x, float y, float z,
                                           float hx, float hy, float hz,
                                           uint32_t* outHitEntity) {
    if (s_ActiveScene == nullptr) return 0;
    kizuri::Entity hit;
    if (!s_ActiveScene->OverlapBox3D({ x, y, z }, { hx, hy, hz }, hit)) return 0;
    if (outHitEntity) *outHitEntity = kizuri::scripting::RegisterEntityHandle(hit);
    return 1;
}

KZ_SCRIPT_API int kz_physics3d_overlap_sphere_count(float x, float y, float z, float radius) {
    if (s_ActiveScene == nullptr) return 0;
    std::vector<kizuri::Entity> hits;
    s_ActiveScene->OverlapSphereAll3D({ x, y, z }, radius, hits);
    return (int)hits.size();
}

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

KZ_SCRIPT_API int kz_particle_set_rate(uint32_t entity, float rate) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    e.GetComponent<kizuri::ParticleSystemComponent>().EmissionRate = rate;
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_lifetime(uint32_t entity, float min, float max) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    auto& pc = e.GetComponent<kizuri::ParticleSystemComponent>();
    pc.LifetimeMin = min; pc.LifetimeMax = std::max(max, min);
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_velocity(uint32_t entity,
                                           float mnx, float mny, float mnz,
                                           float mxx, float mxy, float mxz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    auto& pc = e.GetComponent<kizuri::ParticleSystemComponent>();
    pc.VelocityMin = { mnx, mny, mnz };
    pc.VelocityMax = { mxx, mxy, mxz };
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_gravity(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    e.GetComponent<kizuri::ParticleSystemComponent>().Gravity = { x, y, z };
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_colors(uint32_t entity,
                                         float r, float g, float b, float a,
                                         float er, float eg, float eb, float ea) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    auto& pc = e.GetComponent<kizuri::ParticleSystemComponent>();
    pc.StartColor = { r, g, b, a };
    pc.EndColor = { er, eg, eb, ea };
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_size(uint32_t entity, float start, float end) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    auto& pc = e.GetComponent<kizuri::ParticleSystemComponent>();
    pc.StartSize = start; pc.EndSize = end;
    return 1;
}
KZ_SCRIPT_API int kz_particle_set_additive(uint32_t entity, int additive) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::ParticleSystemComponent>()) return 0;
    e.GetComponent<kizuri::ParticleSystemComponent>().Additive = (additive != 0);
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_sprite_animation(uint32_t entity, const char* sheetPath,
                                                 int fps, int totalFrames, int framesPerRow, int loop) {
    auto e = Resolve(entity);
    if (!e || sheetPath == nullptr) return 0;
    auto& sac = e.AddOrReplaceComponent<kizuri::SpriteAnimationComponent>();
    sac.SheetPath = sheetPath;
    sac.SheetTexture = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(sheetPath));
    sac.FPS = (float)fps;
    sac.TotalFrames = (uint32_t)std::max(totalFrames, 1);
    sac.FramesPerRow = (uint32_t)std::max(framesPerRow, 1);
    sac.Loop = (loop != 0);
    sac.Playing = true;
    return 1;
}
KZ_SCRIPT_API int kz_sprite_animation_play(uint32_t entity, int play) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::SpriteAnimationComponent>()) return 0;
    e.GetComponent<kizuri::SpriteAnimationComponent>().Playing = (play != 0);
    return 1;
}
KZ_SCRIPT_API int kz_sprite_animation_set_fps(uint32_t entity, float fps) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::SpriteAnimationComponent>()) return 0;
    e.GetComponent<kizuri::SpriteAnimationComponent>().FPS = fps;
    return 1;
}

KZ_SCRIPT_API int kz_entity_add_tilemap(uint32_t entity, const char* atlasPath,
                                        int atlasCols, int atlasRows,
                                        int mapW, int mapH, float tileW, float tileH) {
    auto e = Resolve(entity);
    if (!e || atlasPath == nullptr || mapW <= 0 || mapH <= 0) return 0;
    auto& tm = e.AddOrReplaceComponent<kizuri::TilemapComponent>();
    tm.AtlasPath = atlasPath;
    tm.AtlasTexture = kizuri::Texture2D::Create(kizuri::Project::ResolvePath(atlasPath));
    tm.AtlasColumns = (uint32_t)std::max(atlasCols, 1);
    tm.AtlasRows = (uint32_t)std::max(atlasRows, 1);
    tm.MapWidth = (uint32_t)mapW;
    tm.MapHeight = (uint32_t)mapH;
    tm.TileSize = { tileW, tileH };
    tm.Tiles.assign((size_t)mapW * mapH, 0);
    return 1;
}
KZ_SCRIPT_API int kz_tilemap_set_tile(uint32_t entity, int x, int y, int tileValue) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TilemapComponent>()) return 0;
    auto& tm = e.GetComponent<kizuri::TilemapComponent>();
    if (x < 0 || y < 0 || (uint32_t)x >= tm.MapWidth || (uint32_t)y >= tm.MapHeight) return 0;
    tm.Tiles[(size_t)y * tm.MapWidth + (size_t)x] = (uint32_t)std::max(tileValue, 0);
    tm.CollidersDirty = true;
    return 1;
}
KZ_SCRIPT_API int kz_tilemap_add_solid_tile(uint32_t entity, int tileValue) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TilemapComponent>() || tileValue <= 0) return 0;
    auto& tm = e.GetComponent<kizuri::TilemapComponent>();
    auto it = std::find(tm.SolidTileValues.begin(), tm.SolidTileValues.end(), (uint32_t)tileValue);
    if (it == tm.SolidTileValues.end()) {
        tm.SolidTileValues.push_back((uint32_t)tileValue);
        tm.CollidersDirty = true;
    }
    return 1;
}

KZ_SCRIPT_API int kz_entity_get_world_position(uint32_t entity, float* outX, float* outY, float* outZ) {
    auto e = Resolve(entity);
    if (!e || s_ActiveScene == nullptr) return 0;
    glm::vec3 p = glm::vec3(s_ActiveScene->GetWorldTransform(e)[3]);
    if (outX) *outX = p.x;
    if (outY) *outY = p.y;
    if (outZ) *outZ = p.z;
    return 1;
}

KZ_SCRIPT_API void kz_entity_look_at(uint32_t entity, float tx, float ty, float tz) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>() || s_ActiveScene == nullptr) return;
    glm::vec3 pos = glm::vec3(s_ActiveScene->GetWorldTransform(e)[3]);
    glm::vec3 d = glm::normalize(glm::vec3(tx, ty, tz) - pos);
    float yaw = glm::atan(d.z, d.x);
    float pitch = glm::asin(glm::clamp(d.y, -1.0f, 1.0f));
    e.GetComponent<kizuri::TransformComponent>().Rotation = { pitch, yaw, 0.0f };
}

}