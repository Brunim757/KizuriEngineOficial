#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/core/UUID.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/TextRenderer.hpp" // TextAlignment (TextComponent)
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

// Identidade estável da entidade. Toda entidade criada pela Scene tem um
// IDComponent — é a base para hierarquia, prefabs e serialização.
struct IDComponent {
    UUID ID;
    // Ativo/inativo (estilo GameObject.SetActive). Entidade inativa não é
    // desenhada nem atualizada (animações/timeline/scripts), e os filhos
    // herdam o estado (uma entidade é ativa só se ela E todos os pais forem).
    bool Active = true;
};

struct TagComponent {
    std::string Tag;
    // Camada de colisão (0..15): categoriza a entidade pra filtro de física.
    int Layer = 0;
    // Quais camadas colidem com esta (bitmask; bit N = camada N). Padrão:
    // todas (0xFFFFFFFF). Para filtrar, desligue os bits que NÃO devem colidir.
    uint32_t CollisionMask = 0xFFFFFFFFu;
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
    int SortingLayer = 0;
    std::string TexturePath; // serializável
    bool FlipX = false; // inverte horizontalmente no espaço local
    bool FlipY = false;
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
// Decal (pilar AAA v0.35): textura projetada na cena por uma caixa
// orientada — a escala do transform é o tamanho do decal; a projeção sai
// pelo eixo Z local (gire o decal pra encostar na superfície desejada).
// Usado pra sujeira, sangue, marcas de pneu, luzes de neon, poças...
// Occluder (pilar AAA v0.35): caixa sólida que BLOQUEIA a visão de tudo
// que estiver atrás dela — o renderer descarta objetos totalmente ocultos
// por ela (culling de oclusão por projeção em tela).
// Vegetação/foliage (pilar AAA v0.35): espalha N instâncias de uma malha
// numa área ao redor da origem (determinístico pela seed — mesmo resultado
// toda vez que regenera). Renderizado com instancing (1 draw call pra
// centenas de árvores/grama).
struct FoliageComponent {
    std::string MeshSource = "builtin:cone"; // árvore/arbusto/grama
    glm::vec2 AreaSize = { 12.0f, 12.0f };   // extensão XZ
    float HeightScale = 1.0f;                // altura do tronco (cone)
    uint32_t Count = 200;
    float ScaleMin = 0.6f, ScaleMax = 1.3f;
    uint32_t Seed = 42;
    bool AvoidCenter = true;                 // deixa um raio vazio na origem

    // Runtime — regenerado sob demanda.
    std::vector<glm::mat4> Instances;
    Ref<Mesh> MeshAsset;
    glm::vec4 Color = { 0.24f, 0.5f, 0.22f, 1.0f };

    void Regenerate();
};

struct OccluderComponent {
    glm::vec3 HalfExtents{ 0.0f }; // 0 = usa a escala do transform
    float MaxOcclusionDistance = 60.0f; // só oclui alvos até essa distância
};

struct DecalComponent {
    std::string TexturePath;     // serializável
    Ref<Texture2D> Texture;      // runtime (carregada sob demanda)
    glm::vec4 Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    int SortingLayer = 0;
};

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

    // Rebuilda os colliders no próximo frame (mudou tile/sólidos em runtime).
    bool CollidersDirty = false; // runtime — não serializado
};

struct MeshRendererComponent {
    Ref<Mesh> MeshAsset;
    Material MeshMaterial;
    std::string MeshSource; // serializável: "builtin:cube|plane|sphere" ou caminho .obj/.glb/.gltf

    // Lightmap assada (pilar AAA v0.35): textura por-énite multiplicada no
    // fragment shader (AO + ambience + sol direto). Runtime + serializada.
    Ref<Texture2D> LightmapTexture;
    std::string LightmapPath; // serializável
};

// LOD (Level of Detail): várias versões da mesma malha, trocadas pela
// distância à câmera. Levels[0] = maior detalhe (usado de perto); o último
// nível cobre as distâncias maiores. Sem níveis = comportamento normal.
struct LODComponent {
    struct Level {
        std::string MeshSource; // serializável
        float Distance = 50.0f; // passa pra este nível a partir desta distância
        Ref<Mesh> MeshAsset;
    };
    std::vector<Level> Levels;
    float DistanceMultiplier = 1.0f; // escala todas as distâncias (tune)
};

