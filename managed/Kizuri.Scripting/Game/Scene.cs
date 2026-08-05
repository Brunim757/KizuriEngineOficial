// Scene — acesso à cena ativa em runtime: criar entidades, instanciar
// prefabs, trocar de cena, pegar a câmera primária e fazer raycast 2D.
namespace Kizuri;

public static class Scene
{
	// Cria uma entidade vazia (Transform + Tag) na cena ativa.
	public static Entity CreateEntity(string name = "")
		=> new(Interop.KizuriNative.kz_scene_create_entity(name));

	// Instancia uma .kzprefab na posição dada. Em runtime (Play/KizuriGame)
	// a prefab ganha corpos de física e dispara OnCreate dos scripts.
	public static Entity InstantiatePrefab(string prefabPath, Math.Vector3 position = default)
	{
		var p = position;
		return new(Interop.KizuriNative.kz_scene_instantiate_prefab(prefabPath, p.X, p.Y, p.Z));
	}

	// Pedido de troca de cena (o caminho é resolvido relativo ao projeto).
	// A troca real acontece no fim do frame.
	public static void Load(string scenePath)
		=> Interop.KizuriNative.kz_scene_request_load(scenePath);

	// Entidade com CameraComponent marcada como Primary (0 se não houver).
	public static Entity GetPrimaryCamera()
		=> new(Interop.KizuriNative.kz_scene_get_primary_camera());

	// Primeira entidade da cena com o nome dado (Tag). Invalid se não achar.
	public static Entity Find(string name)
		=> new(Interop.KizuriNative.kz_scene_find_entity(name));

	// Duplica a entidade (com toda a subárvore) e devolve a cópia, com um
	// leve deslocamento pra não nascer em cima do original.
	public static Entity Duplicate(Entity entity)
		=> new(Interop.KizuriNative.kz_scene_duplicate_entity(entity.Handle));

	// Raycast 2D contra o mundo Box2D (só funciona durante o Play). Devolve
	// a primeira entidade atingida e o ponto do impacto. false = nada acertado.
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

	// Raycast 3D contra o mundo Bullet (só durante o Play). Devolve a primeira
	// entidade atingida, o ponto do impacto e a fração [0,1].
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

	// OverlapSphere 3D (Bullet, só no Play): true se alguma entidade com
	// collider tocar a esfera.
	public static bool OverlapSphere3D(Math.Vector3 center, float radius, out Entity hit)
	{
		hit = Entity.Invalid;
		uint handle = 0;
		if (Interop.KizuriNative.kz_physics3d_overlap_sphere(center.X, center.Y, center.Z, radius, out handle) == 0)
			return false;
		hit = new Entity(handle);
		return true;
	}

	// OverlapCircle 2D: true se algum collider tocar o círculo (só no Play).
	// Devolve a entidade mais próxima.
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
