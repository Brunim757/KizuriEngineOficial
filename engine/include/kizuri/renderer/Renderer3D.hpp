#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/Buffer.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Shader.hpp"
#include <glm/glm.hpp>
#include <vector>

namespace kizuri {

struct Vertex3D {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
};

// Mesh estática 3D (geometria + buffers de GPU já prontos).
class Mesh {
public:
    Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

    const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
    uint32_t GetIndexCount() const { return m_IndexCount; }

    // AABB local, calculado uma vez no construtor — usado pelo picking por raio do editor.
    const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
    const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

    static Ref<Mesh> CreateCube();
    static Ref<Mesh> CreatePlane();
    static Ref<Mesh> CreateSphere(uint32_t sectors = 32, uint32_t stacks = 16);
    static Ref<Mesh> LoadFromOBJ(const std::string& path);

    // Reconstrói uma mesh a partir da string serializável do editor
    // ("builtin:cube" | "builtin:plane" | "builtin:sphere" | caminho .obj).
    static Ref<Mesh> FromSource(const std::string& source);

private:
    Ref<VertexArray> m_VertexArray;
    uint32_t m_IndexCount = 0;
    glm::vec3 m_BoundsMin{ 0.0f };
    glm::vec3 m_BoundsMax{ 0.0f };
};

struct Material {
    glm::vec3 Albedo = { 0.8f, 0.8f, 0.8f };
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AO = 1.0f; // oclusão ambiente escalar, só afeta a parte IBL
    Ref<Texture2D> AlbedoMap;
    Ref<Texture2D> NormalMap; // tangent-space; TBN calculado por derivada de tela, sem atributo extra na mesh
    // Caminhos serializáveis — é o que permite salvar/abrir cena com
    // textura de material (ver ComponentSerialization.hpp). Vazios = sem mapa.
    std::string AlbedoMapPath;
    std::string NormalMapPath;
};

enum class LightType { Directional = 0, Point = 1, Spot = 2 };

// Uma luz dinâmica da cena. Directional é a única que projeta sombra (v1) e
// serve de sol pro céu/IBL; Point/Spot só contribuem luz direta.
struct Light {
    LightType Type = LightType::Directional;
    glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Direction = { -0.3f, -1.0f, -0.2f };
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerConeDeg = 20.0f;
    float OuterConeDeg = 30.0f;
};

using DirectionalLight = Light; // alias de compatibilidade com código/serialização antigos

constexpr int kCascadeCount = 3; // faixas de distância do CSM: perto/média/longe
constexpr uint32_t kMaxParticlesPerBatch = 4000; // cap do buffer de instância na GPU

// Dado por-partícula que vai pra GPU via instancing (glVertexAttribDivisor) — um draw call
// desenha o lote inteiro. Layout tem que bater com o stride/offset usados em Init()/EndScene.
struct ParticleInstance {
    glm::vec3 Position{ 0.0f };
    float Size = 0.1f;
    glm::vec4 Color{ 1.0f };
};

// Pipeline forward, PBR (Cook-Torrance) + normal mapping + IBL (céu
// procedural) + até kMaxLights luzes dinâmicas + sombra da luz direcional.
class Renderer3D {
public:
    static void Init();
    static void Shutdown();

    static void BeginScene(const PerspectiveCamera& camera);
    static void EndScene();

    // Empilha uma luz pro frame atual; a 1ª Directional submetida vira a que projeta sombra/céu.
    static void SubmitLight(const Light& light);

    // Empilha um comando de desenho; EndScene() resolve sombra + cor de verdade em dois passes.
    static void Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform);

    // Empilha um lote de partículas (billboards sempre de frente pra câmera, GPU-instanced —
    // um glDrawElementsInstanced por lote, não um draw call por partícula). Sem textura (nullptr)
    // usa um degradê radial procedural; recorta pra kMaxParticlesPerBatch se vier maior que isso.
    static void SubmitParticles(const std::vector<ParticleInstance>& instances, bool additive);