// Terreno procedural: gera um mesh de heightmap (fbm) com Mesh::CreateTerrain.
// Requer um MeshRenderer na mesma entidade (o mesh gerado é usado no lugar).
// Com Heightmap preenchido (escultura do editor), a malha vem dele.
struct TerrainComponent {
    uint32_t Segments = 64;
    float Size = 100.0f;
    float HeightScale = 5.0f;
    uint32_t Seed = 1;
    Ref<Mesh> GeneratedMesh; // runtime — gerado sob demanda (não serializado)

    // Alturas explícitas ((Segments+1)², [i][j] = v[i*(Segments+1)+j], i=X).
    // Não serializado — a escultura do editor regenera a malha a cada pincel.
    std::vector<float> Heightmap;

    void Regenerate() {
        if (Heightmap.size() >= (size_t)(Segments + 1) * (Segments + 1))
            GeneratedMesh = Mesh::CreateTerrainFromHeightmap(Segments, Size, Heightmap);
        else
            GeneratedMesh = Mesh::CreateTerrain(Segments, Size, HeightScale, Seed);
    }
};

// Character controller (v1, cinemático): movimento horizontal pelo input
// (MoveCharacter), gravidade e detecção de chão por raycast 3D. Não colide
// com paredes (v1) — use um Rigidbody/collider para isso.
struct CharacterControllerComponent {
    float Speed = 6.0f;
    float Gravity = -20.0f;
    float Radius = 0.4f;
    float Height = 1.8f;
    float StepOffset = 0.3f;
    bool Grounded = false;             // runtime
    glm::vec2 Input = { 0.0f, 0.0f };  // runtime (MoveCharacter)
    glm::vec3 Velocity{ 0.0f };        // runtime
};

// Timeline (cutscene simples): keyframes de Transform (posição/rotação/escala)
// interpolados linearmente ao longo do tempo. Toca no runtime (e no preview
// do editor). Rotação em euler (graus).
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

// Espelha kizuri::Light (Renderer3D.hpp) — entidade de luz de verdade na cena,
// em vez do sol fixo/não editável que existia antes.
struct LightComponent {
    LightType Type = LightType::Directional;
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerConeDeg = 20.0f;
    float OuterConeDeg = 30.0f;
    bool CastsShadow = false; // Point/Spot: projeta sombra (depth cubemap)
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

    // Textura opcional da partícula (vazio = degradê radial procedural).
    std::string TexturePath;
    Ref<Texture2D> Texture;

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
    int Group = 0; // Audio mixer: 0=SFX, 1=Música, 2=UI
    bool Reverb = false; // envia pelo nó de reverb global (pilar AAA v0.34)

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

// Câmera que segue um alvo (estilo "câmera de jogo"): a entidade com este
// componente (e CameraComponent) se move suavemente atrás do alvo a cada
// frame. O alvo é referenciado pelo NOME (Tag) — resolvido em runtime.
struct CameraFollowComponent {
    std::string TargetName;            // nome do alvo (TagComponent)
    glm::vec3 Offset = { 0.0f, 3.0f, -6.0f }; // deslocamento da câmera
    float Smoothness = 8.0f;           // velocidade do lerp (0 = teleporta)
    bool FollowRotation = true;        // gira o yaw junto com o alvo
    bool UseWorldOffset = false;       // offset em espaço MUNDO (senão gira com o alvo)
    glm::vec3 m_CurrentPos = { 0.0f, 3.0f, -6.0f }; // estado interno (não serializado)
    bool m_HasStart = false;
};

// Animador esquelético: toca uma AnimationClip de um .glb/.gltf (skinning).
// MeshPath deve apontar pro mesmo arquivo do MeshRenderer. Skin/Time são
// estado runtime; o resto é serializado (a skin é reparseada no load).
struct AnimatorComponent {
    std::string MeshPath;  // fonte .glb/.gltf (mesma do MeshRenderer)
    std::string ClipName;  // clip atual (vazio = pose de repouso)
    bool Playing = true;
    bool Loop = true;
    float Speed = 1.0f;
    float Time = 0.0f;

    Ref<SkinData> Skin;    // runtime — não serializado

