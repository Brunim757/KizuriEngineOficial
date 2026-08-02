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

    // ---- Input --------------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_input_is_key_pressed(int key);

    // ---- Entities / scene ----------------------------------------------------
    [DllImport(Lib)] internal static extern uint kz_scene_create_entity([MarshalAs(UnmanagedType.LPUTF8Str)] string name);
    [DllImport(Lib)] internal static extern void kz_scene_destroy_entity(uint entity);
    [DllImport(Lib)] internal static extern int kz_entity_has_component(uint entity, int componentType);
    [DllImport(Lib)] internal static extern int kz_entity_get_transform(uint entity, out Math.Vector3 position, out Math.Vector3 rotation, out Math.Vector3 scale);
    [DllImport(Lib)] internal static extern void kz_transform_set_position(uint entity, float x, float y, float z);

    // ---- Physics 2D ----------------------------------------------------------
    [DllImport(Lib)] internal static extern int kz_entity_get_rigidbody2d(uint entity, out int bodyType, out Math.Vector2 velocity);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_linear_velocity(uint entity, float vx, float vy);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_apply_linear_impulse(uint entity, float ix, float iy, int wake);
    [DllImport(Lib)] internal static extern void kz_rigidbody2d_set_transform(uint entity, float x, float y, float angle);
}