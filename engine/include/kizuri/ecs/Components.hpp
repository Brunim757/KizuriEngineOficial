#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/TextRenderer.hpp" // TextAlignment (TextComponent)
#include "kizuri/audio/AudioEngine.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>
#include <string>
#include <functional>
#include <vector>

namespace kizuri {

// Identidade estável da entidade. Toda entidade criada pela Scene tem um
// IDComponent — é a base para hierarquia, prefabs e serialização.
struct IDComponent {
    UUID ID;
};

struct TagComponent {
    std::string Tag;
};

// Relação pai/filho da entidade na cena. Guarda UUIDs em vez de
// entt::entity/ponteiros diretos porque handles do EnTT podem ser
// reciclados — UUID é a única referência que sobrevive a destruir e
// recriar entidades, e é o que vai pro disco em .kzscene/.kzprefab.
struct RelationshipComponent {
    UUID Parent = UUID::Invalid();
    std::vector<UUID> Children;
};

struct TransformComponent {
    glm::vec3 Translation = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Rotation = { 0.0f, 0.0f, 0.0f }; // euler, radianos
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
    std::string TexturePath; // serializável — caminho do arquivo de imagem (vazio = cor sólida)
    int SortingLayer = 0;    // ordem de desenho 2D: menor desenha antes (atrás)
};

struct CircleRendererComponent {
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float Thickness = 1.0f;
    float Fade = 0.005f;
    int SortingLayer = 0;
};

// Texto 2D de jogo (HUD, pontuação, diálogo). Renderizado pelo
// TextRenderer com a fonte embutida (JetBrains Mono). Suporta multilinha
// ('\n' quebra a linha) e alinhamento horizontal (ver TextAlignment).
struct TextComponent {
    std::string Text = "Texto";
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    float FontSize = 48.0f;  // altura em pixels de tela
    TextAlignment Alignment = TextAlignment::Left;
    int SortingLayer = 0;
};

// Animação de sprite 2D por frames numa folha de sprites (sprite sheet).
// FrameAtual/Contador são estado runtime — a folha é amostrada com UVs
// recortadas pela posição do frame atual.
struct SpriteAnimationComponent {
    std::string SheetPath;         // serializável
    Ref<Texture2D> SheetTexture;   // runtime (carregada sob demanda)
    uint32_t FramesPerRow = 1;     // quantos frames por linha na folha
    uint32_t TotalFrames = 1;
    float FPS = 12.0f;
    bool Loop = true;
    int SortingLayer = 0;

    uint32_t CurrentFrame = 0;     // runtime
    float FrameTimer = 0.0f;       // runtime
    bool Playing = true;
};

// Tilemap 2D: grade de índices de tile apontando pra um atlas de textura
// (AtlasPath). Tiles=0 é vazio; o resto é (tile-1) na grade do atlas.
struct TilemapComponent {
    std::string AtlasPath;         // serializável
    Ref<Texture2D> AtlasTexture;   // runtime (carregada sob demanda)
    uint32_t AtlasColumns = 1;     // tiles por linha no atlas
    uint32_t AtlasRows = 1;        // tiles por coluna no atlas (total = Colunas*Linhas)
    uint32_t MapWidth = 0, MapHeight = 0;
    glm::vec2 TileSize = { 1.0f, 1.0f };
    std::vector<uint32_t> Tiles;   // serializável — row-major, 0 = vazio
    int SortingLayer = 0;

    // Valores de tile (1-based, os mesmos usados em Tiles) que geram
    // collider estático Box2D no Play — a base pra níveis de platformer.
    std::vector<uint32_t> SolidTileValues; // serializável
};

struct MeshRendererComponent {
    Ref<Mesh> MeshAsset;
    Material MeshMaterial;
    std::string MeshSource; // serializável: "builtin:cube|plane|sphere" ou caminho .obj/.glb/.gltf
};

// Espelha kizuri::Light (Renderer3D.hpp) — entidade de luz de verdade na cena,
// em vez do sol fixo/não editável que existia antes.
struct LightComponent {
    LightType Type = LightType::Directional;
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerConeDeg = 20.0f;
    float OuterConeDeg = 30.0f;
};

// Estado runtime de uma partícula viva — não serializado, some ao fechar/recarregar a cena.
struct Particle {
    glm::vec3 Position{ 0.0f };
    glm::vec3 Velocity{ 0.0f };
    float Age = 0.0f;
    float Lifetime = 1.0f;
};

// Emissor de partículas GPU-instanced (ver Renderer3D::SubmitParticles). Só simula durante
// Play (OnUpdateRuntime) — mesma convenção da física, que também não roda em modo edição.
struct ParticleSystemComponent {
    bool Playing = true;
    bool Additive = true; // false = alpha blend normal (fumaça); true = aditivo (fogo/faísca/magia)
    float EmissionRate = 30.0f; // partículas/segundo
    uint32_t MaxParticles = 500;
    float LifetimeMin = 0.6f, LifetimeMax = 1.2f;
    glm::vec3 VelocityMin = { -0.5f, 1.5f, -0.5f };
    glm::vec3 VelocityMax = { 0.5f, 3.0f, 0.5f };
    glm::vec3 Gravity = { 0.0f, -2.0f, 0.0f };
    glm::vec4 StartColor = { 1.0f, 0.65f, 0.15f, 1.0f };
    glm::vec4 EndColor = { 1.0f, 0.15f, 0.02f, 0.0f };
    float StartSize = 0.15f, EndSize = 0.4f;

