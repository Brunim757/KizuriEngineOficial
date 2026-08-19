#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/TextRenderer.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/ecs/Animator.hpp"
#include "kizuri/ai/NavGrid.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <functional>
#include <vector>
#include <memory>

namespace kizuri {

struct IDComponent {
    UUID ID;

    bool Active = true;
};

struct TagComponent {
    std::string Tag;

    int Layer = 0;

    uint32_t CollisionMask = 0xFFFFFFFFu;
};

struct RelationshipComponent {
    UUID Parent = UUID::Invalid();
    std::vector<UUID> Children;
};

struct TransformComponent {
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Scale = { 1.0f, 1.0f, 1.0f };

    glm::mat4 GetTransform() const {
        glm::mat4 rot = glm::mat4_cast(glm::quat(Rotation));
        return glm::translate(glm::mat4(1.0f), Translation) * rot * glm::scale(glm::mat4(1.0f), Scale);
    }
};

struct SpriteRendererComponent {
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    Ref<Texture2D> Texture;
    float TilingFactor = 1.0f;
    int SortingLayer = 0;
    std::string TexturePath;
    bool FlipX = false;
    bool FlipY = false;
};

struct CircleRendererComponent {
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float Thickness = 1.0f;
    float Fade = 0.005f;
    int SortingLayer = 0;
};

struct TextComponent {
    std::string Text = "Texto";
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float FontSize = 48.0f;
    TextAlignment Alignment = TextAlignment::Left;
    int SortingLayer = 0;
};

struct FoliageComponent {
    std::string MeshSource = "builtin:cone";
    glm::vec2 AreaSize = { 12.0f, 12.0f };
    float HeightScale = 1.0f;
    uint32_t Count = 200;
    float ScaleMin = 0.6f, ScaleMax = 1.3f;
    uint32_t Seed = 42;
    bool AvoidCenter = true;

    std::vector<glm::mat4> Instances;
    Ref<Mesh> MeshAsset;
    glm::vec4 Color = { 0.24f, 0.5f, 0.22f, 1.0f };

    void Regenerate();
};

struct OccluderComponent {
    glm::vec3 HalfExtents{ 0.0f };
    float MaxOcclusionDistance = 60.0f;
};

struct DecalComponent {
    std::string TexturePath;
    Ref<Texture2D> Texture;
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    int SortingLayer = 0;
};

struct SpriteAnimationComponent {
    std::string SheetPath;
    Ref<Texture2D> SheetTexture;
    uint32_t FramesPerRow = 1;
    uint32_t TotalFrames = 1;
    float FPS = 12.0f;
    bool Loop = true;
    int SortingLayer = 0;

    uint32_t CurrentFrame = 0;
    float FrameTimer = 0.0f;
    bool Playing = true;
};

struct TilemapComponent {
    std::string AtlasPath;
    Ref<Texture2D> AtlasTexture;
    uint32_t AtlasColumns = 1;
    uint32_t AtlasRows = 1;
    uint32_t MapWidth = 0, MapHeight = 0;
    glm::vec2 TileSize = { 1.0f, 1.0f };
    std::vector<uint32_t> Tiles;
    int SortingLayer = 0;

    std::vector<uint32_t> SolidTileValues;

    bool CollidersDirty = false;
};

struct MeshRendererComponent {
    Ref<Mesh> MeshAsset;
    Material MeshMaterial;
    std::string MeshSource;

    Ref<Texture2D> LightmapTexture;
    std::string LightmapPath;
};

struct LODComponent {
    struct Level {
        std::string MeshSource;
        float Distance = 50.0f;
        Ref<Mesh> MeshAsset;
    };
    std::vector<Level> Levels;
    float DistanceMultiplier = 1.0f;
};

struct TerrainComponent {
    uint32_t Segments = 64;
    float Size = 100.0f;
    float HeightScale = 5.0f;
    uint32_t Seed = 1;
    Ref<Mesh> GeneratedMesh;

    std::vector<float> Heightmap;

    void Regenerate() {
        if (Heightmap.size() >= (size_t)(Segments + 1) * (Segments + 1))
            GeneratedMesh = Mesh::CreateTerrainFromHeightmap(Segments, Size, Heightmap);
        else
            GeneratedMesh = Mesh::CreateTerrain(Segments, Size, HeightScale, Seed);
    }
};

struct CharacterControllerComponent {
    float Speed = 6.0f;
    float Gravity = -20.0f;
    float Radius = 0.4f;
    float Height = 1.8f;
    float StepOffset = 0.3f;
    bool Grounded = false;
    glm::vec2 Input = { 0.0f, 0.0f };
    glm::vec3 Velocity{ 0.0f };
};

struct TimelineComponent {
    struct Keyframe {
        float Time = 0.0f;
        glm::vec3 Position{ 0.0f };
        glm::vec3 Rotation{ 0.0f };
        glm::vec3 Scale{ 1.0f };
    };
    std::vector<Keyframe> Keyframes;
    bool Playing = true;
    bool Loop = true;
    float Time = 0.0f;
    float Speed = 1.0f;

