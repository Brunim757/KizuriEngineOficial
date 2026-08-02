// Entity — handle de entidade do jogo. A engine é opaca: nada de EnTT aqui.
namespace Kizuri;

public struct Transform
{
	public Math.Vector3 Translation;
	public Math.Vector3 Rotation; // euler, radianos
	public Math.Vector3 Scale;

	public static Transform Identity => new()
	{
		Translation = Math.Vector3.Zero,
		Rotation = Math.Vector3.Zero,
		Scale = Math.Vector3.One,
	};
}

public readonly struct Entity
{
	internal readonly uint Handle;

	internal Entity(uint handle) => Handle = handle;

	public static Entity Invalid => new(0);
	public bool IsValid => Handle != 0;
	public uint Id => Handle;

	public bool HasComponent(ComponentType type)
		=> Interop.KizuriNative.kz_entity_has_component(Handle, (int)type) != 0;

	public bool HasTransform => HasComponent(ComponentType.Transform);
	public bool HasRigidbody2D => HasComponent(ComponentType.Rigidbody2D);

	public bool TryGetTransform(out Transform t)
	{
		t = Transform.Identity;
		var pos = default(Math.Vector3); var rot = default(Math.Vector3); var scale = default(Math.Vector3);
		if (Interop.KizuriNative.kz_entity_get_transform(Handle, out pos, out rot, out scale) == 0) return false;
		t.Translation = pos; t.Rotation = rot; t.Scale = scale;
		return true;
	}

	public bool TryGetRigidbody2D(out Rigidbody2D rb)
	{
		rb = default;
		if (Interop.KizuriNative.kz_entity_get_rigidbody2d(Handle, out var bodyType, out var velocity) == 0) return false;
		rb = new Rigidbody2D { Handle = Handle, m_Type = (BodyType)bodyType, Velocity = velocity };
		return true;
	}

	public void SetPosition(Math.Vector3 position)
		=> Interop.KizuriNative.kz_transform_set_position(Handle, position.X, position.Y, position.Z);

	public void Destroy() => Interop.KizuriNative.kz_scene_destroy_entity(Handle);
}

public enum ComponentType
{
	Transform = 0,
	Rigidbody2D = 1,
}