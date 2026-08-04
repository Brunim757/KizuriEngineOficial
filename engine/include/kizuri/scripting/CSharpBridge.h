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

// Sem dllexport/visibility aqui de propósito: no MinGW, quando QUALQUER
// símbolo é exportado explicitamente o linker desliga o --export-all-symbols
// (usado pelo WINDOWS_EXPORT_ALL_SYMBOLS do CMake), e aí só os kz_* sairiam
// da DLL e o link dos exemplos quebrava. Os kz_* são símbolos globais, então
// o export automático já cobre eles. (Antes: __declspec(dllexport)/visibility.)
#define KZ_SCRIPT_API

extern "C" {

// --- Host control (chamado pelo editor/KizuriGame) ---
KZ_SCRIPT_API void kz_set_active_scene(void* scene);
KZ_SCRIPT_API void kz_set_time_delta(double seconds);

// --- Log ---
KZ_SCRIPT_API void kz_log(int channel, int level, const char* message);

// --- Time ---
KZ_SCRIPT_API double kz_time_delta_seconds();
KZ_SCRIPT_API void kz_set_time_scale(float scale);
KZ_SCRIPT_API float kz_get_time_scale();

// --- Input ---
KZ_SCRIPT_API int kz_input_is_key_pressed(int key);
KZ_SCRIPT_API int kz_input_is_mouse_button_pressed(int button);
KZ_SCRIPT_API void kz_input_get_mouse_position(float* outX, float* outY);

// --- Entities / scene ---
KZ_SCRIPT_API uint32_t kz_scene_create_entity(const char* name);
KZ_SCRIPT_API void kz_scene_destroy_entity(uint32_t entity);
KZ_SCRIPT_API int kz_entity_has_component(uint32_t entity, int componentType);
KZ_SCRIPT_API int kz_entity_get_transform(uint32_t entity,
                                          float* outPosition, // xyz
                                          float* outRotation, // xyz (euler rad)
                                          float* outScale);   // xyz
KZ_SCRIPT_API void kz_transform_set_position(uint32_t entity, float x, float y, float z);
KZ_SCRIPT_API void kz_entity_set_parent(uint32_t child, uint32_t parent); // parent=0 destaca

// --- Cena em runtime ---
KZ_SCRIPT_API uint32_t kz_scene_instantiate_prefab(const char* path, float x, float y, float z);
KZ_SCRIPT_API void kz_scene_request_load(const char* path);
KZ_SCRIPT_API uint32_t kz_scene_get_primary_camera();
KZ_SCRIPT_API uint32_t kz_scene_duplicate_entity(uint32_t entity); // duplica + subárvore

// --- Adicionar componentes em runtime (0 = sem efeito se a entidade não existe) ---
KZ_SCRIPT_API int kz_entity_add_sprite(uint32_t entity, const char* texturePath); // vazio = cor sólida
KZ_SCRIPT_API int kz_entity_add_text(uint32_t entity, const char* text, float fontSize);
KZ_SCRIPT_API int kz_entity_add_audio(uint32_t entity, const char* clipPath, int loop, int playOnStart);
KZ_SCRIPT_API int kz_entity_add_camera(uint32_t entity, int projectionType); // 0=ortográfica 2D, 1=perspectiva 3D
KZ_SCRIPT_API int kz_entity_add_circle_collider2d(uint32_t entity, float radius,
                                                  float density, float friction, float restitution);
KZ_SCRIPT_API void kz_entity_set_sorting_layer(uint32_t entity, int layer); // ordem 2D de desenho

// --- Mutação de componentes em runtime ---
KZ_SCRIPT_API int kz_sprite_set_texture(uint32_t entity, const char* path);
KZ_SCRIPT_API int kz_sprite_set_color(uint32_t entity, float r, float g, float b, float a);
KZ_SCRIPT_API int kz_text_set_content(uint32_t entity, const char* text);
KZ_SCRIPT_API int kz_text_set_size(uint32_t entity, float size);
KZ_SCRIPT_API int kz_text_set_color(uint32_t entity, float r, float g, float b, float a);

// --- UI (Canvas / Rect / Botão / Texto em espaço de tela) ---
KZ_SCRIPT_API int kz_entity_add_ui_canvas(uint32_t entity, float orthoSize);
KZ_SCRIPT_API int kz_entity_add_ui_rect(uint32_t entity, float x, float y, float w, float h,
                                        float r, float g, float b, float a);
KZ_SCRIPT_API int kz_entity_add_ui_button(uint32_t entity, float x, float y, float w, float h,
                                          float r, float g, float b, float a);
KZ_SCRIPT_API int kz_entity_add_ui_text(uint32_t entity, const char* text, float fontSize,
                                        float r, float g, float b, float a);
KZ_SCRIPT_API int kz_ui_button_was_clicked(uint32_t entity);
KZ_SCRIPT_API int kz_ui_button_is_hovered(uint32_t entity);
KZ_SCRIPT_API void kz_ui_set_rect(uint32_t entity, float x, float y, float w, float h);
KZ_SCRIPT_API void kz_ui_set_color(uint32_t entity, float r, float g, float b, float a);

// --- Áudio ---
KZ_SCRIPT_API int kz_audio_play(uint32_t entity);   // toca o AudioSource da entidade
KZ_SCRIPT_API int kz_audio_stop(uint32_t entity);
KZ_SCRIPT_API void kz_audio_play_one_shot(const char* path, float volume);
KZ_SCRIPT_API void kz_audio_stop_all();

// --- Physics 2D ---
KZ_SCRIPT_API int kz_entity_get_rigidbody2d(uint32_t entity, int* bodyType, float* outVelXY);
KZ_SCRIPT_API void kz_rigidbody2d_set_linear_velocity(uint32_t entity, float vx, float vy);
KZ_SCRIPT_API void kz_rigidbody2d_apply_linear_impulse(uint32_t entity, float ix, float iy, int wake);
KZ_SCRIPT_API void kz_rigidbody2d_set_transform(uint32_t entity, float x, float y, float angle);
KZ_SCRIPT_API int kz_physics2d_raycast(float x0, float y0, float x1, float y1,
                                       float* outHitX, float* outHitY, uint32_t* outHitEntity);
KZ_SCRIPT_API int kz_physics2d_overlap_circle(float x, float y, float radius,
                                              uint32_t* outHitEntity);

} // extern "C"