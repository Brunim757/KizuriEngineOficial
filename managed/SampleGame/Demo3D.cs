using Kizuri;
using Kizuri.Math;

// Demo3D — exercita a API 3D da v0.2: luzes dinâmicas (direcional + ponto),
// mesh renderer com material PBR, rotação/escala em runtime e parâmetros de
// câmera. O cubo usa o primeiro asset real da engine (content/models/Cube.glb
// importado via glTF) quando o path é apontado; por padrão usa o builtin.
public sealed class Demo3D : Script
{
	private Entity _cube;
	private Entity _physicsBox;
	private float _t;
	private float _impulseTimer = 1f;

	public override void OnCreate()
	{
		// Sol (direcional): a 1ª luz direcional da cena projeta sombra (CSM).
		var sun = Scene.CreateEntity("Sol");
		sun.AddLight(LightType.Directional, 1f, 0.95f, 0.85f, intensity: 2f);
		sun.SetRotation(new Vector3(0.8f, 0.4f, 0f));

		// Luz pontual colorida (efeito de fogo/magia).
		var point = Scene.CreateEntity("LuzPonto");
		point.AddLight(LightType.Point, 1f, 0.3f, 0.1f, intensity: 5f, range: 8f);
		point.SetPosition(new Vector3(2f, 1.5f, 0f));

		// Cubo PBR que gira e pulsa.
		_cube = Scene.CreateEntity("Cubo");
		_cube.AddMeshRenderer("builtin:cube");
		_cube.SetMaterial(0.9f, 0.4f, 0.1f, metallic: 0.1f, roughness: 0.35f);
		_cube.SetPosition(new Vector3(0f, 1f, 0f));

		// Física 3D (Bullet3): caixa dinâmica que cai e leva impulso.
		_physicsBox = Scene.CreateEntity("CaixaFisica");
		_physicsBox.AddMeshRenderer("builtin:cube");
		_physicsBox.SetMaterial(0.3f, 0.6f, 0.9f, metallic: 0.8f, roughness: 0.2f);
		_physicsBox.SetPosition(new Vector3(1.5f, 4f, 0f));
		_physicsBox.AddRigidbody3D(BodyType3D.Dynamic, 1f);
		_physicsBox.AddBoxCollider3D(0.5f, 0.5f, 0.5f);

		// Ajusta a câmera de perspectiva da cena em runtime.
		var cam = Scene.GetPrimaryCamera();
		if (cam.IsValid) cam.SetCamera(60f, 0.05f, 500f);

		// Busca por nome (valida Scene.Find).
		var found = Scene.Find("Cubo");
		Log.Info($"Demo3D pronto: sol + luz pontual + cubo PBR + física 3D. (Scene.Find('Cubo')={(found.IsValid ? "ok" : "falhou")})");
	}

	public override void OnUpdate(float deltaSeconds)
	{
		_t += deltaSeconds;
		if (_cube.IsValid && _cube.TryGetTransform(out var t))
		{
			t.Rotation.Y = _t * 0.8f;
			t.Rotation.X = _t * 0.5f;
			t.Translation.Y = 1f + (float)System.Math.Sin(_t * 2f) * 0.3f;
			_cube.SetRotation(t.Rotation);
			_cube.SetPosition(t.Translation);
			_cube.SetScale(new Vector3(1f + (float)System.Math.Sin(_t) * 0.2f, 1f, 1f));
		}

		// De tempos em tempos dá um impulso pra cima na caixa (Bullet3).
		_impulseTimer -= deltaSeconds;
		if (Input.IsKeyDown(Key.Space))
		{
			_impulseTimer = 3f; // espaço dispara imediatamente
			if (_physicsBox.IsValid) _physicsBox.ApplyImpulse(new Vector3(0f, 8f, 0f));
		}
		if (_impulseTimer <= 0f && _physicsBox.IsValid)
		{
			_impulseTimer = 3f;
			if (_physicsBox.TryGetVelocity(out var v))
			{
				Log.Info($"Caixa física vel. atual: ({v.X:0.00}, {v.Y:0.00}, {v.Z:0.00})");
				_physicsBox.ApplyImpulse(new Vector3(0f, 6f, 0f));
			}
		}
	}

	public override void OnCollisionBegin(Entity other) { }
	public override void OnCollisionEnd(Entity other) { }
	public override void OnDestroy() { }
}
