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

struct Material;

struct Vertex3D {
    glm::vec3 Position;
    glm::vec3 Normal;
    glm::vec2 TexCoord;
    glm::vec4 Joints = glm::vec4(0.0f);
    glm::vec4 Weights = glm::vec4(0.0f);
};

class Mesh {
public:
    Mesh(const std::vector<Vertex3D>& vertices, const std::vector<uint32_t>& indices);

    const Ref<VertexArray>& GetVertexArray() const { return m_VertexArray; }
    uint32_t GetIndexCount() const { return m_IndexCount; }

    const std::vector<Vertex3D>& GetVertices() const { return m_Vertices; }
    const std::vector<uint32_t>& GetIndices() const { return m_Indices; }

    const glm::vec3& GetBoundsMin() const { return m_BoundsMin; }
    const glm::vec3& GetBoundsMax() const { return m_BoundsMax; }

    static Ref<Mesh> CreateCube();
    static Ref<Mesh> CreatePlane();
    static Ref<Mesh> CreateSphere(uint32_t sectors = 32, uint32_t stacks = 16);
    static Ref<Mesh> CreateCylinder(uint32_t sectors = 32);
    static Ref<Mesh> CreateCone(uint32_t sectors = 32);
    static Ref<Mesh> CreateCapsule(uint32_t sectors = 24, uint32_t stacks = 12);
    static Ref<Mesh> CreateTorus(uint32_t majorSeg = 48, uint32_t minorSeg = 24);

    static Ref<Mesh> CreateTerrain(uint32_t segments = 64, float size = 100.0f,
                                   float heightScale = 5.0f, uint32_t seed = 1);

    static Ref<Mesh> CreateLODMesh(const std::string& source, int level);

    static Ref<Mesh> CreateTerrainFromHeightmap(uint32_t segments, float size,
                                                const std::vector<float>& heights);    static Ref<Mesh> LoadFromOBJ(const std::string& path);
    static Ref<Mesh> LoadFromGLTF(const std::string& path);
    static Ref<Mesh> LoadFromGLTFMemory(const void* data, std::size_t size);

    static Material ExtractMaterialFromGLTF(const std::string& path);
    static Material ExtractMaterialFromGLTFMemory(const void* data, std::size_t size);

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
    float AO = 1.0f;
    glm::vec3 Emissive = glm::vec3(0.0f);
    float EmissiveStrength = 0.0f;
    Ref<Texture2D> AlbedoMap;
    Ref<Texture2D> NormalMap;
    Ref<Texture2D> MetallicRoughnessMap;
    Ref<Texture2D> EmissiveMap;
    Ref<Texture2D> HeightMap;
    float HeightScale = 0.08f;
    bool PlanarReflect = false;

    std::string AlbedoMapPath;
    std::string NormalMapPath;
    std::string MetallicRoughnessMapPath;
    std::string EmissiveMapPath;
    std::string HeightMapPath;
};

enum class LightType { Directional = 0, Point = 1, Spot = 2 };

struct Light {
    LightType Type = LightType::Directional;
    glm::vec3 Position = { 0.0f, 0.0f, 0.0f };
    glm::vec3 Direction = { -0.3f, -1.0f, -0.2f };
    glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
    float Intensity = 1.0f;
    float Range = 10.0f;
    float InnerConeDeg = 20.0f;
    float OuterConeDeg = 30.0f;
    bool CastsShadow = false;
};

using DirectionalLight = Light;

constexpr int kCascadeCount = 3;
constexpr uint32_t kMaxParticlesPerBatch = 4000;
constexpr uint32_t kMaxInstancesPerBatch = 128;

struct ParticleInstance {
    glm::vec3 Position{ 0.0f };
    float Size = 0.1f;
    glm::vec4 Color{ 1.0f };
};

class Renderer3D {
public:
    static void Init();
    static void Shutdown();

    static void BeginScene(const PerspectiveCamera& camera);
    static void EndScene();

    static glm::mat4 GetLastViewProjection() { return s_ViewProjection; }

    static const GraphicsSettings& GetGraphicsSettings();
    static void SetGraphicsSettings(const GraphicsSettings& settings);

    static void SetEnvironmentHDRIPath(const std::string& path);
    static const std::string& GetEnvironmentHDRIPath();

    static void SubmitLight(const Light& light);

    static void Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform);

    static void Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform,
                       const Ref<Texture2D>& lightmap);

    static void SubmitSkinned(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform,
                              const glm::mat4* jointMatrices, uint32_t jointCount);

    static void SubmitMeshInstances(const Ref<Mesh>& mesh, const Material& material,
                                    const glm::mat4* transforms, uint32_t count);

    static void SubmitParticles(const std::vector<ParticleInstance>& instances, bool additive,
                                const Ref<Texture2D>& texture = nullptr);

    static void DrawGrid();

    struct DebugLine { glm::vec3 From; glm::vec3 To; glm::vec3 Color; };
    static void SubmitDebugLine(const glm::vec3& from, const glm::vec3& to, const glm::vec3& color);

    static void SubmitDecal(const glm::mat4& transform, const Ref<Texture2D>& texture,
                            const glm::vec4& tint = glm::vec4(1.0f));

