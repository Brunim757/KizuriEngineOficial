#pragma once
// CSharpBridge.h — ABI C "KizuriEngine" para o lado managed (Kizuri.Scripting).
// Este header é INTERNO da engine: o que o dev de jogo enxerga é só o
// assembly .NET (Kizuri.Scripting.dll), nunca este arquivo.
//
// Regras do ABI (espelham KizuriNative.cs):
//   - Linkage C (extern "C") p/ nomes sem mangling.
//   - Tipos POD: uint/float/const char*/int. Sem glm, sem entt, sem ponteiros
//     de componente — os handles são uint32 opacos.
//   - bool vira int (1/0) (evita tunho de tamanho BOOL por plataforma).
#include <cstdint>

#if defined(_WIN32)
    #define KZ_SCRIPT_API __declspec(dllexport)
#else
    #define KZ_SCRIPT_API __attribute__((visibility("default")))
#endif

extern "C" {

// --- Host control (chamado pelo editor/KizuriGame) ---
KZ_SCRIPT_API void kz_set_active_scene(void* scene);
KZ_SCRIPT_API void kz_set_time_delta(double seconds);

// --- Log ---
KZ_SCRIPT_API void kz_log(int channel, int level, const char* message);

// --- Time ---
KZ_SCRIPT_API double kz_time_delta_seconds();

// --- Input ---
KZ_SCRIPT_API int kz_input_is_key_pressed(int key);

// --- Entities / scene ---
KZ_SCRIPT_API uint32_t kz_scene_create_entity(const char* name);
KZ_SCRIPT_API void kz_scene_destroy_entity(uint32_t entity);
KZ_SCRIPT_API int kz_entity_has_component(uint32_t entity, int componentType);
KZ_SCRIPT_API int kz_entity_get_transform(uint32_t entity,
                                          float* outPosition, // xyz
                                          float* outRotation, // xyz (euler rad)
                                          float* outScale);   // xyz
KZ_SCRIPT_API void kz_transform_set_position(uint32_t entity, float x, float y, float z);

// --- Physics 2D ---
KZ_SCRIPT_API int kz_entity_get_rigidbody2d(uint32_t entity, int* bodyType, float* outVelXY);
KZ_SCRIPT_API void kz_rigidbody2d_set_linear_velocity(uint32_t entity, float vx, float vy);
KZ_SCRIPT_API void kz_rigidbody2d_apply_linear_impulse(uint32_t entity, float ix, float iy, int wake);
KZ_SCRIPT_API void kz_rigidbody2d_set_transform(uint32_t entity, float x, float y, float angle);

} // extern "C"