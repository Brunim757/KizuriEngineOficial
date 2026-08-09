#pragma once
#include "kizuri/Core.hpp"
#include "kizuri/renderer/Camera.hpp"
#include "kizuri/renderer/Buffer.hpp"
#include "kizuri/renderer/Texture.hpp"
#include "kizuri/renderer/Shader.hpp"
#include "kizuri/renderer/GraphicsSettings.hpp"
#include <glm/glm.hpp>
#include <cstddef>
#include <string>
#include <vector>

namespace kizuri {

struct Material; // forward — o struct completo vem logo abaixo do Mesh
// Vértice 3D. Joints/Weights (índices como float, convertido com int() no
// shader — evita glVertexAttribIPointer) ficam 0 pra malha estática, então
// o skinning é identidade e o mesmo shader atende os dois casos.
struct Vertex3D {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec4 Joints = glm::vec4(0.0f);   // índices das até 4 juntas que pesam no vértice
    glm::vec4 Weights = glm::vec4(0.0f);  // pesos (normalizados; 0 = estática)
};

// Mesh estática 3D (geometria + buffers de GPU já prontos).
class Mesh {
public:
    Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

    const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
    uint32_t GetIndexCount() const { return m_IndexCount; }

    // Vértices no espaço local (mantidos em CPU além do buffer de GPU) —
    // usado pelo editor/picking e pela física (heightmap do terreno).
    const std::vector<Vertex3D>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

    // AABB local, calculado uma vez no construtor — usado pelo picking por raio do editor.
    const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
    const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

    static Ref<Mesh> CreateCube();
    static Ref<Mesh> CreatePlane();
    static Ref<Mesh> CreateSphere(uint32_t sectors = 32, uint32_t stacks = 16);
    static Ref<Mesh> CreateCylinder(uint32_t sectors = 32);
    static Ref<Mesh> CreateCone(uint32_t sectors = 32);
    static Ref<Mesh> CreateCapsule(uint32_t sectors = 24, uint32_t stacks = 12);
    static Ref<Mesh> CreateTorus(uint32_t majorSeg = 48, uint32_t minorSeg = 24);
    // Terreno procedural (heightmap de fbm): grade de Segments+1 x Segments+1,
    // Size unidades de lado, elevação até HeightScale, com Seed fixo.
    static Ref<Mesh> CreateTerrain(uint32_t segments = 64, float size = 100.0f,
                                   float heightScale = 5.0f, uint32_t seed = 1);
    static Ref<Mesh> LoadFromOBJ(const std::string& path);
    static Ref<Mesh> LoadFromGLTF(const std::string& path); // .glb/.gltf via cgltf
    static Ref<Mesh> LoadFromGLTFMemory(const void* data, std::size_t size); // .glb em memória (embutido)

    // Extrai o material PBR do primeiro material do .glb/.gltf (fatores +
    // texturas embutidas no arquivo). Vazio/padrão se o arquivo não tiver.
    // Aceita caminho de disco OU kzres:// (recurso embutido).
    static Material ExtractMaterialFromGLTF(const std::string& path);
    static Material ExtractMaterialFromGLTFMemory(const void* data, std::size_t size); // .glb em memória (embutido)

