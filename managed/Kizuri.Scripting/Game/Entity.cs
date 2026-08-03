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
	public bool HasSprite => HasComponent(ComponentType.Sprite);
	public bool HasText => HasComponent(ComponentType.Text);
	public bool HasAudio => HasComponent(ComponentType.Audio);
	public bool HasCamera => HasComponent(ComponentType.Camera);

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

	// ---- Adicionar componentes em runtime ----

	// texturePath vazio = sprite de cor sólida. O caminho é relativo ao projeto.
	public bool AddSprite(string? texturePath = null)
		=> Interop.KizuriNative.kz_entity_add_sprite(Handle, texturePath ?? string.Empty) != 0;

	public bool AddText(string text, float fontSize = 48f)
		=> Interop.KizuriNative.kz_entity_add_text(Handle, text, fontSize) != 0;

	public bool AddAudio(string clipPath, bool loop = false, bool playOnStart = true)
		=> Interop.KizuriNative.kz_entity_add_audio(Handle, clipPath, loop ? 1 : 0, playOnStart ? 1 : 0) != 0;

	public bool AddCamera(bool perspective3D = false)
		=> Interop.KizuriNative.kz_entity_add_camera(Handle, perspective3D ? 1 : 0) != 0;

	// ---- Mutação em runtime ----

	public bool SetSpriteTexture(string path)
		=> Interop.KizuriNative.kz_sprite_set_texture(Handle, path) != 0;

	public void SetSpriteColor(float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_sprite_set_color(Handle, r, g, b, a);

	public void SetText(string text) => Interop.KizuriNative.kz_text_set_content(Handle, text);
	public void SetTextSize(float size) => Interop.KizuriNative.kz_text_set_size(Handle, size);
	public void SetTextColor(float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_text_set_color(Handle, r, g, b, a);

	public bool PlayAudio() => Interop.KizuriNative.kz_audio_play(Handle) != 0;
	public bool StopAudio() => Interop.KizuriNative.kz_audio_stop(Handle) != 0;

	// ---- UI (espaço de tela; precisa de um pai com UICanvas pra renderizar) ----

	// Torna a entidade uma raiz de Canvas. Filhos com UIRect são desenhados em
	// espaço de tela (0,0 = centro, y pra cima); orthoSize = meia-altura.
	public bool AddUICanvas(float orthoSize = 10f)
		=> Interop.KizuriNative.kz_entity_add_ui_canvas(Handle, orthoSize) != 0;

	public bool AddUIRect(float x, float y, float w, float h, float r = 1f, float g = 1f, float b = 1f, float a = 1f)
		=> Interop.KizuriNative.kz_entity_add_ui_rect(Handle, x, y, w, h, r, g, b, a) != 0;

	public bool AddUIButton(float x, float y, float w, float h, float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_entity_add_ui_button(Handle, x, y, w, h, r, g, b, a) != 0;

	public bool AddUIText(string text, float fontSize = 0.6f, float r = 1f, float g = 1f, float b = 1f, float a = 1f)
		=> Interop.KizuriNative.kz_entity_add_ui_text(Handle, text, fontSize, r, g, b, a) != 0;

	public bool UIButtonWasClicked() => Interop.KizuriNative.kz_ui_button_was_clicked(Handle) != 0;
	public bool UIButtonIsHovered() => Interop.KizuriNative.kz_ui_button_is_hovered(Handle) != 0;

	public void SetUIRect(float x, float y, float w, float h)
		=> Interop.KizuriNative.kz_ui_set_rect(Handle, x, y, w, h);

	public void SetUIColor(float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_ui_set_color(Handle, r, g, b, a);
}

public enum ComponentType
{
	Transform = 0,
	Rigidbody2D = 1,
	Sprite = 2,
	Text = 3,
	Audio = 4,
	Camera = 5,
	Light = 6,
	UIRect = 7,
	UIButton = 8,
	UICanvas = 9,
}