    bool HasClip(const std::string& name) const { return Skin && Skin->GetClipIndex(name) >= 0; }
    void Play(const std::string& name) {
        if (HasClip(name)) { ClipName = name; Time = 0.0f; Playing = true; }
    }
};

// Estado de animação (pilar AAA v0.35): um clip + velocidade + loop.
struct AnimStateDef {
    std::string Name;      // identificador usado pela API (PlayState("correr"))
    std::string Clip;      // clip da skin tocado neste estado
    float Speed = 1.0f;
    bool Loop = true;
};

// Transição entre estados: de From até To com crossfade de BlendTime segundos.
struct AnimTransitionDef {
    int From = -1;         // índice do estado de origem (-1 = qualquer)
    int To = 0;            // índice do estado de destino
    float BlendTime = 0.3f;
};

// Blend de animação (pilar AAA v0.34): mistura dois clips da MESMA skin por
// um peso 0..1 (ex: idle↔andando pelo speed). A mistura é feita nas matrizes
// globais (TRS: lerp posição, slerp rotação, lerp escala) ANTES do inverse
// bind — o mesmo resultado do crossfade dos motores AAA.
// Máquina de estados de animação (pilar AAA v0.35): Estados + Transições
// com crossfade — o estado atual define o clip; a transição mistura os dois
// clips pelo tempo de blend (reutiliza o caminho de blend da pose).
struct AnimatorStateMachineComponent {
    std::vector<AnimStateDef> States;
    std::vector<AnimTransitionDef> Transitions;

    int CurrentState = -1;            // runtime
    int m_TransitionFrom = -1;        // runtime
    float m_TransitionTime = 0.0f;    // runtime
    float m_TransitionDuration = 0.3f;// runtime

    // Toca um estado pelo nome; retorna true se achou. Crossfade se existir
    // transição (ou o blend padrão de 0.3s).
    bool SetState(const std::string& name, float defaultBlend = 0.3f);
    bool IsInState(const std::string& name) const;
};

struct AnimationBlendComponent {
    std::string ClipA;         // clip base (peso 0)
    std::string ClipB;         // clip alvo (peso 1)
    float BlendWeight = 0.0f;  // 0..1
    bool UseBlend = true;
};

// IK de dois ossos (pilar AAA v0.34): posiciona uma ponta (mão/pé) num alvo
// ajustando dois ossos intermediários (ombro↔cotovelo↔mão, quadril↔joelho↔pé)
// — solução analítica clássica de triângulo. Aplica-se DEPOIS do blend, nas
// matrizes globais; Weight 0..1 mistura com a pose original.
struct TwoBoneIKComponent {
    std::string RootBone;   // raiz do encadeamento (ombro/quadril)
    std::string MidBone;    // meio (cotovelo/joelho)
    std::string TipBone;    // ponta (mão/pé)
    glm::vec3 Target{ 0.0f }; // alvo em espaço MUNDO
    float Weight = 1.0f;
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
    float GravityScale = 1.0f; // <0 = gravidade invertida, 0 = sem gravidade
    void* RuntimeBody = nullptr; // b2Body*

