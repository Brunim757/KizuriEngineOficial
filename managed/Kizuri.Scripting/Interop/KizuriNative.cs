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
    [DllImport(Lib)] internal static extern double kz_time_get_time();
    [DllImport(Lib)] internal static extern double kz_time_get_unscaled_time();
    [DllImport(Lib)] internal static extern ulong kz_time_get_frame();
    [DllImport(Lib)] internal static extern void kz_set_time_scale(float scale);
    [DllImport(Lib)] internal static extern float kz_get_time_scale();

    // ---- Input --------------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_input_is_key_pressed(int key);
    [DllImport(Lib)] internal static extern int kz_input_is_key_down(int key);
    [DllImport(Lib)] internal static extern int kz_input_is_action_pressed([MarshalAs(UnmanagedType.LPUTF8Str)] string action);
    [DllImport(Lib)] internal static extern void kz_input_set_action_key([MarshalAs(UnmanagedType.LPUTF8Str)] string action, int key);
    [DllImport(Lib)] internal static extern int kz_input_get_action_key([MarshalAs(UnmanagedType.LPUTF8Str)] string action);
    [DllImport(Lib)] internal static extern int kz_input_is_mouse_button_pressed(int button);
    [DllImport(Lib)] internal static extern int kz_input_is_mouse_button_down(int button);
    [DllImport(Lib)] internal static extern void kz_input_get_mouse_position(out float x, out float y);

    // ---- Entities / scene ----------------------------------------------------
    [DllImport(Lib)] internal static extern uint kz_scene_create_entity([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib)] internal static extern void kz_scene_destroy_entity(uint entity);
    [DllImport(Lib)] internal static extern int kz_entity_has_component(uint entity, int componentType);
    [DllImport(Lib)] internal static extern int kz_entity_get_transform(uint entity, out Math.Vector3 position, out Math.Vector3 rotation, out Math.Vector3 scale);
    [DllImport(Lib)] internal static extern void kz_transform_set_position(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_entity_set_world_position(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_transform_set_rotation(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_transform_set_scale(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern int kz_entity_get_rotation(uint entity, out Math.Vector3 rotation);
    [DllImport(Lib)] internal static extern int kz_entity_get_scale(uint entity, out Math.Vector3 scale);
    [DllImport(Lib)] internal static extern void kz_entity_set_parent(uint child, uint parent);

    // ---- Cena em runtime ------------------------------------------------------
    [DllImport(Lib)] internal static extern uint kz_scene_instantiate_prefab([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float x, float y, float z);
    [DllImport(Lib)] internal static extern uint kz_scene_instantiate_prefab_rot([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float x, float y, float z, float rx, float ry, float rz);
    [DllImport(Lib)] internal static extern void kz_scene_draw_instanced([MarshalAs(UnmanagedType.LPUTF8Str)] string meshSource, float r, float g, float b, [In] float[] transformData, int count);
    [DllImport(Lib)] internal static extern void kz_scene_request_load([MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern uint kz_scene_get_primary_camera();
    [DllImport(Lib)] internal static extern uint kz_scene_find_entity([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib)] internal static extern int kz_scene_count_entities_with_tag([MarshalAs(UnmanagedType.LPUTF8Str)] string tag);
    [DllImport(Lib)] internal static extern int kz_scene_get_entities_with_tag([MarshalAs(UnmanagedType.LPUTF8Str)] string tag, [Out] uint[] handles, int maxCount);
    [DllImport(Lib)] internal static extern int kz_entity_get_position(uint entity, out Math.Vector3 position);
    [DllImport(Lib)] internal static extern int kz_entity_get_parent(uint entity, out uint parent);
    [DllImport(Lib)] internal static extern int kz_entity_get_child_count(uint entity);
    [DllImport(Lib)] internal static extern int kz_entity_get_child(uint entity, int index, out uint child);
    [DllImport(Lib)] internal static extern void kz_entity_set_active(uint entity, int active);
    [DllImport(Lib)] internal static extern int kz_entity_is_active(uint entity);
    [DllImport(Lib)] internal static extern int kz_entity_get_name(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] System.Text.StringBuilder buffer, int bufferSize);
    [DllImport(Lib)] internal static extern void kz_entity_set_name(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib)] internal static extern int kz_particle_set_texture(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int kz_particle_set_rate(uint entity, float rate);
    [DllImport(Lib)] internal static extern int kz_particle_set_lifetime(uint entity, float min, float max);
    [DllImport(Lib)] internal static extern int kz_particle_set_velocity(uint entity, float mnx, float mny, float mnz, float mxx, float mxy, float mxz);
    [DllImport(Lib)] internal static extern int kz_particle_set_gravity(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern int kz_particle_set_colors(uint entity, float r, float g, float b, float a, float er, float eg, float eb, float ea);
    [DllImport(Lib)] internal static extern int kz_particle_set_size(uint entity, float start, float end);
    [DllImport(Lib)] internal static extern int kz_particle_set_additive(uint entity, int additive);
    [DllImport(Lib)] internal static extern int kz_entity_add_sprite_animation(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string sheetPath, int fps, int totalFrames, int framesPerRow, int loop);
    [DllImport(Lib)] internal static extern int kz_sprite_animation_play(uint entity, int play);
    [DllImport(Lib)] internal static extern int kz_sprite_animation_set_fps(uint entity, float fps);
    [DllImport(Lib)] internal static extern int kz_entity_add_tilemap(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string atlasPath, int atlasCols, int atlasRows, int mapW, int mapH, float tileW, float tileH);
    [DllImport(Lib)] internal static extern int kz_tilemap_set_tile(uint entity, int x, int y, int tileValue);
    [DllImport(Lib)] internal static extern int kz_tilemap_add_solid_tile(uint entity, int tileValue);
    [DllImport(Lib)] internal static extern int kz_entity_get_world_position(uint entity, out float x, out float y, out float z);
    [DllImport(Lib)] internal static extern void kz_entity_look_at(uint entity, float tx, float ty, float tz);
    [DllImport(Lib)] internal static extern uint kz_scene_duplicate_entity(uint entity);
    [DllImport(Lib)] internal static extern int kz_scene_get_entity_count();
    [DllImport(Lib)] internal static extern uint kz_scene_get_entity_at(int index);

    // ---- Adicionar componentes em runtime -------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_add_sprite(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string? texturePath);
    [DllImport(Lib)] internal static extern int kz_entity_add_text(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string text, float fontSize);
    [DllImport(Lib)] internal static extern int kz_entity_add_audio(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipPath, int loop, int playOnStart);
    [DllImport(Lib)] internal static extern int kz_entity_add_camera(uint entity, int projectionType);
    [DllImport(Lib)] internal static extern int kz_entity_add_camera_follow(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string targetName);
    [DllImport(Lib)] internal static extern void kz_camera_follow_set_target(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string targetName);
    [DllImport(Lib)] internal static extern void kz_camera_follow_set_offset(uint entity, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_camera_follow_set_smoothness(uint entity, float smoothness);
    [DllImport(Lib)] internal static extern void kz_camera_set_params(uint entity, float fovDeg, float nearClip, float farClip);
    [DllImport(Lib)] internal static extern int kz_entity_add_circle_collider2d(uint entity, float radius, float density, float friction, float restitution);
    [DllImport(Lib)] internal static extern void kz_entity_set_sorting_layer(uint entity, int layer);
    [DllImport(Lib)] internal static extern int kz_entity_add_light(uint entity, int type, float r, float g, float b, float intensity, float range, float innerConeDeg, float outerConeDeg, int castsShadow);
    [DllImport(Lib)] internal static extern int kz_entity_add_mesh_renderer(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string meshSource);
    [DllImport(Lib)] internal static extern int kz_entity_add_terrain(uint entity, uint segments, float size, float heightScale, uint seed);
    [DllImport(Lib)] internal static extern int kz_terrain_regenerate(uint entity, uint segments, float size, float heightScale, uint seed);

    // ---- Animação esquelética (skinning) -------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_add_animator(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string meshPath);
    [DllImport(Lib)] internal static extern int kz_animator_play(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string clipName);
    [DllImport(Lib)] internal static extern float kz_animator_get_time(uint entity);
    [DllImport(Lib)] internal static extern void kz_animator_set_time(uint entity, float time);
    [DllImport(Lib)] internal static extern void kz_animator_set_speed(uint entity, float speed);
    [DllImport(Lib)] internal static extern void kz_animator_set_loop(uint entity, int loop);
    [DllImport(Lib)] internal static extern void kz_animator_set_playing(uint entity, int playing);

    // ---- Física 3D (Bullet3) ---------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_add_rigidbody3d(uint entity, int bodyType, float mass);
    [DllImport(Lib)] internal static extern int kz_entity_add_box_collider3d(uint entity, float hx, float hy, float hz);
    [DllImport(Lib)] internal static extern int kz_entity_add_sphere_collider3d(uint entity, float radius);
    [DllImport(Lib)] internal static extern int kz_rigidbody3d_apply_force(uint entity, float fx, float fy, float fz);
    [DllImport(Lib)] internal static extern int kz_rigidbody3d_apply_impulse(uint entity, float ix, float iy, float iz);
    [DllImport(Lib)] internal static extern int kz_rigidbody3d_get_linear_velocity(uint entity, out float vx, out float vy, out float vz);
    [DllImport(Lib)] internal static extern void kz_rigidbody3d_set_linear_velocity(uint entity, float vx, float vy, float vz);
    [DllImport(Lib)] internal static extern int kz_rigidbody3d_apply_torque(uint entity, float tx, float ty, float tz);
    [DllImport(Lib)] internal static extern int kz_rigidbody3d_get_angular_velocity(uint entity, out float wx, out float wy, out float wz);
    [DllImport(Lib)] internal static extern void kz_rigidbody3d_set_angular_velocity(uint entity, float wx, float wy, float wz);
    [DllImport(Lib)] internal static extern void kz_rigidbody3d_set_gravity_scale(uint entity, float scale);
    [DllImport(Lib)] internal static extern void kz_rigidbody3d_set_damping(uint entity, float linear, float angular);
    [DllImport(Lib)] internal static extern int kz_physics3d_raycast(float x0, float y0, float z0, float x1, float y1, float z1, out float hitX, out float hitY, out float hitZ, out float fraction, out uint hitEntity);
    [DllImport(Lib)] internal static extern int kz_physics3d_overlap_sphere(float x, float y, float z, float radius, out uint hitEntity);
    [DllImport(Lib)] internal static extern int kz_physics3d_overlap_box(float x, float y, float z, float hx, float hy, float hz, out uint hitEntity);
    [DllImport(Lib)] internal static extern int kz_physics3d_overlap_sphere_count(float x, float y, float z, float radius);
    [DllImport(Lib)] internal static extern int kz_physics3d_overlap_sphere_fill(float x, float y, float z, float radius, [Out] uint[] handles, int maxCount);

    // ---- Mutação de componentes em runtime ------------------------------------
    [DllImport(Lib)] internal static extern int kz_sprite_set_texture(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int kz_sprite_set_color(uint entity, float r, float g, float b, float a);
    [DllImport(Lib)] internal static extern void kz_sprite_set_flip(uint entity, int flipX, int flipY);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_gravity_scale(uint entity, float scale);
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
    [DllImport(Lib)] internal static extern int kz_material_set_metallic_roughness_map(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern int kz_material_set_height_map(uint entity, [MarshalAs(UnmanagedType.LPUTF8Str)] string path);
    [DllImport(Lib)] internal static extern void kz_material_set_height_scale(uint entity, float scale);
    [DllImport(Lib)] internal static extern void kz_entity_add_character_controller(uint entity, float speed, float gravity);
    [DllImport(Lib)] internal static extern void kz_entity_move_character(uint entity, float x, float z);
    [DllImport(Lib)] internal static extern void kz_entity_add_timeline(uint entity);
    [DllImport(Lib)] internal static extern void kz_timeline_play(uint entity, int play);
    [DllImport(Lib)] internal static extern void kz_timeline_set_time(uint entity, float time);
    [DllImport(Lib)] internal static extern void kz_timeline_add_keyframe(uint entity, float time, float px, float py, float pz);
    [DllImport(Lib)] internal static extern void kz_entity_set_layer(uint entity, int layer);
    [DllImport(Lib)] internal static extern int kz_entity_get_layer(uint entity);
    [DllImport(Lib)] internal static extern void kz_entity_set_collision_mask(uint entity, uint mask);
    [DllImport(Lib)] internal static extern uint kz_entity_get_collision_mask(uint entity);
    [DllImport(Lib)] internal static extern void kz_material_set_emissive(uint entity, float r, float g, float b, float strength);

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
    [DllImport(Lib)] internal static extern void kz_audio_set_volume(uint entity, float volume);
    [DllImport(Lib)] internal static extern void kz_audio_set_spatial(uint entity, int spatial, float minDist, float maxDist);
    [DllImport(Lib)] internal static extern void kz_audio_play_one_shot([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float volume);
    [DllImport(Lib)] internal static extern void kz_audio_play_one_shot_at([MarshalAs(UnmanagedType.LPUTF8Str)] string path, float volume, float x, float y, float z);
    [DllImport(Lib)] internal static extern void kz_audio_stop_all();
    [DllImport(Lib)] internal static extern void kz_audio_set_master_volume(float volume);
    [DllImport(Lib)] internal static extern void kz_audio_set_group_volume(int group, float volume);
    [DllImport(Lib)] internal static extern float kz_audio_get_group_volume(int group);

    // ---- Physics 2D ----------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_get_rigidbody2d(uint entity, out int bodyType, out Math.Vector2 velocity);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_linear_velocity(uint entity, float vx, float vy);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_apply_linear_impulse(uint entity, float ix, float iy, int wake);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_apply_force(uint entity, float fx, float fy, int wake);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_angular_velocity(uint entity, float w);
    [DllImport(Lib)] internal static extern float kz_rigidbody2d_get_angular_velocity(uint entity);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_fixed_rotation(uint entity, int fixedRotation);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_transform(uint entity, float x, float y, float angle);
    [DllImport(Lib)] internal static extern int kz_physics2d_raycast(float x0, float y0, float x1, float y1, out float hitX, out float hitY, out uint hitEntity);
    [DllImport(Lib)] internal static extern int kz_physics2d_overlap_circle(float x, float y, float radius, out uint hitEntity);
}