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

#include <unordered_map>

namespace {

kizuri::Scene* s_ActiveScene = nullptr;
double s_DeltaSeconds = 0.0;

// handle opaco (uint32) -> UUID estável da entidade. Evita expor entt::entity
// ao C#; o UUID também sobrevive se a entidade for recriada.
uint32_t s_NextHandle = 1;
std::unordered_map<uint32_t, kizuri::UUID> s_Handles;
std::unordered_map<kizuri::UUID, uint32_t> s_HandlesByUUID;

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
}

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

// ---------------------------------------------------------------------------
// Input
// ---------------------------------------------------------------------------
KZ_SCRIPT_API int kz_input_is_key_pressed(int key) {
    return kizuri::Input::IsKeyPressed(key) ? 1 : 0;
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
        default: return 0;
    }
}

KZ_SCRIPT_API void kz_transform_set_position(uint32_t entity, float x, float y, float z) {
    auto e = Resolve(entity);
    if (!e || !e.HasComponent<kizuri::TransformComponent>()) return;
    auto& tc = e.GetComponent<kizuri::TransformComponent>();
    tc.Translation = glm::vec3(x, y, z);
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

} // extern "C"