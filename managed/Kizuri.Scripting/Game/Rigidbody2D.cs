// Rigidbody2D — struct de física devolvida por Entity.TryGetRigidbody2D.
namespace Kizuri;

public enum BodyType { Static = 0, Dynamic, Kinematic }

public struct Rigidbody2D
{
	internal uint Handle;
	internal BodyType m_Type;
	internal Math.Vector2 Velocity;

	public BodyType Type => m_Type;

	public Math.Vector2 GetLinearVelocity()
		=> Interop.KizuriNative.kz_entity_get_rigidbody2d(Handle, out _, out var v) != 0 ? v : default;

	public void SetLinearVelocity(Math.Vector2 v)
		=> Interop.KizuriNative.kz_rigidbody2d_set_linear_velocity(Handle, v.X, v.Y);

	public void ApplyLinearImpulse(Math.Vector2 impulse, bool wake = true)
		=> Interop.KizuriNative.kz_rigidbody2d_apply_linear_impulse(Handle, impulse.X, impulse.Y, wake ? 1 : 0);

	// Força contínua (aplicar todo frame no OnUpdate, estilo thrust).
	public void ApplyForce(Math.Vector2 force, bool wake = true)
		=> Interop.KizuriNative.kz_rigidbody2d_apply_force(Handle, force.X, force.Y, wake ? 1 : 0);

	// Velocidade de rotação (rad/s) — chame todo frame ou use ApplyTorque em massa.
	public float GetAngularVelocity() => Interop.KizuriNative.kz_rigidbody2d_get_angular_velocity(Handle);
	public void SetAngularVelocity(float radiansPerSecond)
		=> Interop.KizuriNative.kz_rigidbody2d_set_angular_velocity(Handle, radiansPerSecond);

	// Impede a rotação do corpo (plataforma/tank que não pode tombar).
	public void SetFixedRotation(bool fixedRotation)
		=> Interop.KizuriNative.kz_rigidbody2d_set_fixed_rotation(Handle, fixedRotation ? 1 : 0);

	public void SetTransform(Math.Vector2 position, float angleRadians)
		=> Interop.KizuriNative.kz_rigidbody2d_set_transform(Handle, position.X, position.Y, angleRadians);
}