    // Reconstrói uma mesh a partir da string serializável do editor
    // ("builtin:cube|plane|sphere|cylinder|cone|capsule|torus" | caminho .obj/.glb/.gltf).
    static Ref<Mesh> FromSource(const std::string& source);

private:
    Ref<VertexArray> m_VertexArray;
    std::vector<Vertex3D> m_Vertices;
    std::vector<uint32_t> m_Indices;
    uint32_t m_IndexCount = 0;
    glm::vec3 m_BoundsMin{ 0.0f };
    glm::vec3 m_BoundsMax{ 0.0f };
};

struct Material {
    glm::vec3 Albedo = { 0.8f, 0.8f, 0.8f };
    float Metallic = 0.0f;
    float Roughness = 0.5f;
    float AO = 1.0f; // oclusão ambiente escalar, só afeta a parte IBL
    glm::vec3 Emissive = glm::vec3(0.0f); // cor emissiva (alimenta o bloom)
    float EmissiveStrength = 0.0f;
    Ref<Texture2D> AlbedoMap;
    Ref<Texture2D> NormalMap; // tangent-space; TBN calculado por derivada de tela, sem atributo extra na mesh
    Ref<Texture2D> MetallicRoughnessMap; // canal G = roughness, canal B = metallic (convenção glTF)
    Ref<Texture2D> EmissiveMap;
    Ref<Texture2D> HeightMap; // parallax occlusion mapping (POM) — profundidade real de superfície
    float HeightScale = 0.08f; // intensidade do deslocamento de paralaxe (POM)
    bool PlanarReflect = false; // espelho real: a cena é renderizada de novo numa câmera refletida na face da entidade
    // Caminhos serializáveis — é o que permite salvar/abrir cena com
    // textura de material (ver ComponentSerialization.hpp). Vazios = sem mapa.
    std::string AlbedoMapPath;
    std::string NormalMapPath;
    std::string MetallicRoughnessMapPath;
    std::string EmissiveMapPath;
    std::string HeightMapPath;
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
    bool CastsShadow = false; // Point/Spot: projeta sombra (depth cubemap)
};

using DirectionalLight = Light; // alias de compatibilidade com código/serialização antigos

constexpr int kCascadeCount = 3; // faixas de distância do CSM: perto/média/longe
constexpr uint32_t kMaxParticlesPerBatch = 4000; // cap do buffer de instância na GPU
constexpr uint32_t kMaxInstancesPerBatch = 128;  // instâncias de malha por draw call (uniform array)

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

    // Configurações gráficas globais (qualidade preset / MSAA / SSAO /
    // bloom / exposição / resolução interna). Aplicar em runtime não exige
    // reiniciar nada: os recursos que dependem de tamanho (HDR, shadow map,
    // SSAO) são recriados no próximo EndScene de forma preguiçosa.
    static const GraphicsSettings& GetGraphicsSettings();
    static void SetGraphicsSettings(const GraphicsSettings& settings);

    // Ambiente IBL: caminho vazio = céu procedural (padrão); caminho de um
    // .hdr/.exr equirectangular = carrega, converte pra cubemap e rebakeia
    // irradiância + pré-filtro a partir da imagem HDR real.
    static void SetEnvironmentHDRIPath(const std::string& path);
    static const std::string& GetEnvironmentHDRIPath();

    // Empilha uma luz pro frame atual; a 1ª Directional submetida vira a que projeta sombra/céu.
    static void SubmitLight(const Light& light);

    // Empilha um comando de desenho; EndScene() resolve sombra + cor de verdade em dois passes.
    static void Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform);

    // Mesmo que Submit, mas com skinning: jointMatrices (global * inverseBind)
    // das kMaxSkinJoints primeiras juntas, avaliadas pela SkinData do animator.
    static void SubmitSkinned(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform,
                              const glm::mat4* jointMatrices, uint32_t jointCount);

    // Instancing de malhas: desenha a MESMA malha/material em N transformadas
    // num ÚNICO draw call (floresta, multidão, pedras...). Internamente usa
    // uniform array + gl_InstanceID (128 por lote; chamadas maiores dividem).
    static void SubmitMeshInstances(const Ref<Mesh>& mesh, const Material& material,
                                    const glm::mat4* transforms, uint32_t count);

    // Empilha um lote de partículas (billboards sempre de frente pra câmera, GPU-instanced —
    // um glDrawElementsInstanced por lote, não um draw call por partícula). Sem textura (nullptr)
    // usa um degradê radial procedural; recorta pra kMaxParticlesPerBatch se vier maior que isso.
    static void SubmitParticles(const std::vector<ParticleInstance>& instances, bool additive,
                                const Ref<Texture2D>& texture = nullptr);

    // Grid de referência do editor (plano XZ) — não entra no jogo exportado nem no shadow map.
    static void DrawGrid();

    // Linhas de DEPURAÇÃO com teste de profundidade (gizmos de collider etc.).
    // Desenhadas dentro do passe HDR, depois das malhas — ocultadas por
    // geometria mais próxima e no topo da superfície do próprio objeto.
    struct DebugLine { glm::vec3 From; glm::vec3 To; glm::vec3 Color; };
    static void SubmitDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);

