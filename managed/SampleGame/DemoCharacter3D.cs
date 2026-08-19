using Kizuri;
using Kizuri.Math;




public sealed class DemoCharacter3D : Script
{
	private Entity _player;
	private Entity _flutuante;
	private float _gravityToggleTimer = 3f;
	private bool _gravityDown = true;

	public override void OnCreate()
	{
		
		var sun = Scene.CreateEntity("Sol");
		sun.AddLight(LightType.Directional, 1f, 0.95f, 0.85f, intensity: 2f);
		sun.SetRotation(new Vector3(46f, 23f, 0f));

		
		var terrain = Scene.CreateEntity("Terreno");
		terrain.AddTerrain(64, 60f, 6f, 42);
		terrain.SetMaterial(0.32f, 0.38f, 0.26f, roughness: 0.95f);
		terrain.AddRigidbody3D(BodyType3D.Static);

		
		_player = Scene.CreateEntity("Jogador");
		_player.AddMeshRenderer("builtin:capsule");
		_player.SetMaterial(0.25f, 0.55f, 0.9f, metallic: 0.2f, roughness: 0.4f);
		_player.SetPosition(new Vector3(0f, 3f, 0f));
		_player.SetScale(new Vector3(0.6f, 1.4f, 0.6f));
		_player.AddCharacterController(6f, -20f); 

		
		for (int i = 0; i < 4; ++i)
		{
			var box = Scene.CreateEntity("Caixa " + i);
			box.AddMeshRenderer("builtin:cube");
			box.SetMaterial(0.8f, 0.4f + i * 0.1f, 0.2f, roughness: 0.4f);
			box.SetPosition(new Vector3(-6f + i * 4f, 10f + i * 2f, 4f));
			box.AddRigidbody3D(BodyType3D.Dynamic, 1f);
			box.AddBoxCollider3D(0.5f, 0.5f, 0.5f);
		}

		
		_flutuante = Scene.CreateEntity("Flutuante");
		_flutuante.AddMeshRenderer("builtin:cube");
		_flutuante.SetMaterial(0.9f, 0.2f, 0.2f, roughness: 0.3f);
		_flutuante.SetPosition(new Vector3(8f, 6f, -6f));
		_flutuante.AddRigidbody3D(BodyType3D.Dynamic, 1f);
		_flutuante.AddBoxCollider3D(0.5f, 0.5f, 0.5f);

		
		var cam = Scene.CreateEntity("Câmera");
		cam.AddCamera(perspective3D: true);
		cam.AddCameraFollow("Jogador", new Vector3(0f, 3.5f, -7f), 6f);
	}

	public override void OnUpdate(float deltaSeconds)
	{
		
		var wish = Vector2.Zero;
		if (Input.IsKeyPressed(Key.W)) wish.Y += 1f;
		if (Input.IsKeyPressed(Key.S)) wish.Y -= 1f;
		if (Input.IsKeyPressed(Key.A)) wish.X -= 1f;
		if (Input.IsKeyPressed(Key.D)) wish.X += 1f;
		if (wish.Length > 0.001f) wish /= wish.Length;
		_player.MoveCharacter(wish.X, wish.Y);

		
		_gravityToggleTimer -= deltaSeconds;
		if (_gravityToggleTimer <= 0f)
		{
			_gravityToggleTimer = 3f;
			_gravityDown = !_gravityDown;
			_flutuante.SetGravityScale3D(_gravityDown ? 1f : 0f);
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