    // API de física pro script — só tem efeito durante o Play, quando
    // RuntimeBody já foi criado. Fica aqui (não no script) porque o b2Body
    // é interno; scripts não precisam linkar Box2D.
    void ApplyLinearImpulse(const glm::vec2& impulse, bool wake = true);
    void ApplyForce(const glm::vec2& force, bool wake = true);
    void SetLinearVelocity(const glm::vec2& velocity);
    glm::vec2 GetLinearVelocity() const;
    float GetAngularVelocity() const;
    void SetAngularVelocity(float w);
    void SetFixedRotation(bool fixed);
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
    // <0 = gravidade invertida, 0 = sem gravidade (flutuação).
    float GravityScale = 1.0f;
    float LinearDamping = 0.0f;
    float AngularDamping = 0.0f;
    void* RuntimeBody = nullptr; // btRigidBody*
};

struct BoxCollider3DComponent {
    glm::vec3 HalfExtents = { 0.5f, 0.5f, 0.5f };
};

struct SphereCollider3DComponent {
    float Radius = 0.5f;
};

// Colisor 3D de MALHA (pilar AAA v0.34): envoltório convexo da malha do
// MeshRenderer da entidade (ou de MeshPath, se definido) — geometria de
// verdade, não caixa/esfera. Convexo só (o Bullet não faz concave; pra
// terreno/ambiente use o colisor de terreno ou várias caixas).
struct MeshColliderComponent {
    std::string MeshPath; // vazio = usa o MeshRenderer da própria entidade
    uint32_t MaxPoints = 96; // amostragem do hull (perf vs fidelidade)
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

// ---------------------------------------------------------------------------
// IA e Navegação (pilar AAA v0.34)
// ---------------------------------------------------------------------------

// Grade de navegação: região do mundo (XZ) que os agentes usam pra andar.
// Em runtime, o Scene desenha os NavObstacleComponent na grade e os
// NavAgentComponent consultam ela pra calcular caminhos (A*).
struct NavGridComponent {
    // Origem e tamanho da grade (quantidade de células).
    glm::vec3 Origin{ 0.0f };      // canto da grade (x, ?, z)
    uint32_t Width = 40;           // células em X
    uint32_t Depth = 40;           // células em Z
    float CellSize = 1.0f;         // tamanho de cada célula (mundo)
    bool AutoBuild = true;         // rasteriza os obstáculos no Play

    // Estado runtime — preenchido pelo Scene::BuildNavGrid.
    std::shared_ptr<kizuri::NavGrid> Grid;
};

// Obstáculo de navegação: o NavGridComponent rasteriza a AABB desta entidade
// (com HalfExtents opcional; vazio = usa a escala do transform) como células
// bloqueadas. Não muda a física — só a navegação.
struct NavObstacleComponent {
    glm::vec3 HalfExtents{ 0.0f }; // 0 = deriva da escala do transform
};

// Agente de navegação: segue o caminho mais curto até um destino no
// NavGridComponent mais próximo. Movimento suave no plano XZ (altura fica
// com o script/character controller). Rotação (yaw) vira pra direção.
struct NavAgentComponent {
    float Speed = 4.0f;            // unidades/segundo
    float TurnSpeed = 8.0f;        // quão rápido gira o yaw (rad/s aprox)
    float StopDistance = 0.3f;     // para quando estiver a essa distância
    float Radius = 0.3f;           // raio de afastamento do obstáculo (margin)
    bool FaceMovement = true;      // gira o yaw pra direção do movimento
    bool Enabled = true;           // congela o agente sem destruir o caminho

    // API de script — ver Scene::SetNavDestination/StopNavAgent.
    glm::vec3 Destination{ 0.0f };
    bool HasDestination = false;
    std::vector<glm::vec3> Path;      // runtime
    size_t PathIndex = 0;             // runtime
    float PathTimer = 0.0f;           // runtime (recalcula periodicamente)
};

// Inteligência do inimigo: máquina de estados simples (Patrulha → Persegue →
// Ataca) dirigindo um NavAgent (mesma entidade ou ChildAgentName — se a
// entidade tiver um NavAgent, usa ele; senão procura um filho com esse nome).
struct EnemyAIComponent {
    enum class State { Patrol = 0, Chase = 1, Attack = 2 };

    State InitialState = State::Patrol;
    float SightRange = 12.0f;      // enxerga o alvo até essa distância
    float LoseRange = 18.0f;       // perde o alvo além dela (histerese)
    float ChaseRange = 8.0f;       // distância pra parar e atacar
    float AttackCooldown = 1.2f;   // segundos entre ataques
    float AttackDamage = 1.0f;
    float PatrolWait = 1.5f;       // segundos parado em cada ponto de patrulha
    std::vector<glm::vec3> PatrolPoints; // usados em ordem (loop)
    std::string TargetTag = "Jogador";   // quem o inimigo persegue (TagComponent)

    // Runtime.
    State m_State = InitialState;
    float m_StateTimer = 0.0f;      // tempo no estado (pro cooldown de ataque)
    float m_PatrolTimer = 0.0f;     // espera no ponto de patrulha
    int m_PatrolIndex = 0;          // ponto de patrulha atual
    bool m_HasTarget = false;
    uint32_t m_TargetHandle = 0;    // handle da entidade alvo (resolvida a cada frame)
};

} // namespace kizuri