    float Duration() const {
        float d = 0.0f;
        for (auto& k : Keyframes) d = std::max(d, k.Time);
        return d;
    }
};

struct LightComponent {
    LightType Type = LightType::Directional;
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerConeDeg = 20.0f;
    float OuterConeDeg = 30.0f;
    bool CastsShadow = false;
};

struct Particle {
    glm::vec3 Position{ 0.0f };
    glm::vec3 Velocity{ 0.0f };
    float Age = 0.0f;
    float Lifetime = 1.0f;
};

struct ParticleSystemComponent {
    bool Playing = true;
    bool Additive = true;
    float EmissionRate = 30.0f;
    uint32_t MaxParticles = 500;
    float LifetimeMin = 0.6f, LifetimeMax = 1.2f;
    glm::vec3 VelocityMin = { -0.5f, 1.5f, -0.5f };
    glm::vec3 VelocityMax = { 0.5f, 3.0f, 0.5f };
    glm::vec3 Gravity = { 0.0f, -2.0f, 0.0f };
    glm::vec4 StartColor = { 1.0f, 0.65f, 0.15f, 1.0f };
    glm::vec4 EndColor = { 1.0f, 0.15f, 0.02f, 0.0f };
    float StartSize = 0.15f, EndSize = 0.4f;

    std::string TexturePath;
    Ref<Texture2D> Texture;

    std::vector<Particle> ActiveParticles;
    float EmissionAccumulator = 0.0f;
};

struct AudioSourceComponent {
    std::string ClipPath;
    bool Loop = false;
    bool PlayOnStart = true;
    bool Spatial = true;
    float Volume = 1.0f;
    float MinDistance = 1.0f, MaxDistance = 50.0f;
    int Group = 0;
    bool Reverb = false;

    SoundHandle Handle = kInvalidSound;
    bool HasStarted = false;

    void Play();
    void Stop();
    bool IsPlaying() const;
};

struct CameraComponent {
    enum class ProjectionType { Orthographic2D = 0, Perspective3D = 1 };
    ProjectionType Type = ProjectionType::Orthographic2D;

    float OrthoSize = 10.0f;
    float PerspectiveFOV = 45.0f;
    float NearClip = 0.01f;
    float FarClip = 1000.0f;

    bool Primary = true;
    bool FixedAspectRatio = false;
};

struct CameraFollowComponent {
    std::string TargetName;
    glm::vec3 Offset = { 0.0f, 3.0f, -6.0f };
    float Smoothness = 8.0f;
    bool FollowRotation = true;
    bool UseWorldOffset = false;
    glm::vec3 m_CurrentPos = { 0.0f, 3.0f, -6.0f };
    bool m_HasStart = false;
};

struct AnimatorComponent {
    std::string MeshPath;
    std::string ClipName;
    bool Playing = true;
    bool Loop = true;
    float Speed = 1.0f;
    float Time = 0.0f;

    Ref<SkinData> Skin;

    bool HasClip(const std::string& name) const { return Skin && Skin->GetClipIndex(name) >= 0; }
    void Play(const std::string& name) {
        if (HasClip(name)) { ClipName = name; Time = 0.0f; Playing = true; }
    }
};

struct AnimStateDef {
    std::string Name;
    std::string Clip;
    float Speed = 1.0f;
    bool Loop = true;
};

struct AnimTransitionDef {
    int From = -1;
    int To = 0;
    float BlendTime = 0.3f;
};

struct AnimatorStateMachineComponent {
    std::vector<AnimStateDef> States;
    std::vector<AnimTransitionDef> Transitions;

    int CurrentState = -1;
    int m_TransitionFrom = -1;
    float m_TransitionTime = 0.0f;
    float m_TransitionDuration = 0.3f;

