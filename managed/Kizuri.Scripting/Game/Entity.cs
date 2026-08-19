
namespace Kizuri;

public struct Transform
{
	public Math.Vector3 Translation;
	public Math.Vector3 Rotation; 
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

	
	
	
	public bool Active
	{
		get => Interop.KizuriNative.kz_entity_is_active(Handle) != 0;
		set => Interop.KizuriNative.kz_entity_set_active(Handle, value ? 1 : 0);
	}

	
	public string Name
	{
		get
		{
			var sb = new System.Text.StringBuilder(128);
			Interop.KizuriNative.kz_entity_get_name(Handle, sb, sb.Capacity);
			return sb.ToString();
		}
		set => Interop.KizuriNative.kz_entity_set_name(Handle, value);
	}

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
		t.Translation = pos;
		t.Rotation = new Math.Vector3(rot.X * K_RAD2DEG, rot.Y * K_RAD2DEG, rot.Z * K_RAD2DEG); 
		t.Scale = scale;
		return true;
	}

	public bool TryGetRigidbody2D(out Rigidbody2D rb)
	{
		rb = default;
		if (Interop.KizuriNative.kz_entity_get_rigidbody2d(Handle, out var bodyType, out var velocity) == 0) return false;
		rb = new Rigidbody2D { Handle = Handle, m_Type = (BodyType)bodyType, Velocity = velocity };
		return true;
	}

	
	public Rigidbody2D GetRigidbody2D()
	{
		TryGetRigidbody2D(out var rb);
		return rb;
	}

	
	public Rigidbody2D Rigidbody2D => GetRigidbody2D();

	public void SetPosition(Math.Vector3 position)
		=> Interop.KizuriNative.kz_transform_set_position(Handle, position.X, position.Y, position.Z);

	
	
	public void SetWorldPosition(Math.Vector3 position)
		=> Interop.KizuriNative.kz_entity_set_world_position(Handle, position.X, position.Y, position.Z);

	
	public Math.Vector3 Position
	{
		get
		{
			if (Interop.KizuriNative.kz_entity_get_position(Handle, out var pos) != 0) return pos;
			return Math.Vector3.Zero;
		}
	}

	

	
	public Entity Parent
	{
		get
		{
			if (Interop.KizuriNative.kz_entity_get_parent(Handle, out uint p) != 0) return new Entity(p);
			return Entity.Invalid;
		}
	}

	
	public int ChildCount => Interop.KizuriNative.kz_entity_get_child_count(Handle);

	
	public Entity GetChild(int index)
	{
		if (Interop.KizuriNative.kz_entity_get_child(Handle, index, out uint child) != 0) return new Entity(child);
		return Entity.Invalid;
	}

	
	public Entity GetChild(string name)
	{
		int count = ChildCount;
		for (int i = 0; i < count; ++i)
		{
			var c = GetChild(i);
			if (c.IsValid && c.Name == name) return c;
		}
		return Entity.Invalid;
	}

	
	public Entity FindInChildren(string name)
	{
		int count = ChildCount;
		for (int i = 0; i < count; ++i)
		{
			var c = GetChild(i);
			if (!c.IsValid) continue;
			if (c.Name == name) return c;
			var found = c.FindInChildren(name);
			if (found.IsValid) return found;
		}
		return Entity.Invalid;
	}

	
	
	
	const float K_DEG2RAD = (float)(System.Math.PI / 180.0);
	const float K_RAD2DEG = (float)(180.0 / System.Math.PI);

	public void SetRotation(Math.Vector3 degrees)
		=> Interop.KizuriNative.kz_transform_set_rotation(Handle,
			degrees.X * K_DEG2RAD, degrees.Y * K_DEG2RAD, degrees.Z * K_DEG2RAD);

	
	public void Rotate(Math.Vector3 degrees)
	{
		var r = Rotation;
		SetRotation(r + degrees);
	}

	
	public void Translate(Math.Vector3 delta)
	{
		var p = Position;
		SetPosition(p + delta);
	}

	public void SetScale(Math.Vector3 scale)
		=> Interop.KizuriNative.kz_transform_set_scale(Handle, scale.X, scale.Y, scale.Z);

	
	
	public Math.Vector3 Rotation
	{
		get
		{
			if (Interop.KizuriNative.kz_entity_get_rotation(Handle, out var rot) != 0)
				return new Math.Vector3(rot.X * K_RAD2DEG, rot.Y * K_RAD2DEG, rot.Z * K_RAD2DEG);
			return Math.Vector3.Zero;
		}
	}

	public Math.Vector3 Scale
	{
		get
		{
			if (Interop.KizuriNative.kz_entity_get_scale(Handle, out var s) != 0) return s;
			return Math.Vector3.One;
		}
	}

	
	public bool TryGetWorldPosition(out Math.Vector3 position)
	{
		position = Math.Vector3.Zero;
		return Interop.KizuriNative.kz_entity_get_world_position(Handle, out position.X, out position.Y, out position.Z) != 0;
	}

	public void LookAt(Math.Vector3 target)
		=> Interop.KizuriNative.kz_entity_look_at(Handle, target.X, target.Y, target.Z);

	
	
	public Math.Vector3 GetForward()
	{
		return GetForwardFromDeg(Rotation);
	}

	internal static Math.Vector3 GetForwardFromDeg(Math.Vector3 r)
	{
		float cy = (float)System.Math.Cos(r.Y * K_DEG2RAD), sy = (float)System.Math.Sin(r.Y * K_DEG2RAD);
		float cp = (float)System.Math.Cos(r.X * K_DEG2RAD), sp = (float)System.Math.Sin(r.X * K_DEG2RAD);
		return new Math.Vector3(cy * cp, sp, sy * cp);
	}

	
	public Math.Vector3 GetRight()
	{
		var r = Rotation;
		float sy = (float)System.Math.Sin(r.Y * K_DEG2RAD), cy = (float)System.Math.Cos(r.Y * K_DEG2RAD);
		return new Math.Vector3(-sy, 0f, cy);
	}

	
	public void MoveForward(float distance)
	{
		var f = GetForward();
		SetPosition(Position + new Math.Vector3(f.X * distance, f.Y * distance, f.Z * distance));
	}

	public void MoveRight(float distance)
	{
		var r = GetRight();
		SetPosition(Position + new Math.Vector3(r.X * distance, 0f, r.Z * distance));
	}

	
	public void SetParent(Entity parent)
		=> Interop.KizuriNative.kz_entity_set_parent(Handle, parent.Handle);

	public void SetParent()
		=> Interop.KizuriNative.kz_entity_set_parent(Handle, 0);

	public void Destroy() => Interop.KizuriNative.kz_scene_destroy_entity(Handle);

	

	
	public bool AddSprite(string? texturePath = null)
		=> Interop.KizuriNative.kz_entity_add_sprite(Handle, texturePath ?? string.Empty) != 0;

	public bool AddText(string text, float fontSize = 48f)
		=> Interop.KizuriNative.kz_entity_add_text(Handle, text, fontSize) != 0;

	public bool AddAudio(string clipPath, bool loop = false, bool playOnStart = true)
		=> Interop.KizuriNative.kz_entity_add_audio(Handle, clipPath, loop ? 1 : 0, playOnStart ? 1 : 0) != 0;

	public bool AddCamera(bool perspective3D = false)
		=> Interop.KizuriNative.kz_entity_add_camera(Handle, perspective3D ? 1 : 0) != 0;

	
	public void SetCamera(float fovDeg, float nearClip = 0.01f, float farClip = 1000f)
		=> Interop.KizuriNative.kz_camera_set_params(Handle, fovDeg, nearClip, farClip);

	
	
	
	public bool AddCameraFollow(string targetName, Math.Vector3 offset = default, float smoothness = 8f)
	{
		bool ok = Interop.KizuriNative.kz_entity_add_camera_follow(Handle, targetName) != 0;
		if (!ok) return false;
		Interop.KizuriNative.kz_camera_follow_set_offset(Handle, offset.X, offset.Y, offset.Z);
		Interop.KizuriNative.kz_camera_follow_set_smoothness(Handle, smoothness);
		return true;
	}

	public void SetCameraFollowTarget(string targetName)
		=> Interop.KizuriNative.kz_camera_follow_set_target(Handle, targetName);

	public void SetCameraFollowOffset(Math.Vector3 offset)
		=> Interop.KizuriNative.kz_camera_follow_set_offset(Handle, offset.X, offset.Y, offset.Z);

	public void SetCameraFollowSmoothness(float smoothness)
		=> Interop.KizuriNative.kz_camera_follow_set_smoothness(Handle, smoothness);

	
	
	
	public bool AddLight(LightType type, float r = 1f, float g = 1f, float b = 1f,
	                     float intensity = 1f, float range = 10f,
	                     float innerConeDeg = 20f, float outerConeDeg = 30f,
	                     bool castsShadow = false)
		=> Interop.KizuriNative.kz_entity_add_light(Handle, (int)type, r, g, b, intensity, range, innerConeDeg, outerConeDeg, castsShadow ? 1 : 0) != 0;

	
	
	public bool AddMeshRenderer(string meshSource = "builtin:cube")
		=> Interop.KizuriNative.kz_entity_add_mesh_renderer(Handle, meshSource) != 0;

	
	
	public bool AddTerrain(uint segments = 64, float size = 100f, float heightScale = 5f, uint seed = 1)
		=> Interop.KizuriNative.kz_entity_add_terrain(Handle, segments, size, heightScale, seed) != 0;

	
	public bool RegenerateTerrain(uint segments, float size, float heightScale, uint seed)
		=> Interop.KizuriNative.kz_terrain_regenerate(Handle, segments, size, heightScale, seed) != 0;

	public void SetLightColor(float r, float g, float b)
		=> Interop.KizuriNative.kz_light_set_color(Handle, r, g, b);
	public void SetLightIntensity(float intensity)
		=> Interop.KizuriNative.kz_light_set_intensity(Handle, intensity);

	public void SetMaterial(float r, float g, float b, float metallic = 0f, float roughness = 0.5f)
	{
		Interop.KizuriNative.kz_material_set_albedo(Handle, r, g, b);
		Interop.KizuriNative.kz_material_set_metallic(Handle, metallic);
		Interop.KizuriNative.kz_material_set_roughness(Handle, roughness);
	}

	public bool SetMaterialAlbedoMap(string path)
		=> Interop.KizuriNative.kz_material_set_albedo_map(Handle, path) != 0;
	public bool SetMaterialNormalMap(string path)
		=> Interop.KizuriNative.kz_material_set_normal_map(Handle, path) != 0;
	public bool SetMaterialMetallicRoughnessMap(string path)
		=> Interop.KizuriNative.kz_material_set_metallic_roughness_map(Handle, path) != 0;
	public bool SetMaterialHeightMap(string path)
		=> Interop.KizuriNative.kz_material_set_height_map(Handle, path) != 0;
	public void SetMaterialHeightScale(float scale)
		=> Interop.KizuriNative.kz_material_set_height_scale(Handle, scale);

	
	
	
	public void AddCharacterController(float speed = 6f, float gravity = -20f)
		=> Interop.KizuriNative.kz_entity_add_character_controller(Handle, speed, gravity);
	public void MoveCharacter(float fwdX, float fwdZ)
		=> Interop.KizuriNative.kz_entity_move_character(Handle, fwdX, fwdZ);

	
	
	public bool AddAnimationBlend(string clipA, string clipB, float weight = 0f)
		=> Interop.KizuriNative.kz_entity_add_animation_blend(Handle, clipA, clipB, weight) != 0;
	public void SetAnimationBlend(float weight)
		=> Interop.KizuriNative.kz_animation_set_blend(Handle, weight);
	public bool AddTwoBoneIK(string root, string mid, string tip)
		=> Interop.KizuriNative.kz_entity_add_two_bone_ik(Handle, root, mid, tip) != 0;
	public void SetIKTarget(float x, float y, float z, float weight = 1f)
		=> Interop.KizuriNative.kz_ik_set_target(Handle, x, y, z, weight);

	public bool AddNavGrid(float originX, float originZ, uint width, uint depth, float cellSize)
		=> Interop.KizuriNative.kz_entity_add_nav_grid(Handle, originX, originZ, width, depth, cellSize) != 0;
	public bool AddNavObstacle(float halfX, float halfY, float halfZ)
		=> Interop.KizuriNative.kz_entity_add_nav_obstacle(Handle, halfX, halfY, halfZ) != 0;
	public bool AddNavAgent(float speed = 4f, float turnSpeed = 8f)
		=> Interop.KizuriNative.kz_entity_add_nav_agent(Handle, speed, turnSpeed) != 0;
	public void SetNavDestination(float x, float y, float z)
		=> Interop.KizuriNative.kz_navagent_set_destination(Handle, x, y, z);
	public void StopNavAgent()
		=> Interop.KizuriNative.kz_navagent_stop(Handle);
	public bool NavAgentHasPath()
		=> Interop.KizuriNative.kz_navagent_has_path(Handle) != 0;
	public float NavAgentRemainingDistance()
		=> Interop.KizuriNative.kz_navagent_remaining_distance(Handle);

	
	public void AddTimeline() => Interop.KizuriNative.kz_entity_add_timeline(Handle);
	public void PlayTimeline(bool play = true) => Interop.KizuriNative.kz_timeline_play(Handle, play ? 1 : 0);
	public void StopTimeline() => Interop.KizuriNative.kz_timeline_play(Handle, 0);
	public void SetTimelineTime(float t) => Interop.KizuriNative.kz_timeline_set_time(Handle, t);
	public void AddTimelineKeyframe(float time, Math.Vector3 position)
		=> Interop.KizuriNative.kz_timeline_add_keyframe(Handle, time, position.X, position.Y, position.Z);

	
	public int Layer
	{
		get => Interop.KizuriNative.kz_entity_get_layer(Handle);
		set => Interop.KizuriNative.kz_entity_set_layer(Handle, value);
	}
	public uint CollisionMask
	{
		get => Interop.KizuriNative.kz_entity_get_collision_mask(Handle);
		set => Interop.KizuriNative.kz_entity_set_collision_mask(Handle, value);
	}
	
	public void SetCollideWithLayers(params int[] layers)
	{
		uint mask = 0;
		foreach (var l in layers) mask |= (1u << l);
		CollisionMask = mask;
	}
	public void SetMaterialEmissive(float r, float g, float b, float strength = 1f)
		=> Interop.KizuriNative.kz_material_set_emissive(Handle, r, g, b, strength);

	

	
	public bool AddAnimator(string meshPath)
		=> Interop.KizuriNative.kz_entity_add_animator(Handle, meshPath) != 0;

	
	public bool PlayAnimation(string clipName)
		=> Interop.KizuriNative.kz_animator_play(Handle, clipName) != 0;

	public float AnimationTime => Interop.KizuriNative.kz_animator_get_time(Handle);
	public void SetAnimationTime(float time) => Interop.KizuriNative.kz_animator_set_time(Handle, time);
	public void SetAnimationSpeed(float speed) => Interop.KizuriNative.kz_animator_set_speed(Handle, speed);
	public void SetAnimationLoop(bool loop) => Interop.KizuriNative.kz_animator_set_loop(Handle, loop ? 1 : 0);
	public void SetAnimationPlaying(bool playing) => Interop.KizuriNative.kz_animator_set_playing(Handle, playing ? 1 : 0);

	
	public bool PlayAnimationState(string stateName, float crossfadeSeconds = 0.3f)
		=> Interop.KizuriNative.kz_animator_set_state(Handle, stateName, crossfadeSeconds) != 0;

	public string AnimationState => Interop.KizuriNative.kz_animator_get_state(Handle);

	

	
	public bool AddRigidbody3D(BodyType3D bodyType = BodyType3D.Dynamic, float mass = 1f)
		=> Interop.KizuriNative.kz_entity_add_rigidbody3d(Handle, (int)bodyType, mass) != 0;

	public bool AddBoxCollider3D(float hx = 0.5f, float hy = 0.5f, float hz = 0.5f)
		=> Interop.KizuriNative.kz_entity_add_box_collider3d(Handle, hx, hy, hz) != 0;

	public bool AddSphereCollider3D(float radius = 0.5f)
		=> Interop.KizuriNative.kz_entity_add_sphere_collider3d(Handle, radius) != 0;

	public bool ApplyForce(Math.Vector3 force)
		=> Interop.KizuriNative.kz_rigidbody3d_apply_force(Handle, force.X, force.Y, force.Z) != 0;

	public bool ApplyImpulse(Math.Vector3 impulse)
		=> Interop.KizuriNative.kz_rigidbody3d_apply_impulse(Handle, impulse.X, impulse.Y, impulse.Z) != 0;

	public bool TryGetVelocity(out Math.Vector3 velocity)
	{
		velocity = Math.Vector3.Zero;
		return Interop.KizuriNative.kz_rigidbody3d_get_linear_velocity(Handle, out velocity.X, out velocity.Y, out velocity.Z) != 0;
	}

	public void SetVelocity(Math.Vector3 velocity)
		=> Interop.KizuriNative.kz_rigidbody3d_set_linear_velocity(Handle, velocity.X, velocity.Y, velocity.Z);

	public bool ApplyTorque(Math.Vector3 torque)
		=> Interop.KizuriNative.kz_rigidbody3d_apply_torque(Handle, torque.X, torque.Y, torque.Z) != 0;

	public bool TryGetAngularVelocity(out Math.Vector3 angular)
	{
		angular = Math.Vector3.Zero;
		return Interop.KizuriNative.kz_rigidbody3d_get_angular_velocity(Handle, out angular.X, out angular.Y, out angular.Z) != 0;
	}

	public void SetAngularVelocity(Math.Vector3 angular)
		=> Interop.KizuriNative.kz_rigidbody3d_set_angular_velocity(Handle, angular.X, angular.Y, angular.Z);

	
	public void SetGravityScale3D(float scale)
		=> Interop.KizuriNative.kz_rigidbody3d_set_gravity_scale(Handle, scale);

	public void SetDamping3D(float linear, float angular)
		=> Interop.KizuriNative.kz_rigidbody3d_set_damping(Handle, linear, angular);

	public bool AddCircleCollider2D(float radius = 0.5f, float density = 1f, float friction = 0.5f, float restitution = 0f)
		=> Interop.KizuriNative.kz_entity_add_circle_collider2d(Handle, radius, density, friction, restitution) != 0;

	
	public bool SetParticleTexture(string path)
		=> Interop.KizuriNative.kz_particle_set_texture(Handle, path) != 0;

	
	public bool SetParticleRate(float rate) => Interop.KizuriNative.kz_particle_set_rate(Handle, rate) != 0;
	public bool SetParticleLifetime(float min, float max) => Interop.KizuriNative.kz_particle_set_lifetime(Handle, min, max) != 0;
	public bool SetParticleVelocity(Math.Vector3 min, Math.Vector3 max)
		=> Interop.KizuriNative.kz_particle_set_velocity(Handle, min.X, min.Y, min.Z, max.X, max.Y, max.Z) != 0;
	public bool SetParticleGravity(Math.Vector3 gravity)
		=> Interop.KizuriNative.kz_particle_set_gravity(Handle, gravity.X, gravity.Y, gravity.Z) != 0;
	public bool SetParticleColors(Math.Vector4 start, Math.Vector4 end)
		=> Interop.KizuriNative.kz_particle_set_colors(Handle, start.X, start.Y, start.Z, start.W, end.X, end.Y, end.Z, end.W) != 0;
	public bool SetParticleSize(float start, float end) => Interop.KizuriNative.kz_particle_set_size(Handle, start, end) != 0;
	
	public bool SetParticleAdditive(bool additive) => Interop.KizuriNative.kz_particle_set_additive(Handle, additive ? 1 : 0) != 0;

	
	public bool AddSpriteAnimation(string sheetPath, float fps = 12f, int totalFrames = 1,
	                               int framesPerRow = 1, bool loop = true)
		=> Interop.KizuriNative.kz_entity_add_sprite_animation(Handle, sheetPath, (int)fps, totalFrames, framesPerRow, loop ? 1 : 0) != 0;
	public void PlaySpriteAnimation(bool play = true) => Interop.KizuriNative.kz_sprite_animation_play(Handle, play ? 1 : 0);
	public void SetSpriteAnimationFPS(float fps) => Interop.KizuriNative.kz_sprite_animation_set_fps(Handle, fps);

	
	
	public bool AddTilemap(string atlasPath, int atlasCols, int atlasRows, int mapW, int mapH,
	                       float tileW = 1f, float tileH = 1f)
		=> Interop.KizuriNative.kz_entity_add_tilemap(Handle, atlasPath, atlasCols, atlasRows, mapW, mapH, tileW, tileH) != 0;
	
	public bool SetTile(int x, int y, int tileValue)
		=> Interop.KizuriNative.kz_tilemap_set_tile(Handle, x, y, tileValue) != 0;
	
	public bool AddSolidTile(int tileValue)
		=> Interop.KizuriNative.kz_tilemap_add_solid_tile(Handle, tileValue) != 0;

	
	
	public void SetSortingLayer(int layer)
		=> Interop.KizuriNative.kz_entity_set_sorting_layer(Handle, layer);

	

	public bool SetSpriteTexture(string path)
		=> Interop.KizuriNative.kz_sprite_set_texture(Handle, path) != 0;

	public void SetSpriteColor(float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_sprite_set_color(Handle, r, g, b, a);

	
	public void SetSpriteFlip(bool flipX, bool flipY)
		=> Interop.KizuriNative.kz_sprite_set_flip(Handle, flipX ? 1 : 0, flipY ? 1 : 0);

	
	public void SetGravityScale(float scale)
		=> Interop.KizuriNative.kz_rigidbody2d_set_gravity_scale(Handle, scale);

	public void SetText(string text) => Interop.KizuriNative.kz_text_set_content(Handle, text);
	public void SetTextSize(float size) => Interop.KizuriNative.kz_text_set_size(Handle, size);
	public void SetTextColor(float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_text_set_color(Handle, r, g, b, a);

	public bool PlayAudio() => Interop.KizuriNative.kz_audio_play(Handle) != 0;
	public bool StopAudio() => Interop.KizuriNative.kz_audio_stop(Handle) != 0;

	
	public void SetAudioVolume(float volume) => Interop.KizuriNative.kz_audio_set_volume(Handle, volume);
	public void SetSoundReverb(bool enabled)
		=> Interop.KizuriNative.kz_audio_set_reverb(Handle, enabled ? 1 : 0);

	public void SetAudioSpatial(bool spatial, float minDistance = 1f, float maxDistance = 50f)
		=> Interop.KizuriNative.kz_audio_set_spatial(Handle, spatial ? 1 : 0, minDistance, maxDistance);

	

	
	
	public bool AddUICanvas(float orthoSize = 10f)
		=> Interop.KizuriNative.kz_entity_add_ui_canvas(Handle, orthoSize) != 0;

	public bool AddUIRect(float x, float y, float w, float h, float r = 1f, float g = 1f, float b = 1f, float a = 1f)
		=> Interop.KizuriNative.kz_entity_add_ui_rect(Handle, x, y, w, h, r, g, b, a) != 0;

	public bool AddUIButton(float x, float y, float w, float h, float r, float g, float b, float a = 1f)
		=> Interop.KizuriNative.kz_entity_add_ui_button(Handle, x, y, w, h, r, g, b, a) != 0;

	public bool AddUIText(string text, float fontSize = 14f, float r = 1f, float g = 1f, float b = 1f, float a = 1f)
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
	CircleCollider2D = 10,
	MeshRenderer = 11,
	ParticleSystem = 12,
	Animator = 13,
	Rigidbody3D = 14,
	BoxCollider3D = 15,
	SphereCollider3D = 16,
}


public enum BodyType3D
{
	Static = 0,
	Dynamic = 1,
	Kinematic = 2,
}


public enum LightType
{
	Directional = 0,
	Point = 1,
	Spot = 2,
}