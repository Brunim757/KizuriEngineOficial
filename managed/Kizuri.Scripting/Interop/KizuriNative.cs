// KizuriNative.cs — P/Invoke para o ABI C 'kz_*' exportado pela engine
// (engine/src/scripting/CSharpBridge.cpp). Regras:
//   - bool   -> int (1/0)
//   - structs de saída (Math.Vector2/3) -> out (ponteiro; layout pod, blittable)
//   - arg de entrada -> primitivos float (evita ambiguidades de marshalling)
//   - uint -> handle de entidade (opaco)
// Isolado aqui para que a parte pública do assembly nunca dependa de C++.
using System.Runtime.InteropServices;

namespace Kizuri.Interop;

internal static class KizuriNative
{
    private const string Lib = "KizuriEngine";

    [DllImport(Lib)] internal static extern void kz_set_active_scene(IntPtr scene);

    // ---- Log (channel 0 = core, 1 = app; level 0=trace..5=critical) --------
    [DllImport(Lib)] internal static extern void kz_log(int channel, int level, [MarshalAs(UnmanagedType.LPUTF8Str)] string message);

    // ---- Time ---------------------------------------------------------------
    [DllImport(Lib)] internal static extern void kz_set_time_delta(double seconds);
    [DllImport(Lib)] internal static extern double kz_time_delta_seconds();
    [DllImport(Lib)] internal static extern void kz_set_time_scale(float scale);
    [DllImport(Lib)] internal static extern float kz_get_time_scale();

    // ---- Input --------------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_input_is_key_pressed(int key);
    [DllImport(Lib)] internal static extern int kz_input_is_mouse_button_pressed(int button);
    [DllImport(Lib)] internal static extern void kz_input_get_mouse_position(out float x, out float y);

    // ---- Entities / scene ----------------------------------------------------
    [DllImport(Lib)] internal static extern uint kz_scene_create_entity([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib)] internal static extern void kz_scene_destroy_entity(uint entity);
    [DllImport(Lib)] internal static extern int kz_entity_has_component(uint entity, int componentType);
    [DllImport(Lib)] internal static extern int kz_entity_get_transform(uint entity, out Math.Vector3 position, out Math.Vector3 rotation, out Math.Vector3 scale);
    [DllImport(Lib)] internal static extern void kz_transform_set_position(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_transform_set_rotation(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_transform_set_scale(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_entity_set_parent(uint child, uint parent);

    // ---- Cena em runtime ------------------------------------------------------
    [DllImport(Lib)] internal static extern uint kz_scene_instantiate_prefab([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_scene_request_load([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern uint kz_scene_get_primary_camera();
    [DllImport(Lib)] internal static extern uint kz_scene_duplicate_entity(uint entity);

    // ---- Adicionar componentes em runtime -------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_add_sprite(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string? texturePath);
    [DllImport(Lib)] internal static extern int kz_entity_add_text(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string text, float fontSize);
    [DllImport(Lib)] internal static extern int kz_entity_add_audio(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipPath, int loop, int playOnStart);
    [DllImport(Lib)] internal static extern int kz_entity_add_camera(uint entity, int projectionType);
    [DllImport(Lib)] internal static extern void kz_camera_set_params(uint entity, float fovDeg, float nearClip, float farClip);
    [DllImport(Lib)] internal static extern int kz_entity_add_circle_collider2d(uint entity, float radius, float density, float friction, float restitution);
    [DllImport(Lib)] internal static extern void kz_entity_set_sorting_layer(uint entity, int layer);
    [DllImport(Lib)] internal static extern int kz_entity_add_light(uint entity, int type, float r, float g, float b, float intensity, float range, float innerConeDeg, float outerConeDeg);
    [DllImport(Lib)] internal static extern int kz_entity_add_mesh_renderer(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string meshSource);

    // ---- Mutação de componentes em runtime ------------------------------------
    [DllImport(Lib)] internal static extern int kz_sprite_set_texture(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int kz_sprite_set_color(uint entity, float r, float g, float b, float a);
    [DllImport(Lib)] internal static extern int kz_text_set_content(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string text);
    [DllImport(Lib)] internal static extern int kz_text_set_size(uint entity, float size);
    [DllImport(Lib)] internal static extern int kz_text_set_color(uint entity, float r, float g, float b, float a);

    // ---- Luz / material PBR --------------------------------------------------
    [DllImport(Lib)] internal static extern void kz_light_set_color(uint entity, float r, float g, float b);
    [DllImport(Lib)] internal static extern void kz_light_set_intensity(uint entity, float intensity);
    [DllImport(Lib)] internal static extern void kz_material_set_albedo(uint entity, float r, float g, float b);
    [DllImport(Lib)] internal static extern void kz_material_set_metallic(uint entity, float metallic);
    [DllImport(Lib)] internal static extern void kz_material_set_roughness(uint entity, float roughness);
    [DllImport(Lib)] internal static extern int kz_material_set_albedo_map(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int kz_material_set_normal_map(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);

    // ---- UI (Canvas / Rect / Botão / Texto em espaço de tela) ------------------
    [DllImport(Lib)] internal static extern int kz_entity_add_ui_canvas(uint entity, float orthoSize);
    [DllImport(Lib)] internal static extern int kz_entity_add_ui_rect(uint entity, float x, float y, float w, float h, float r, float g, float b, float a);
    [DllImport(Lib)] internal static extern int kz_entity_add_ui_button(uint entity, float x, float y, float w, float h, float r, float g, float b, float a);
    [DllImport(Lib)] internal static extern int kz_entity_add_ui_text(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string text, float fontSize, float r, float g, float b, float a);
    [DllImport(Lib)] internal static extern int kz_ui_button_was_clicked(uint entity);
    [DllImport(Lib)] internal static extern int kz_ui_button_is_hovered(uint entity);
    [DllImport(Lib)] internal static extern void kz_ui_set_rect(uint entity, float x, float y, float w, float h);
    [DllImport(Lib)] internal static extern void kz_ui_set_color(uint entity, float r, float g, float b, float a);

    // ---- Áudio ------------------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_audio_play(uint entity);
    [DllImport(Lib)] internal static extern int kz_audio_stop(uint entity);
    [DllImport(Lib)] internal static extern void kz_audio_play_one_shot([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float volume);
    [DllImport(Lib)] internal static extern void kz_audio_stop_all();

    // ---- Physics 2D ----------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_get_rigidbody2d(uint entity, out int bodyType, out Math.Vector2 velocity);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_linear_velocity(uint entity, float vx, float vy);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_apply_linear_impulse(uint entity, float ix, float iy, int wake);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_transform(uint entity, float x, float y, float angle);
    [DllImport(Lib)] internal static extern int kz_physics2d_raycast(float x0, float y0, float x1, float y1, out float hitX, out float hitY, out uint hitEntity);
    [DllImport(Lib)] internal static extern int kz_physics2d_overlap_circle(float x, float y, float radius, out uint hitEntity);
}