private:
    struct DrawCommand {
        Ref<Mesh> MeshAsset;
        Material Mat;
        glm::mat4 Transform;
        std::vector<glm::mat4> Joints; // vazio = malha estática
    };

    // Lote instanciado: mesma malha/material, N transformadas num draw call.
    struct InstanceBatch {
        Ref<Mesh> MeshAsset;
        Material Mat;
        std::vector<glm::mat4> Transforms;
    };

    static Ref<Shader> s_MeshShader;
    static Ref<Shader> s_LineShader;
    static Ref<VertexArray> s_GridVAO;
    static uint32_t s_GridVertexCount;
    static Ref<VertexArray> s_DebugVAO;
    static Ref<VertexBuffer> s_DebugVBO;
    static glm::mat4 s_ViewProjection;

    static std::vector<DrawCommand> s_DrawList;
    static std::vector<InstanceBatch> s_InstanceBatches;
    static std::vector<DebugLine> s_DebugLines;
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


    // HDRI: textura equirectangular 2D carregada do arquivo (0 = procedural);
    // o shader converte pra cubemap durante o bake.
    static uint32_t s_EquirectTexture;
    static Ref<Shader> s_EquirectShader;
    static std::string s_EnvironmentHDRIPath;

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
    static bool LoadHDRI(const std::string& path);

    // Pós-processamento: a cena inteira (mesh + skybox) é desenhada num framebuffer HDR interno
    // (RGBA16F, sem clamp em [0,1]) em vez de ir direto pro destino final. Com MSAA ligado esse
    // framebuffer é multisample; depois do passe da cena um blit resolve cor+profundidade pro
    // framebuffer simples s_HDRFBO (que é o que os passes seguintes amostram). Dali, um bright-pass +
    // blur separável (ping-pong, meia resolução) gera o glow do bloom, SSAO é calculado do depth
    // resolvido, e um passe de composição final soma bloom + aplica oclusão + tonemap ACES e grava
    // no framebuffer que o chamador pediu.
    static void EnsurePostBuffers(uint32_t width, uint32_t height, int msaa); // (re)cria se o tamanho/msaa mudou
    static void EnsureShadowMaps(uint32_t size); // (re)cria os shadow maps se a resolução mudou
    static void EnsureSSAOBuffers(uint32_t width, uint32_t height); // (re)cria se o tamanho mudou
    static GraphicsSettings s_Settings;
    static uint32_t s_HDRFBO, s_HDRColorBuffer, s_HDRDepthTexture;      // destino simples (resolvido)
    static uint32_t s_MSAAHDRFBO, s_MSAAHDRColor, s_MSAAHDRDepthRBO;    // destino multisample (MSAA>1)
    static int s_CurrentMSAA;
    static uint32_t s_BloomFBO[2], s_BloomColorBuffer[2];
    static uint32_t s_PostWidth, s_PostHeight;
    static Ref<VertexArray> s_FullscreenQuad;
    static Ref<Shader> s_BrightPassShader, s_BlurShader, s_CompositeShader;
    static bool s_DrawGridFlag; // DrawGrid() só marca a intenção; o desenho de verdade é dentro do passe HDR (EndScene)

    // SSAO: depth resolvido -> oclusão meia-resolução -> blur -> aplicado no composite.
    static uint32_t s_SSAOFBO, s_SSAOColorBuffer;
    static uint32_t s_SSAOBlurFBO, s_SSAOBlurBuffer;
    static uint32_t s_NoiseTexture; // 4x4 vetores aleatórios pra quebrar a banda do hemisfério
    static Ref<Shader> s_SSAOShader;
    static std::vector<glm::vec3> s_SSAOKernel; // amostras do hemisfério (geradas em Init)
    static uint32_t s_SSAOWidth, s_SSAOHeight;

    // SSR (reflexos em espaço de tela) — marcha do raio refletido contra o depth
    // buffer num loop de PASSOS FIXOS (GLSL 330-safe). Cor + depth -> RGBA16F;
    // o composite adiciona a reflexão ao HDR antes do tonemap.
    static Ref<Shader> s_SSRShader;
    static uint32_t s_SSRFBO, s_SSRColorBuffer;

    // TAA (anti-aliasing temporal): composite escreve num alvo intermediário
    // (s_TAAComposite), um passe fullscreen mistura com o histórico
    // (ping-pong s_TAAHistoryFBO) com clamp de vizinhança e blita pro destino.
    static Ref<Shader> s_TAAShader;
    static uint32_t s_TAACompositeFBO, s_TAACompositeTex;
    static uint32_t s_TAAHistoryFBO[2], s_TAAHistoryTex[2];
    static bool s_TAAHistoryValid;
    static uint32_t s_TAACounter;

    // God rays / luz volumétrica (espaço de tela): marcha radial até o sol na
    // tela, acumulando o brilho da cena (loop de passos FIXOS, GLSL 330-safe).
    static Ref<Shader> s_GodRaysShader;
    static uint32_t s_GodRaysFBO, s_GodRaysColorBuffer;

    // Planar reflections (espelho real): a cena é renderizada de novo numa
    // câmera refletida na face do espelho (pré-passe), e o material espelhado
    // amostra essa textura. 100% OpenGL 3.3 (FBO + blit padrão).
    static uint32_t s_PlanarFBO, s_PlanarColor, s_PlanarDepth;
    static uint32_t s_PlanarWidth, s_PlanarHeight;
    static glm::mat4 s_ReflectionViewProjection; // VP da câmera refletida (pro sampling no shader)
    static bool s_HasPlanarReflection;

    // DOF (bokeh, 1 passe) + Motion blur (por reprojeção): FBOs RGBA16F entre
    // a cena HDR e o bright-pass. Movimento estimado pela VP do frame anterior
    // (sem velocity buffer) — ambos com loops de passos FIXOS, GLSL 330-safe.
    static Ref<Shader> s_DOFShader, s_MotionBlurShader;
    static uint32_t s_DOFFBO, s_DOFTex;
    static uint32_t s_MotionFBO, s_MotionTex;
    static glm::mat4 s_MotionPrevVP; // VP (sem jitter) do frame anterior
    static glm::mat4 s_MotionCurrVP; // VP (sem jitter) do frame atual

    // v0.25 — SSGI (GI 1-bounce), Lens flare, FXAA.
    static Ref<Shader> s_SSGIShader;
    static uint32_t s_SSGIFBO, s_SSGIColor;
    static Ref<Shader> s_LensFlareShader;
    static uint32_t s_LensFBO, s_LensColor;
    static Ref<Shader> s_FXAAShader;

    // (Re)cria o FBO da reflexão planar se a resolução mudou.
    static void EnsurePlanarBuffers(uint32_t width, uint32_t height);
    // Pré-passe do espelho real: desenha skybox + meshes (sem o espelho) com a
    // câmera refletida e projeção com near plane oblíquo na face do espelho.
    static void RenderPlanarReflection(const glm::vec3& planePoint, const glm::vec3& planeNormal,
                                       int mirrorIndex, uint32_t width, uint32_t height,
                                       const glm::mat4& reflectView, const glm::mat4& reflectProj,
                                       const glm::mat4& reflectVP, const glm::vec3& reflectCam);

    // Partículas: 1 VAO reaproveitado por todos os lotes do frame — buffer de quad estático
    // (attribs 0/1) + buffer de instância dinâmico, reescrito via glBufferSubData a cada lote.
    struct ParticleBatch { std::vector<ParticleInstance> Instances; bool Additive; Ref<Texture2D> Texture; };
    static std::vector<ParticleBatch> s_ParticleBatches;
    static uint32_t s_ParticleVAO, s_ParticleQuadVBO, s_ParticleEBO, s_ParticleInstanceVBO;
    static Ref<Shader> s_ParticleShader;
};

} // namespace kizuri