    // Grid de referência do editor (plano XZ) — não entra no jogo exportado nem no shadow map.
    static void DrawGrid();

private:
    struct DrawCommand {
        Ref<Mesh> MeshAsset;
        Material Mat;
        glm::mat4 Transform;
    };

    static Ref<Shader> s_MeshShader;
    static Ref<Shader> s_LineShader;
    static Ref<VertexArray> s_GridVAO;
    static uint32_t s_GridVertexCount;
    static glm::mat4 s_ViewProjection;

    static std::vector<DrawCommand> s_DrawList;
    static std::vector<Light> s_LightList;
    static Light s_ShadowCaster;   // 1ª luz Directional do frame; usada pro shadow map e pro céu/IBL
    static bool s_HasShadowCaster;
    static glm::vec3 s_CameraPos;

    // Shadow mapping direcional em cascata (CSM), 3 faixas de distância da câmera —
    // cada cascata tem seu próprio FBO/textura/matriz, área ajustada à parte do frustum que cobre.
    static uint32_t s_ShadowFBO[kCascadeCount];
    static uint32_t s_ShadowMap[kCascadeCount];
    static Ref<Shader> s_ShadowShader;
    static glm::mat4 s_LightSpaceMatrix[kCascadeCount];
    static float s_CascadeSplits[kCascadeCount]; // distância (view-space) onde cada cascata termina

    // IBL: céu procedural (s_EnvironmentCubemap, também usado como skybox), convoluído em
    // irradiância difusa (s_IrradianceCubemap) e pré-filtrado GGX por rugosidade (s_PrefilterCubemap).
    // Bake único em Init() — ver GenerateEnvironment() e a limitação de ambiente estático no ROADMAP.
    static uint32_t s_EnvironmentCubemap;
    static uint32_t s_IrradianceCubemap;
    static uint32_t s_PrefilterCubemap;
    static Ref<Mesh> s_CaptureCube;
    static Ref<Shader> s_SkyboxShader;
    static glm::mat4 s_View;
    static glm::mat4 s_Projection;
    static float s_CamFOV, s_CamAspect, s_CamNear, s_CamFar;

    static void GenerateEnvironment();
    static void ComputeCascades(const glm::vec3& lightDir); // preenche s_LightSpaceMatrix[]/s_CascadeSplits[]

    // Pós-processamento: a cena inteira (mesh + skybox) é desenhada num framebuffer HDR interno
    // (RGBA16F, sem clamp em [0,1]) em vez de ir direto pro destino final. Dali, um bright-pass +
    // blur separável (ping-pong, meia resolução) gera o glow do bloom, e um passe de composição
    // final soma bloom + cena, aplica tonemap ACES e grava no framebuffer que o chamador pediu.
    static void EnsurePostBuffers(uint32_t width, uint32_t height); // (re)cria se o tamanho mudou
    static uint32_t s_HDRFBO, s_HDRColorBuffer, s_HDRDepthRBO;
    static uint32_t s_BloomFBO[2], s_BloomColorBuffer[2];
    static uint32_t s_PostWidth, s_PostHeight;
    static Ref<VertexArray> s_FullscreenQuad;
    static Ref<Shader> s_BrightPassShader, s_BlurShader, s_CompositeShader;
    static bool s_DrawGridFlag; // DrawGrid() só marca a intenção; o desenho de verdade é dentro do passe HDR (EndScene)

    // Partículas: 1 VAO reaproveitado por todos os lotes do frame — buffer de quad estático
    // (attribs 0/1) + buffer de instância dinâmico, reescrito via glBufferSubData a cada lote.
    struct ParticleBatch { std::vector<ParticleInstance> Instances; bool Additive; };
    static std::vector<ParticleBatch> s_ParticleBatches;
    static uint32_t s_ParticleVAO, s_ParticleQuadVBO, s_ParticleEBO, s_ParticleInstanceVBO;
    static Ref<Shader> s_ParticleShader;
};

} // namespace kizuri
