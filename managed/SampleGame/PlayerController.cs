using Kizuri;
using Kizuri.Math;

// PlayerController — demonstra a API de gameplay: movimento, projéteis
// criados em runtime, HUD dinâmico, save/load e raycast 2D.
public sealed class PlayerController : Script
{
	private const float Speed = 5.0f;

	private Entity _hud;
	private readonly List<Bullet> _bullets = new();
	private float _score;
	private bool _mouseDown;
	private float _raycastTimer;

	private struct Bullet
	{
		public Entity Entity;
		public Vector3 Velocity;
		public float Life;
	}

	public override void OnCreate()
	{
		Log.Info($"PlayerController entidade '{Entity.Id}' pronta.");

		// HUD criado e alterado em runtime (texto world-space, atualizado no OnUpdate).
		_hud = Scene.CreateEntity("HUD");
		_hud.AddText("Pontos: 0", 32f);
		_hud.SetPosition(new Vector3(-8f, 5f, 0f));

		// Save: o Get* carrega do disco automaticamente na primeira consulta.
		_score = SaveSystem.GetFloat("score", 0f);
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

		// Clique esquerdo atira um projétil (entidade criada em runtime com
		// sprite de cor sólida; o movimento é atualizado aqui embaixo).
		if (Input.IsMouseButtonPressed(MouseButton.Left) && !_mouseDown)
		{
			_mouseDown = true;
			if (Entity.TryGetTransform(out var tp))
			{
				var bullet = Scene.CreateEntity("Bullet");
				bullet.AddSprite(); // sem textura = cor sólida
				bullet.SetSpriteColor(1f, 0.85f, 0.2f);
				bullet.SetPosition(tp.Translation);
				_bullets.Add(new Bullet { Entity = bullet, Velocity = new Vector3(8f, 0f, 0f), Life = 1.5f });
			}
		}
		if (!Input.IsMouseButtonPressed(MouseButton.Left)) _mouseDown = false;

		for (int i = _bullets.Count - 1; i >= 0; --i)
		{
			var b = _bullets[i];
			b.Life -= deltaSeconds;
			if (b.Life <= 0f)
			{
				b.Entity.Destroy();
				_bullets.RemoveAt(i);
				continue;
			}
			if (b.Entity.TryGetTransform(out var bt))
			{
				bt.Translation += b.Velocity * deltaSeconds;
				b.Entity.SetPosition(bt.Translation);
			}
			_bullets[i] = b;
		}

		// Pontuação cresce com o tempo; F5 grava em save.json.
		_score += deltaSeconds * 10f;
		_hud.SetText($"Pontos: {(int)_score} | Projéteis: {_bullets.Count}");
		if (Input.IsKeyPressed(Key.F5))
		{
			SaveSystem.Set("score", _score);
			SaveSystem.Save();
			Log.Info("Jogo salvo.");
		}

		// Raycast 2D de exemplo: raio pra baixo, loga o primeiro hit a cada 0.5s.
		_raycastTimer -= deltaSeconds;
		if (_raycastTimer <= 0f)
		{
			_raycastTimer = 0.5f;
			if (Entity.TryGetTransform(out var rt))
			{
				var from = new Vector2(rt.Translation.X, rt.Translation.Y);
				var to = new Vector2(rt.Translation.X, rt.Translation.Y - 20f);
				if (Scene.Raycast2D(from, to, out var hit, out var point))
					Log.Info($"Raycast acertou entidade {hit.Id} em ({point.X:0.00}, {point.Y:0.00}).");
			}
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}

// Registro global dos scripts (equivalente 'RegisterScripts(name)').
public static class SampleGameModule
{
	[Kizuri.GameEntryPoint]
	public static void RegisterAll()
	{
		Kizuri.GameModule.Register<PlayerController>("PlayerController");
		Kizuri.GameModule.Register<UISample>("UISample");
		Kizuri.GameModule.Register<Demo3D>("Demo3D");
		Kizuri.GameModule.Register<DemoCharacter3D>("DemoCharacter3D");
		Kizuri.GameModule.Register<DemoNav>("DemoNav");
		Kizuri.GameModule.Register<DemoNet>("DemoNet");
	}
}