private:
    struct DrawCommand {
        Ref<Mesh> MeshAsset;
        Material Mat;
        glm::mat4 Transform;
        std::vector<glm::mat4> Joints;
        Ref<Texture2D> Lightmap;
    };

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
    static Light s_ShadowCaster;
    static bool s_HasShadowCaster;
    static glm::vec3 s_CameraPos;

    static uint32_t s_ShadowFBO[kCascadeCount];
    static uint32_t s_ShadowMap[kCascadeCount];
    static Ref<Shader> s_ShadowShader;
    static glm::mat4 s_LightSpaceMatrix[kCascadeCount];
    static float s_CascadeSplits[kCascadeCount];

    static uint32_t s_EquirectTexture;
    static Ref<Shader> s_EquirectShader;
    static std::string s_EnvironmentHDRIPath;

    static uint32_t s_EnvironmentCubemap;
    static uint32_t s_IrradianceCubemap;
    static uint32_t s_PrefilterCubemap;
    static Ref<Mesh> s_CaptureCube;
    static Ref<Shader> s_SkyboxShader;
    static glm::mat4 s_View;
    static glm::mat4 s_Projection;
    static float s_CamFOV, s_CamAspect, s_CamNear, s_CamFar;

    static void GenerateEnvironment();
    static void ComputeCascades(const glm::vec3& lightDir);
    static bool LoadHDRI(const std::string& path);

    static void EnsurePostBuffers(uint32_t width, uint32_t height, int msaa);
    static void EnsureShadowMaps(uint32_t size);
    static void EnsureSSAOBuffers(uint32_t width, uint32_t height);
    static GraphicsSettings s_Settings;
    static uint32_t s_HDRFBO, s_HDRColorBuffer, s_HDRDepthTexture;
    static uint32_t s_MSAAHDRFBO, s_MSAAHDRColor, s_MSAAHDRDepthRBO;
    static int s_CurrentMSAA;
    static uint32_t s_BloomFBO[2], s_BloomColorBuffer[2];
    static uint32_t s_PostWidth, s_PostHeight;
    static Ref<VertexArray> s_FullscreenQuad;
    static Ref<Shader> s_BrightPassShader, s_BlurShader, s_CompositeShader;
    static bool s_DrawGridFlag;

    static uint32_t s_SSAOFBO, s_SSAOColorBuffer;
    static uint32_t s_SSAOBlurFBO, s_SSAOBlurBuffer;
    static uint32_t s_NoiseTexture;
    static Ref<Shader> s_SSAOShader;
    static std::vector<glm::vec3> s_SSAOKernel;
    static uint32_t s_SSAOWidth, s_SSAOHeight;

    static Ref<Shader> s_SSRShader;
    static uint32_t s_SSRFBO, s_SSRColorBuffer;

    static Ref<Shader> s_TAAShader;
    static uint32_t s_TAACompositeFBO, s_TAACompositeTex;
    static uint32_t s_TAAHistoryFBO[2], s_TAAHistoryTex[2];
    static bool s_TAAHistoryValid;
    static uint32_t s_TAACounter;

    static Ref<Shader> s_GodRaysShader;
    static uint32_t s_GodRaysFBO, s_GodRaysColorBuffer;

    static uint32_t s_PlanarFBO, s_PlanarColor, s_PlanarDepth;
    static uint32_t s_PlanarWidth, s_PlanarHeight;
    static glm::mat4 s_ReflectionViewProjection;
    static bool s_HasPlanarReflection;

    static Ref<Shader> s_DOFShader, s_MotionBlurShader;
    static uint32_t s_DOFFBO, s_DOFTex;
    static uint32_t s_MotionFBO, s_MotionTex;
    static glm::mat4 s_MotionPrevVP;
    static glm::mat4 s_MotionCurrVP;

    static Ref<Shader> s_SSGIShader;
    static uint32_t s_SSGIFBO, s_SSGIColor;
    static Ref<Shader> s_LensFlareShader;
    static uint32_t s_LensFBO, s_LensColor;
    static Ref<Shader> s_FXAAShader;

    static void EnsurePlanarBuffers(uint32_t width, uint32_t height);

    static void RenderPlanarReflection(const glm::vec3& planePoint, const glm::vec3& planeNormal,
                                       int mirrorIndex, uint32_t width, uint32_t height,
                                       const glm::mat4& reflectView, const glm::mat4& reflectProj,
                                       const glm::mat4& reflectVP, const glm::vec3& reflectCam);

    struct ParticleBatch { std::vector<ParticleInstance> Instances; bool Additive; Ref<Texture2D> Texture; };
    static std::vector<ParticleBatch> s_ParticleBatches;
    static uint32_t s_ParticleVAO, s_ParticleQuadVBO, s_ParticleEBO, s_ParticleInstanceVBO;
    static Ref<Shader> s_ParticleShader;
};

}