    std::vector<Particle> ActiveParticles; // estado runtime — não serializado
    float EmissionAccumulator = 0.0f;
};

// Emissor de som ligado a um arquivo — usa o AudioEngine (miniaudio por baixo, ver
// kizuri/audio/AudioEngine.hpp). Só toca durante Play (OnUpdateRuntime), mesma convenção de
// física/partículas. Sem trigger por evento ainda (colisão, script) — só PlayOnStart nessa v1.
struct AudioSourceComponent {
    std::string ClipPath;
    bool Loop = false;
    bool PlayOnStart = true;
    bool Spatial = true; // atenuação por distância + panorâmica 3D; false = volume/pan fixos (música/UI)
    float Volume = 1.0f;
    float MinDistance = 1.0f, MaxDistance = 50.0f;

    SoundHandle Handle = kInvalidSound; // estado runtime — não serializado
    bool HasStarted = false;

    // API de evento: toca/para agora, independente de PlayOnStart — chamável
    // por script (NativeScript), colisão ou código de jogo. Implementadas em
    // Components.cpp (precisam do AudioEngine).
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

// ---------------------------------------------------------------------------
// UI (sistema de interface em espaço de tela, estilo Canvas)
// ---------------------------------------------------------------------------
// Canvas: entidade raiz que renderiza os DESCENDENTES com UIRectComponent em
// espaço de tela. A projeção é centrada em (0,0) com y pra cima; OrthoSize é
// a meia-altura em unidades de UI (0,0 = centro da tela).
struct UICanvasComponent {
    float OrthoSize = 10.0f;
};

// Retângulo de UI: posiciona um elemento em espaço de tela (centro em
// Position, tamanho em Size). Color é o fundo (a=0 desenha só o texto).
struct UIRectComponent {
    glm::vec2 Position = { 0.0f, 0.0f };
    glm::vec2 Size = { 1.0f, 1.0f };
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// Botão interativo: faz um UIRect responder a hover/clique. O estado
// (Hovered/Pressed/WasClicked) é runtime — preenchido pelo Scene::UpdateUIPointer
// e consumido por scripts via o ABI kz_ui_button_*. Não é serializado.
struct UIButtonComponent {
    bool Hovered = false;
    bool Pressed = false;
    bool WasClicked = false; // clique com o mouse em cima (borda de subida)
};

struct Rigidbody2DComponent {
    enum class BodyType { Static = 0, Dynamic, Kinematic };
    BodyType Type = BodyType::Dynamic;
    bool FixedRotation = false;
    void* RuntimeBody = nullptr; // b2Body*

    // API de física pro script — só tem efeito durante o Play, quando
    // RuntimeBody já foi criado. Fica aqui (não no script) porque o b2Body
    // é interno; scripts não precisam linkar Box2D.
    void ApplyLinearImpulse(const glm::vec2& impulse, bool wake = true);
    void SetLinearVelocity(const glm::vec2& velocity);
    glm::vec2 GetLinearVelocity() const;
    // Sincroniza o corpo com Translation/Rotation.z do Transform (útil
    // pra Kinematic, ou pra teleportar um Dynamic sem lutar com o Step).
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
    void* RuntimeBody = nullptr; // btRigidBody*
};

struct BoxCollider3DComponent {
    glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
};

struct SphereCollider3DComponent {
    float Radius = 0.5f;
};

// Componente de script nativo em C++ — esta é a "primeira geração" do que
// será a KZScript. O jogo herda de NativeScript e sobrescreve os callbacks.
class NativeScript;

struct NativeScriptComponent {
    NativeScript* Instance = nullptr;

    // Nome da classe registrada no ScriptRegistry (ver
    // kizuri/scripting/ScriptEngine.hpp) — só preenchido quando o
    // vínculo veio de BindByName() (o caminho que o editor usa). Fica
    // vazio quando o vínculo veio de Bind<T>() em tempo de compilação
    // (o caminho que código de jogo escrito direto em C++ usa hoje).
    // É esse campo que a serialização grava em .kzscene.
    std::string ClassName;

    std::function<NativeScript*()> InstantiateScript;
    std::function<void(NativeScriptComponent*)> DestroyScript;

    template<typename T>
    void Bind() {
        ClassName.clear();
        InstantiateScript = [] { return static_cast<NativeScript*>(new T()); };
        DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
    }

    // Vincula por nome via o ScriptRegistry do GameModule carregado — é o
    // caminho usado pelo editor, que nunca conhece o tipo C++ do script em
    // tempo de compilação, só o nome escolhido no dropdown do Inspetor.
    // Implementado em Components.cpp (não aqui) pra não criar um ciclo de
    // include com NativeScript.hpp/ScriptEngine.hpp.
    void BindByName(const std::string& className);
};

} // namespace kizuri