    bool SetState(const std::string& name, float defaultBlend = 0.3f);
    bool IsInState(const std::string& name) const;
};

struct AnimationBlendComponent {
    std::string ClipA;
    std::string ClipB;
    float BlendWeight = 0.0f;
    bool UseBlend = true;
};

struct TwoBoneIKComponent {
    std::string RootBone;
    std::string MidBone;
    std::string TipBone;
    glm::vec3 Target{ 0.0f };
    float Weight = 1.0f;
};

struct UICanvasComponent {
    float OrthoSize = 10.0f;
};

struct UIRectComponent {
    glm::vec2 Position = { 0.0f, 0.0f };
    glm::vec2 Size = { 1.0f, 1.0f };
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

struct UIButtonComponent {
    bool Hovered = false;
    bool Pressed = false;
    bool WasClicked = false;
};

struct Rigidbody2DComponent {
    enum class BodyType { Static = 0, Dynamic, Kinematic };
    BodyType Type = BodyType::Dynamic;
    bool FixedRotation = false;
    float GravityScale = 1.0f;
    void* RuntimeBody = nullptr;

    void ApplyLinearImpulse(const glm::vec2& impulse, bool wake = true);
    void ApplyForce(const glm::vec2& force, bool wake = true);
    void SetLinearVelocity(const glm::vec2& velocity);
    glm::vec2 GetLinearVelocity() const;
    float GetAngularVelocity() const;
    void SetAngularVelocity(float w);
    void SetFixedRotation(bool fixed);

    void SetTransform(const glm::vec2& position, float angleRadians);
};

struct BoxCollider2DComponent {
    glm::vec2 Offset = { 0.0f, 0.0f };
    glm::vec2 Size = { 0.5f, 0.5f };
    float Density = 1.0f;
    float Friction = 0.5f;
    float Restitution = 0.0f;
    float RestitutionThreshold = 0.5f;
};

struct CircleCollider2DComponent {
    glm::vec2 Offset = { 0.0f, 0.0f };
    float Radius = 0.5f;
    float Density = 1.0f;
    float Friction = 0.5f;
    float Restitution = 0.0f;
};

struct Rigidbody3DComponent {
    enum class BodyType { Static = 0, Dynamic, Kinematic };
    BodyType Type = BodyType::Dynamic;
    float Mass = 1.0f;

    float GravityScale = 1.0f;
    float LinearDamping = 0.0f;
    float AngularDamping = 0.0f;
    void* RuntimeBody = nullptr;
};

struct BoxCollider3DComponent {
    glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
};

struct SphereCollider3DComponent {
    float Radius = 0.5f;
};

struct MeshColliderComponent {
    std::string MeshPath;
    uint32_t MaxPoints = 96;
};

class NativeScript;

struct NativeScriptComponent {
    NativeScript* Instance = nullptr;

    std::string ClassName;

    std::function<NativeScript*()> InstantiateScript;
    std::function<void(NativeScriptComponent*)> DestroyScript;

    void DestroyInstance();

    template<typename T>
    void Bind() {
        ClassName.clear();
        InstantiateScript = [] { return static_cast<NativeScript*>(new T()); };
        DestroyScript = [](NativeScriptComponent* nsc) { nsc->DestroyInstance(); };
    }

    void BindByName(const std::string& className);
};

struct NavGridComponent {

    glm::vec3 Origin{ 0.0f };
    uint32_t Width = 40;
    uint32_t Depth = 40;
    float CellSize = 1.0f;
    bool AutoBuild = true;

    std::shared_ptr<kizuri::NavGrid> Grid;
};

struct NavObstacleComponent {
    glm::vec3 HalfExtents{ 0.0f };
};

struct NavAgentComponent {
    float Speed = 4.0f;
    float TurnSpeed = 8.0f;
    float StopDistance = 0.3f;
    float Radius = 0.3f;
    bool FaceMovement = true;
    bool Enabled = true;

    glm::vec3 Destination{ 0.0f };
    bool HasDestination = false;
    std::vector<glm::vec3> Path;
    size_t PathIndex = 0;
    float PathTimer = 0.0f;
};

struct EnemyAIComponent {
    enum class State { Patrol = 0, Chase = 1, Attack = 2 };

    State InitialState = State::Patrol;
    float SightRange = 12.0f;
    float LoseRange = 18.0f;
    float ChaseRange = 8.0f;
    float AttackCooldown = 1.2f;
    float AttackDamage = 1.0f;
    float PatrolWait = 1.5f;
    std::vector<glm::vec3> PatrolPoints;
    std::string TargetTag = "Jogador";

    State m_State = InitialState;
    float m_StateTimer = 0.0f;
    float m_PatrolTimer = 0.0f;
    int m_PatrolIndex = 0;
    bool m_HasTarget = false;
    uint32_t m_TargetHandle = 0;
};

}
