using Kizuri;
using Kizuri.Math;

// Demo3D — exercita a API 3D da v0.2: luzes dinâmicas (direcional + ponto),
// mesh renderer com material PBR, rotação/escala em runtime e parâmetros de
// câmera. O cubo usa o primeiro asset real da engine (content/models/Cube.glb
// importado via glTF) quando o path é apontado; por padrão usa o builtin.
public sealed class Demo3D : Script
{
	private Entity _cube;
	private float _t;

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

		// Ajusta a câmera de perspectiva da cena em runtime.
		var cam = Scene.GetPrimaryCamera();
		if (cam.IsValid) cam.SetCamera(60f, 0.05f, 500f);

		Log.Info("Demo3D pronto: sol + luz pontual + cubo PBR.");
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
	}
}
