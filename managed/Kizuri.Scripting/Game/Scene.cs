

namespace Kizuri;

public static class Scene
{
	
	public static Entity CreateEntity(string name = "")
		=> new(Interop.KizuriNative.kz_scene_create_entity(name));

	
	
	public static Entity InstantiatePrefab(string prefabPath, Math.Vector3 position = default)
	{
		var p = position;
		return new(Interop.KizuriNative.kz_scene_instantiate_prefab(prefabPath, p.X, p.Y, p.Z));
	}

	
	public static Entity InstantiatePrefab(string prefabPath, Math.Vector3 position, Math.Vector3 rotation)
	{
		var p = position;
		var r = rotation;
		return new(Interop.KizuriNative.kz_scene_instantiate_prefab_rot(prefabPath, p.X, p.Y, p.Z, r.X, r.Y, r.Z));
	}

	
	
	public static void Load(string scenePath)
		=> Interop.KizuriNative.kz_scene_request_load(scenePath);

	
	
	
	
	public static void DrawInstanced(string meshSource, Math.Vector3 color, float[] transforms, int count)
		=> Interop.KizuriNative.kz_scene_draw_instanced(meshSource, color.X, color.Y, color.Z, transforms, count);

	
	
	public static float[] MakeTransform(float x, float y, float z, float scale = 1f)
	{
		return new float[]
		{
			scale, 0f, 0f, 0f,
			0f, scale, 0f, 0f,
			0f, 0f, scale, 0f,
			x, y, z, 1f
		};
	}

	
	public static Entity GetPrimaryCamera()
		=> new(Interop.KizuriNative.kz_scene_get_primary_camera());

	
	public static Entity Find(string name)
		=> new(Interop.KizuriNative.kz_scene_find_entity(name));

	
	
	public static Entity[] EntitiesWithTag(string tag)
	{
		int count = Interop.KizuriNative.kz_scene_count_entities_with_tag(tag);
		if (count <= 0) return System.Array.Empty<Entity>();
		var handles = new uint[count];
		int got = Interop.KizuriNative.kz_scene_get_entities_with_tag(tag, handles, count);
		var result = new Entity[got];
		for (int i = 0; i < got; ++i) result[i] = new Entity(handles[i]);
		return result;
	}

	
	
	public static Entity Duplicate(Entity entity)
		=> new(Interop.KizuriNative.kz_scene_duplicate_entity(entity.Handle));

	
	
	public static Entity[] All
	{
		get
		{
			int count = Interop.KizuriNative.kz_scene_get_entity_count();
			if (count <= 0) return System.Array.Empty<Entity>();
			var result = new Entity[count];
			for (int i = 0; i < count; ++i) result[i] = new Entity(Interop.KizuriNative.kz_scene_get_entity_at(i));
			return result;
		}
	}

	
	
	public static bool Raycast2D(Math.Vector2 from, Math.Vector2 to, out Entity hit, out Math.Vector2 point)
	{
		hit = Entity.Invalid;
		point = default;
		uint handle = 0;
		float hx = 0f, hy = 0f;
		if (Interop.KizuriNative.kz_physics2d_raycast(from.X, from.Y, to.X, to.Y, out hx, out hy, out handle) == 0)
			return false;
		hit = new Entity(handle);
		point = new Math.Vector2(hx, hy);
		return true;
	}

	
	
	public static bool Raycast3D(Math.Vector3 from, Math.Vector3 to, out Entity hit, out Math.Vector3 point, out float fraction)
	{
		hit = Entity.Invalid;
		point = default;
		fraction = 1f;
		uint handle = 0;
		float hx = 0f, hy = 0f, hz = 0f;
		if (Interop.KizuriNative.kz_physics3d_raycast(from.X, from.Y, from.Z, to.X, to.Y, to.Z,
			out hx, out hy, out hz, out fraction, out handle) == 0)
			return false;
		hit = new Entity(handle);
		point = new Math.Vector3(hx, hy, hz);
		return true;
	}

	
	
	public static bool OverlapSphere3D(Math.Vector3 center, float radius, out Entity hit)
	{
		hit = Entity.Invalid;
		uint handle = 0;
		if (Interop.KizuriNative.kz_physics3d_overlap_sphere(center.X, center.Y, center.Z, radius, out handle) == 0)
			return false;
		hit = new Entity(handle);
		return true;
	}

	
	public static bool OverlapBox3D(Math.Vector3 center, Math.Vector3 halfExtents, out Entity hit)
	{
		hit = Entity.Invalid;
		uint handle = 0;
		if (Interop.KizuriNative.kz_physics3d_overlap_box(center.X, center.Y, center.Z,
			halfExtents.X, halfExtents.Y, halfExtents.Z, out handle) == 0)
			return false;
		hit = new Entity(handle);
		return true;
	}

	
	public static Entity[] OverlapSphereAll3D(Math.Vector3 center, float radius)
	{
		int count = Interop.KizuriNative.kz_physics3d_overlap_sphere_count(center.X, center.Y, center.Z, radius);
		if (count <= 0) return System.Array.Empty<Entity>();
		var handles = new uint[count];
		int got = Interop.KizuriNative.kz_physics3d_overlap_sphere_fill(center.X, center.Y, center.Z, radius, handles, count);
		var result = new Entity[got];
		for (int i = 0; i < got; ++i) result[i] = new Entity(handles[i]);
		return result;
	}

	
	
	public static bool OverlapCircle2D(Math.Vector2 center, float radius, out Entity hit)
	{
		hit = Entity.Invalid;
		uint handle = 0;
		if (Interop.KizuriNative.kz_physics2d_overlap_circle(center.X, center.Y, radius, out handle) == 0)
			return false;
		hit = new Entity(handle);
		return true;
	}
}
