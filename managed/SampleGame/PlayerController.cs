using Kizuri;
using Kizuri.Math;

// PlayerController — o usuário so vê assim em C#; nada de glm/entt/Box2D.
public sealed class PlayerController : Script
{
	private const float Speed = 5.0f;

public override void OnCreate()
	{
		Log.Info($"PlayerController entidade '{Entity.Id}' pronta.");
	}

	public override void OnUpdate(float deltaSeconds)
	{
		var wish = Vector2.Zero;
		if (Input.IsKeyPressed(Key.A) || Input.IsKeyPressed(Key.Left))  wish.X -= 1f;
		if (Input.IsKeyPressed(Key.D) || Input.IsKeyPressed(Key.Right)) wish.X += 1f;
		if (Input.IsKeyPressed(Key.W) || Input.IsKeyPressed(Key.Up))    wish.Y += 1f;
		if (Input.IsKeyPressed(Key.S) || Input.IsKeyPressed(Key.Down))  wish.Y -= 1f;

		if (wish.Length > 0.001f)
		{
			wish /= wish.Length;
			wish *= Speed;
		}

		if (Entity.TryGetRigidbody2D(out var rb))
		{
			var v = rb.GetLinearVelocity();
			rb.SetLinearVelocity(new Vector2(wish.X, v.Y));
		}
		else if (Entity.TryGetTransform(out var t))
		{
			t.Translation.X += wish.X * deltaSeconds;
			t.Translation.Y += wish.Y * deltaSeconds;
			Entity.SetPosition(t.Translation);
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}

// Registro global dos scripts (equivalente 'RegisterScripts(name)').
public static class SampleGameModule
{
	public static void RegisterAll()
	{
		Kizuri.GameModule.Register<PlayerController>("PlayerController");
	}
}