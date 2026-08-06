#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/ecs/Animator.hpp"
#include "kizuri/core/EmbeddedContent.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <stb_image.h>
#include <string>
#include <fstream>
#include <cstddef>
#include <cstdlib>
#include <cmath>
#include <utility>

namespace kizuri {

Ref<Shader> Renderer3D::s_MeshShader;
Ref<Shader> Renderer3D::s_LineShader;
Ref<VertexArray> Renderer3D::s_GridVAO;
uint32_t Renderer3D::s_GridVertexCount = 0;
glm::mat4 Renderer3D::s_ViewProjection = glm::mat4(1.0f);

std::vector<Renderer3D::DrawCommand> Renderer3D::s_DrawList;
std::vector<Light> Renderer3D::s_LightList;
Light Renderer3D::s_ShadowCaster;
bool Renderer3D::s_HasShadowCaster = false;
glm::vec3 Renderer3D::s_CameraPos = glm::vec3(0.0f);

uint32_t Renderer3D::s_ShadowFBO[kCascadeCount] = {};
uint32_t Renderer3D::s_ShadowMap[kCascadeCount] = {};
Ref<Shader> Renderer3D::s_ShadowShader;
glm::mat4 Renderer3D::s_LightSpaceMatrix[kCascadeCount];
float Renderer3D::s_CascadeSplits[kCascadeCount] = {};


uint32_t Renderer3D::s_EquirectTexture = 0;
Ref<Shader> Renderer3D::s_EquirectShader;
std::string Renderer3D::s_EnvironmentHDRIPath;

uint32_t Renderer3D::s_EnvironmentCubemap = 0;
uint32_t Renderer3D::s_IrradianceCubemap = 0;
uint32_t Renderer3D::s_PrefilterCubemap = 0;
Ref<Mesh> Renderer3D::s_CaptureCube;
Ref<Shader> Renderer3D::s_SkyboxShader;
glm::mat4 Renderer3D::s_View = glm::mat4(1.0f);
glm::mat4 Renderer3D::s_Projection = glm::mat4(1.0f);
float Renderer3D::s_CamFOV = 45.0f, Renderer3D::s_CamAspect = 16.0f / 9.0f;
float Renderer3D::s_CamNear = 0.1f, Renderer3D::s_CamFar = 1000.0f;

GraphicsSettings Renderer3D::s_Settings;
uint32_t Renderer3D::s_HDRFBO = 0, Renderer3D::s_HDRColorBuffer = 0, Renderer3D::s_HDRDepthTexture = 0;
uint32_t Renderer3D::s_MSAAHDRFBO = 0, Renderer3D::s_MSAAHDRColor = 0, Renderer3D::s_MSAAHDRDepthRBO = 0;
int Renderer3D::s_CurrentMSAA = 1;
uint32_t Renderer3D::s_BloomFBO[2] = {}, Renderer3D::s_BloomColorBuffer[2] = {};
uint32_t Renderer3D::s_PostWidth = 0, Renderer3D::s_PostHeight = 0;
Ref<VertexArray> Renderer3D::s_FullscreenQuad;
Ref<Shader> Renderer3D::s_BrightPassShader, Renderer3D::s_BlurShader, Renderer3D::s_CompositeShader;
bool Renderer3D::s_DrawGridFlag = false;

uint32_t Renderer3D::s_SSAOFBO = 0, Renderer3D::s_SSAOColorBuffer = 0;
uint32_t Renderer3D::s_SSAOBlurFBO = 0, Renderer3D::s_SSAOBlurBuffer = 0;
uint32_t Renderer3D::s_NoiseTexture = 0;
Ref<Shader> Renderer3D::s_SSAOShader;
std::vector<glm::vec3> Renderer3D::s_SSAOKernel;
uint32_t Renderer3D::s_SSAOWidth = 0, Renderer3D::s_SSAOHeight = 0;

Ref<Shader> Renderer3D::s_SSRShader;
uint32_t Renderer3D::s_SSRFBO = 0, Renderer3D::s_SSRColorBuffer = 0;

Ref<Shader> Renderer3D::s_TAAShader;
uint32_t Renderer3D::s_TAACompositeFBO = 0, Renderer3D::s_TAACompositeTex = 0;
uint32_t Renderer3D::s_TAAHistoryFBO[2] = { 0, 0 }, Renderer3D::s_TAAHistoryTex[2] = { 0, 0 };
bool Renderer3D::s_TAAHistoryValid = false;
uint32_t Renderer3D::s_TAACounter = 0;

Ref<Shader> Renderer3D::s_GodRaysShader;
uint32_t Renderer3D::s_GodRaysFBO = 0, Renderer3D::s_GodRaysColorBuffer = 0;

uint32_t Renderer3D::s_PlanarFBO = 0, Renderer3D::s_PlanarColor = 0, Renderer3D::s_PlanarDepth = 0;
uint32_t Renderer3D::s_PlanarWidth = 0, Renderer3D::s_PlanarHeight = 0;
glm::mat4 Renderer3D::s_ReflectionViewProjection = glm::mat4(1.0f);
bool Renderer3D::s_HasPlanarReflection = false;
static float s_PostTime = 0.0f; // relógio do pós-processamento (grão de filme animado)

std::vector<Renderer3D::ParticleBatch> Renderer3D::s_ParticleBatches;
uint32_t Renderer3D::s_ParticleVAO = 0, Renderer3D::s_ParticleQuadVBO = 0;
uint32_t Renderer3D::s_ParticleEBO = 0, Renderer3D::s_ParticleInstanceVBO = 0;
Ref<Shader> Renderer3D::s_ParticleShader;

static constexpr uint32_t kEnvironmentSize = 128;
static constexpr uint32_t kIrradianceSize = 32;
static constexpr uint32_t kPrefilterBaseSize = 128;
static constexpr uint32_t kPrefilterMipLevels = 5;

static constexpr uint32_t kMaxLights = 16;

static const char* s_MeshVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in vec4 a_JointIndices;
layout(location = 4) in vec4 a_JointWeights;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat3 u_NormalMatrix;
uniform bool u_Animated;
uniform mat4 u_JointMatrices[64];

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    vec3 pos = a_Position;
    vec3 nrm = a_Normal;
    if (u_Animated) {
        // 4 juntas por vértice (peso 0 nos não usados). Índices chegam como
        // float e viram int() aqui — GLSL 330 não tem índice dinâmico de
        // array, mas mat4 por índice de atributo int() é constante.
        mat4 skin = u_JointMatrices[int(a_JointIndices.x)] * a_JointWeights.x
                  + u_JointMatrices[int(a_JointIndices.y)] * a_JointWeights.y
                  + u_JointMatrices[int(a_JointIndices.z)] * a_JointWeights.z
                  + u_JointMatrices[int(a_JointIndices.w)] * a_JointWeights.w;
        pos = vec3(skin * vec4(a_Position, 1.0));
        nrm = mat3(skin) * a_Normal;
    }
    v_FragPos = vec3(u_Transform * vec4(pos, 1.0));
    v_Normal = u_NormalMatrix * nrm;
    v_TexCoord = a_TexCoord;
    gl_Position = u_ViewProjection * vec4(v_FragPos, 1.0);
}
)";

static const char* s_MeshFragmentSrc = R"(
#version 330 core
layout(location = 0) out vec4 o_Color;

in vec3 v_FragPos;
in vec3 v_Normal;
in vec2 v_TexCoord;

uniform vec3 u_Albedo;
uniform float u_Metallic;
uniform float u_Roughness;
uniform float u_AO;
uniform vec3 u_Emissive;
uniform float u_EmissiveStrength;
uniform sampler2D u_AlbedoMap;
uniform bool u_HasAlbedoMap;
uniform sampler2D u_NormalMap;
uniform bool u_HasNormalMap;
uniform sampler2D u_MetallicRoughnessMap;
uniform bool u_HasMetallicRoughnessMap;
uniform sampler2D u_EmissiveMap;
uniform bool u_HasEmissiveMap;
uniform sampler2D u_HeightMap; // parallax occlusion mapping (POM)
uniform bool u_HasHeightMap;
uniform float u_HeightScale;   // intensidade do deslocamento de paralaxe

uniform vec3 u_ViewPos;
uniform mat4 u_View;
uniform bool u_HasShadow; // se a luz índice 0 é a que projeta sombra

uniform vec3 u_FogColor;
uniform float u_FogDensity;
uniform bool u_FogEnabled;

#define CASCADE_COUNT 3
uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform mat4 u_LightSpaceMatrix[CASCADE_COUNT];
uniform float u_CascadeSplits[CASCADE_COUNT]; // distância (view-space) onde cada cascata termina
uniform int u_ShadowPCF; // raio do PCF
uniform float u_ShadowSoftness; // PCSS: largura da penumbra (0 = desligado)

#define MAX_LIGHTS 16
uniform int u_LightCount;
uniform int u_LightTypes[MAX_LIGHTS];       // 0=Directional, 1=Point, 2=Spot
uniform vec3 u_LightPositions[MAX_LIGHTS];
uniform vec3 u_LightDirections[MAX_LIGHTS];
uniform vec3 u_LightColors[MAX_LIGHTS];
uniform float u_LightIntensities[MAX_LIGHTS];
uniform float u_LightRanges[MAX_LIGHTS];
uniform float u_LightInnerCos[MAX_LIGHTS];
uniform float u_LightOuterCos[MAX_LIGHTS];

uniform samplerCube u_IrradianceMap;
uniform samplerCube u_PrefilterMap;
uniform float u_MaxPrefilterLod;

// Reflexão planar (espelho real): só o material marcado como espelho amostra
// a textura da câmera refletida, projetando o próprio fragmento com a VP dela.
uniform sampler2D u_PlanarReflectionMap;
uniform bool u_IsPlanarMirror;
uniform mat4 u_ReflectionViewProjection;

const float PI = 3.14159265359;

// ---------------------------------------------------------------------
// PCSS (Percentage-Closer Soft Shadows) — sombra suave com penumbra
// proporcional ao tamanho do bloqueador. Tudo com loops de TETO CONSTANTE
// (PCSS_MAX_RADIUS é #define fixo) — compila em GLSL 330 core em qualquer
// driver. O PCSS antigo usava raio DINÂMICO (uniform) e quebrava em
// 3.3/Wine (objetos brancos); aqui o raio é fixo e só o número de taps
// EFETIVOS varia via `continue` (o loop em si permanece unrollável).
// ---------------------------------------------------------------------
#define PCSS_MAX_RADIUS 5

float SampleShadowPCF(sampler2D map, vec3 projCoords, float bias) {
    if (projCoords.z > 1.0) return 0.0;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);
    int r = max(u_ShadowPCF, 0);
    for (int x = -r; x <= r; ++x) {
        for (int y = -r; y <= r; ++y) {
            float pcfDepth = texture(map, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    float count = float((2 * r + 1) * (2 * r + 1));
    return shadow / count;
}

// PCSS: busca de bloqueadores -> penumbra -> PCF dentro da penumbra.
float SampleShadowPCSS(sampler2D map, vec3 projCoords, float bias) {
    if (projCoords.z > 1.0) return 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);

    // 1) Busca de bloqueadores num raio FIXO (teto constante).
    float blockerSum = 0.0;
    int blockerCount = 0;
    for (int x = -PCSS_MAX_RADIUS; x <= PCSS_MAX_RADIUS; ++x) {
        for (int y = -PCSS_MAX_RADIUS; y <= PCSS_MAX_RADIUS; ++y) {
            float d = texture(map, projCoords.xy + vec2(x, y) * texelSize).r;
            if (d < projCoords.z - bias) { blockerSum += d; ++blockerCount; }
        }
    }
    if (blockerCount == 0) return 0.0; // sem bloqueador = 100% iluminado
    float avgBlocker = blockerSum / float(blockerCount);

    // 2) Tamanho da penumbra proporcional à distância do bloqueador.
    float penumbra = clamp((projCoords.z - bias - avgBlocker) * u_ShadowSoftness * 120.0, 0.0, 1.0);
    if (penumbra <= 0.001) {
        // Sem penumbra (bloqueador colado): PCF simples.
        return SampleShadowPCF(map, projCoords, bias);
    }
    int pr = max(1, int(ceil(penumbra * float(PCSS_MAX_RADIUS))));

    // 3) PCF dentro do raio da penumbra (loop FIXO; o continue só descarta
    //    taps fora do raio efetivo — não muda a contagem do loop).
    float shadow = 0.0;
    int cnt = 0;
    for (int x = -PCSS_MAX_RADIUS; x <= PCSS_MAX_RADIUS; ++x) {
        for (int y = -PCSS_MAX_RADIUS; y <= PCSS_MAX_RADIUS; ++y) {
            if (x * x + y * y > pr * pr) continue;
            if (projCoords.z - bias > texture(map, projCoords.xy + vec2(x, y) * texelSize).r) shadow += 1.0;
            ++cnt;
        }
    }
    return shadow / float(max(cnt, 1));
}

// Escolhe a cascata pela profundidade view-space e amostra o shadow map certo — GLSL 330
// core não permite indexar array de sampler com índice dinâmico, daí o if/else explícito.
float CalculateShadow(vec3 N, vec3 L) {
    float viewDepth = -(u_View * vec4(v_FragPos, 1.0)).z;
    int idx = (viewDepth < u_CascadeSplits[0]) ? 0 : (viewDepth < u_CascadeSplits[1]) ? 1 : 2;
    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);

    // PCSS se a suavidade > 0; senão o PCF simples comprovado.
    bool usePCSS = (u_ShadowSoftness > 0.001);
    if (idx == 0) {
        vec4 p = u_LightSpaceMatrix[0] * vec4(v_FragPos, 1.0);
        vec3 pc = p.xyz / p.w * 0.5 + 0.5;
        return usePCSS ? SampleShadowPCSS(u_ShadowMap0, pc, bias) : SampleShadowPCF(u_ShadowMap0, pc, bias);
    } else if (idx == 1) {
        vec4 p = u_LightSpaceMatrix[1] * vec4(v_FragPos, 1.0);
        vec3 pc = p.xyz / p.w * 0.5 + 0.5;
        return usePCSS ? SampleShadowPCSS(u_ShadowMap1, pc, bias) : SampleShadowPCF(u_ShadowMap1, pc, bias);
    } else {
        vec4 p = u_LightSpaceMatrix[2] * vec4(v_FragPos, 1.0);
        vec3 pc = p.xyz / p.w * 0.5 + 0.5;
        return usePCSS ? SampleShadowPCSS(u_ShadowMap2, pc, bias) : SampleShadowPCF(u_ShadowMap2, pc, bias);
    }
}

// Distribuição GGX/Trowbridge-Reitz: alinhamento dos micro-facetos com o meio-vetor H.
float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + 0.0001);
}

// Oclusão geométrica de Smith (Schlick-GGX) — auto-sombreamento dos micro-facetos.
float GeometrySchlickGGX(float NdotV, float roughness) {
    float r = roughness + 1.0;
    float k = (r * r) / 8.0;
    return NdotV / (NdotV * (1.0 - k) + k);
}
float GeometrySmith(vec3 N, vec3 V, vec3 L, float roughness) {
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

// Fresnel-Schlick: fração refletida vs. absorvida/refratada, em função do ângulo.
vec3 FresnelSchlick(float cosTheta, vec3 F0) {
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}
vec3 FresnelSchlickRoughness(float cosTheta, vec3 F0, float roughness) {
    return F0 + (max(vec3(1.0 - roughness), F0) - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

// Aproximação de Karis pro BRDF ambiente — troca a LUT split-sum por uma fórmula fechada.
vec2 EnvBRDFApprox(float NdotV, float roughness) {
    const vec4 c0 = vec4(-1.0, -0.0275, -0.572, 0.022);
    const vec4 c1 = vec4(1.0, 0.0425, 1.04, -0.04);
    vec4 r = roughness * c0 + c1;
    float a004 = min(r.x * r.x, exp2(-9.28 * NdotV)) * r.x + r.y;
    return vec2(-1.04, 1.04) * a004 + r.zw;
}

// TBN por derivada de tela (dFdx/dFdy) — não precisa de atributo de tangente na mesh.
// sampleCoord: onde amostrar o mapa; as derivadas de v_TexCoord (originais) são
// as usadas pra montar a base tangente (o POM desloca a coordenada, não a base).
vec3 ApplyNormalMap(vec3 N, vec2 sampleCoord) {
    vec3 tangentNormal = texture(u_NormalMap, sampleCoord).xyz * 2.0 - 1.0;
    vec3 Q1 = dFdx(v_FragPos), Q2 = dFdy(v_FragPos);
    vec2 st1 = dFdx(v_TexCoord), st2 = dFdy(v_TexCoord);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * tangentNormal);
}

// Parallax Occlusion Mapping — marcha em passos FIXOS (teto constante, seguro
// em GLSL 330 core) sobre o mapa de altura ao longo da visão em espaço tangente,
// com interpolação linear no final pra evitar "corte" entre camadas. O
// viewDir Vt já vem em espaço tangente (dot com a base T,B,N).
#define POM_MAX_LAYERS 24
vec2 ApplyParallax(vec2 tc, vec3 Vt, out float parallaxMask) {
    parallaxMask = 0.0;
    // Vt.z < 0 (olhando de lado/costas) ou quase raso: sem POM pra não estourar.
    if (Vt.z < 0.02) return tc;
    parallaxMask = 1.0;

    vec2 P = Vt.xy / Vt.z * u_HeightScale;
    // Mais camadas perto de ângulos rasantes (constante no teto POM_MAX_LAYERS).
    float numLayers = mix(8.0, POM_MAX_LAYERS, 1.0 - Vt.z);
    if (numLayers < 2.0) numLayers = 2.0;
    vec2 delta = P / numLayers;
    float layerDepth = 1.0 / numLayers;

    vec2 currentTex = tc;
    float currentDepth = 0.0;
    float mapDepth = texture(u_HeightMap, currentTex).r;
    for (int i = 0; i < POM_MAX_LAYERS; ++i) {
        if (currentDepth >= mapDepth) break;
        currentTex -= delta;
        mapDepth = texture(u_HeightMap, currentTex).r;
        currentDepth += layerDepth;
    }

    // Interpolação linear entre os dois últimos passos (menos "serrilhado").
    vec2 prevTex = currentTex + delta;
    float afterDepth = mapDepth - currentDepth;
    float beforeDepth = texture(u_HeightMap, prevTex).r - currentDepth + layerDepth;
    float weight = clamp(afterDepth / (afterDepth + beforeDepth), 0.0, 1.0);
    return mix(currentTex, prevTex, weight);
}

void main() {
    // POM (parallax occlusion): desloca a coordenada de textura ao longo da
    // visão (espaço tangente, base TBN por derivada) antes de amostrar os
    // mapas — a superfície ganha relevo real que acompanha o ângulo da câmera.
    vec2 texCoord = v_TexCoord;
    vec3 N = normalize(v_Normal);
    float pomMask = 0.0;
    if (u_HasHeightMap) {
        vec3 Q1 = dFdx(v_FragPos), Q2 = dFdy(v_FragPos);
        vec2 st1 = dFdx(v_TexCoord), st2 = dFdy(v_TexCoord);
        vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
        vec3 B = -normalize(cross(N, T));
        vec3 Vw = normalize(u_ViewPos - v_FragPos);
        vec3 Vt = normalize(vec3(dot(Vw, T), dot(Vw, B), dot(Vw, N)));
        texCoord = ApplyParallax(texCoord, Vt, pomMask);
    }

    vec3 albedo = u_HasAlbedoMap ? texture(u_AlbedoMap, texCoord).rgb * u_Albedo : u_Albedo;

    float metallic = u_Metallic;
    float roughness = u_Roughness;
    if (u_HasMetallicRoughnessMap) {
        vec3 mr = texture(u_MetallicRoughnessMap, texCoord).rgb;
        roughness *= mr.g; // convenção glTF
        metallic *= mr.b;
    }

    if (u_HasNormalMap) N = ApplyNormalMap(N, texCoord);

    vec3 V = normalize(u_ViewPos - v_FragPos);
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    // Dielétrico ~4% de F0 fixo; metal usa o próprio albedo como F0 (colore o especular).
    vec3 F0 = mix(vec3(0.04), albedo, metallic);
    float shadow = u_HasShadow ? CalculateShadow(N, normalize(-u_LightDirections[0])) : 0.0;

    // --- Soma de todas as luzes dinâmicas do frame, Cook-Torrance por luz ---
    vec3 directLighting = vec3(0.0);
    for (int i = 0; i < u_LightCount; ++i) {
        vec3 L;
        float attenuation = 1.0;
        if (u_LightTypes[i] == 0) {
            L = normalize(-u_LightDirections[i]);
        } else {
            vec3 toLight = u_LightPositions[i] - v_FragPos;
            float dist = length(toLight);
            L = toLight / max(dist, 0.0001);
            float rangeFalloff = clamp(1.0 - pow(dist / max(u_LightRanges[i], 0.001), 4.0), 0.0, 1.0);
            attenuation = (rangeFalloff * rangeFalloff) / max(dist * dist, 0.01);
            if (u_LightTypes[i] == 2) {
                float theta = dot(L, normalize(-u_LightDirections[i]));
                float epsilon = max(u_LightInnerCos[i] - u_LightOuterCos[i], 0.0001);
                attenuation *= clamp((theta - u_LightOuterCos[i]) / epsilon, 0.0, 1.0);
            }
        }

        vec3 H = normalize(V + L);
        float NdotL = max(dot(N, L), 0.0);
        float NDF = DistributionGGX(N, H, roughness);
        float G = GeometrySmith(N, V, L, roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specularBRDF = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - metallic);
        vec3 radiance = u_LightColors[i] * u_LightIntensities[i] * attenuation;
        vec3 contribution = (kD * albedo / PI + specularBRDF) * radiance * NdotL;

        // Sombra só se aplica à luz 0 quando ela é a shadow caster (sempre a Directional, ver C++).
        if (i == 0 && u_HasShadow) contribution *= (1.0 - shadow);

        directLighting += contribution;
    }

    // --- Luz ambiente (IBL) — nunca é sombreada, luz indireta ainda chega em área de sombra ---
    vec3 F_ibl = FresnelSchlickRoughness(NdotV, F0, roughness);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - metallic);
    vec3 diffuseIBL = texture(u_IrradianceMap, N).rgb * albedo;
    vec2 envBRDF = EnvBRDFApprox(NdotV, roughness);
    vec3 specularIBL;
    if (u_IsPlanarMirror && u_ReflectionViewProjection[3][3] != 0.0) {
        // Espelho real: projeta o fragmento (que está NA face do espelho) na
        // VP da câmera refletida e amostra a cena espelhada. Não usa o F0 do
        // material (um espelho reflete sem tintar a cor da reflexão).
        vec4 rclip = u_ReflectionViewProjection * vec4(v_FragPos, 1.0);
        vec3 reflColor = vec3(0.0);
        if (rclip.w > 0.0001) {
            vec2 ruv = rclip.xy / rclip.w * 0.5 + 0.5;
            ruv.y = 1.0 - ruv.y; // origem da textura do FBO
            ruv = clamp(ruv, 0.001, 0.999);
            reflColor = texture(u_PlanarReflectionMap, ruv).rgb;
        }
        if (any(isnan(reflColor)) || any(isinf(reflColor))) reflColor = vec3(0.0);
        float edge = clamp(0.7 + 0.3 * pow(1.0 - NdotV, 3.0), 0.0, 1.0);
        specularIBL = reflColor * edge;
    } else {
        vec3 prefilteredColor = textureLod(u_PrefilterMap, R, roughness * u_MaxPrefilterLod).rgb;
        specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);
    }
    vec3 ambient = (kD_ibl * diffuseIBL + specularIBL) * u_AO;

    // Emissivo soma direto (e, acima de 1.0, alimenta o bright-pass do bloom).
    vec3 emissive = u_Emissive * u_EmissiveStrength;
    if (u_HasEmissiveMap) emissive *= texture(u_EmissiveMap, texCoord).rgb;

    vec3 color = ambient + directLighting + emissive;

    // Névoa exponencial (distância da câmera) — nunca no skybox, que não passa por aqui.
    if (u_FogEnabled) {
        float dist = length(u_ViewPos - v_FragPos);
        float fogFactor = 1.0 - exp(-u_FogDensity * u_FogDensity * dist * dist);
        color = mix(color, u_FogColor, clamp(fogFactor, 0.0, 1.0));
    }

    // Sem tonemap aqui — sai HDR linear cru de propósito. O passe de composição
    // (bright-pass + bloom + ACES) é quem faz o tonemap, uma vez só, no final.
    o_Color = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------
// Shaders de bake do ambiente (IBL) — rodam só dentro de GenerateEnvironment(),
// nunca no loop de desenho normal. Todos compartilham o mesmo vertex shader:
// desenham um cubo unitário centrado na origem e passam a própria posição
// local como direção (o cubo nunca se move, só a câmera de captura gira).
// ---------------------------------------------------------------------
static const char* s_CaptureVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;

out vec3 v_LocalPos;

void main() {
    v_LocalPos = a_Position;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

// Céu atmosférico procedural (ESCURO de propósito): mantém o gradiente base
// que funcionava (IBL estável) e adiciona tinta de pôr-do-sol no horizonte,
// azul mais rico no zênite e estrelas à noite. O clamp em 4.0 garante que o
// cubemap de ambiente nunca estoure o tonemap/lave os objetos de branco.
static const char* s_SkyFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform vec3 u_SunDir;   // aponta DA superfície PRA o sol (oposto de DirectionalLight::Direction)
uniform vec3 u_SunColor;

const vec3 HORIZON = vec3(0.14, 0.24, 0.44);
const vec3 ZENITH  = vec3(0.05, 0.14, 0.42);
const vec3 GROUND  = vec3(0.07, 0.08, 0.10);
const vec3 SUNSET  = vec3(1.00, 0.42, 0.14);

void main() {
    vec3 dir = normalize(v_LocalPos);
    vec3 sun = normalize(u_SunDir);
    float sunDot = max(dot(dir, sun), 0.0);
    float sunLift = max(sun.y, 0.0);
    float view = dir.y;

    // Gradiente base (comprovado — não infla o IBL).
    vec3 sky = (view >= 0.0)
        ? mix(HORIZON, ZENITH, smoothstep(-0.05, 0.35, view))
        : mix(HORIZON, GROUND, smoothstep(0.0, 0.6, -view));

    // Atmosfera: brilho quente no horizonte quando o sol está baixo (pôr-do-sol)
    // e um azul mais rico no zênite (Rayleigh).
    float horizonGlow = pow(max(1.0 - abs(view), 0.0), 2.5);
    float sunsetAmount = 1.0 - smoothstep(0.03, 0.55, sunLift);
    sky += SUNSET * horizonGlow * sunsetAmount * 0.14;
    sky += vec3(0.02, 0.05, 0.10) * smoothstep(0.3, 1.0, view) * (0.4 + sunLift);

    // Sol + halo (compactos, como o original).
    sky += u_SunColor * pow(sunDot, 900.0) * 12.0;
    sky += u_SunColor * pow(sunDot, 16.0) * 0.10;

    // Estrelas quando o sol está abaixo do horizonte.
    if (sunLift < 0.06) {
        vec3 sp = dir * 600.0;
        vec2 cell = floor(sp.xy);
        vec2 f = fract(sp.xy) - 0.5;
        float star = smoothstep(0.48, 0.5, 1.0 - length(f));
        star *= step(0.997, fract(sin(dot(cell, vec2(12.9898, 78.233))) * 43758.5453));
        sky += vec3(0.85, 0.9, 1.0) * star * (1.0 - sunLift * 16.0) * 0.4;
    }

    sky = min(sky, vec3(4.0)); // nunca estoura o IBL
    o_Color = vec4(sky, 1.0);
}
)";

// Convolução de irradiância: integra cosseno-ponderado sobre a hemisfera
// ao redor de cada normal (aqui, cada direção do cubo de destino), lendo o
// cubemap de ambiente já pronto. sampleDelta = 0.05 é deliberadamente
// grosso (~2500 amostras/texel) — aceitável porque isso roda só uma vez,
// numa textura pequena (32x32/face), no bake de Init(), nunca por frame.
static const char* s_IrradianceFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;

const float PI = 3.14159265359;

void main() {
    vec3 N = normalize(v_LocalPos);
    vec3 up = abs(N.y) < 0.999 ? vec3(0.0, 1.0, 0.0) : vec3(0.0, 0.0, 1.0);
    vec3 right = normalize(cross(up, N));
    up = normalize(cross(N, right));

    vec3 irradiance = vec3(0.0);
    float sampleDelta = 0.05;
    float nrSamples = 0.0;
    for (float phi = 0.0; phi < 2.0 * PI; phi += sampleDelta) {
        for (float theta = 0.0; theta < 0.5 * PI; theta += sampleDelta) {
            vec3 tangentSample = vec3(sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta));
            vec3 sampleVec = tangentSample.x * right + tangentSample.y * up + tangentSample.z * N;
            irradiance += texture(u_EnvironmentMap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples += 1.0;
        }
    }
    irradiance = PI * irradiance / nrSamples;
    o_Color = vec4(irradiance, 1.0);
}
)";

// Pré-filtragem especular GGX: pra cada mip (= uma faixa de rugosidade),
// faz importance sampling do ambiente ponderado pela distribuição GGX
// daquela rugosidade. Mip 0 (rugosidade 0) degenera pra praticamente um
// espelho; mips mais altos ficam progressivamente mais borrados. 32
// amostras por texel é pouco pra padrão AAA, mas o bake acontece uma vez
// só, então o trade-off (leve ruído em rugosidade média em troca de bake
// rápido) é aceitável pra v1 — mais amostras é ajuste de uma linha se
// algum dia isso incomodar visualmente.
static const char* s_PrefilterFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;
uniform float u_Roughness;

const float PI = 3.14159265359;

float DistributionGGX(vec3 N, vec3 H, float roughness) {
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    return a2 / (PI * denom * denom + 0.0001);
}

// Sequência de Hammersley (Van der Corput em base 2) — sequência
// quase-aleatória de baixa discrepância, cobre a hemisfera mais
// uniformemente que amostragem aleatória pura com o mesmo número de
// amostras.
float RadicalInverse_VdC(uint bits) {
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10;
}
vec2 Hammersley(uint i, uint N) {
    return vec2(float(i) / float(N), RadicalInverse_VdC(i));
}

vec3 ImportanceSampleGGX(vec2 Xi, vec3 N, float roughness) {
    float a = roughness * roughness;
    float phi = 2.0 * PI * Xi.x;
    float cosTheta = sqrt((1.0 - Xi.y) / (1.0 + (a * a - 1.0) * Xi.y));
    float sinTheta = sqrt(1.0 - cosTheta * cosTheta);

    vec3 H = vec3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);

    vec3 up = abs(N.z) < 0.999 ? vec3(0.0, 0.0, 1.0) : vec3(1.0, 0.0, 0.0);
    vec3 tangent = normalize(cross(up, N));
    vec3 bitangent = cross(N, tangent);
    return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

void main() {
    vec3 N = normalize(v_LocalPos);
    vec3 R = N;
    vec3 V = R;

    const uint SAMPLE_COUNT = 32u;
    vec3 prefilteredColor = vec3(0.0);
    float totalWeight = 0.0;

    for (uint i = 0u; i < SAMPLE_COUNT; ++i) {
        vec2 Xi = Hammersley(i, SAMPLE_COUNT);
        vec3 H = ImportanceSampleGGX(Xi, N, u_Roughness);
        vec3 L = normalize(2.0 * dot(V, H) * H - V);

        float NdotL = max(dot(N, L), 0.0);
        if (NdotL > 0.0) {
            prefilteredColor += texture(u_EnvironmentMap, L).rgb * NdotL;
            totalWeight += NdotL;
        }
    }
    prefilteredColor = totalWeight > 0.0 ? prefilteredColor / totalWeight : vec3(0.0);
    o_Color = vec4(prefilteredColor, 1.0);
}
)";

// Skybox de verdade (desenhado por frame): mesmo cubo, mas com a
// profundidade forçada pro plano mais distante possível (gl_Position.z =
// gl_Position.w) e comparação de profundidade LEQUAL — assim ele só
// aparece onde nenhuma outra geometria já desenhou, sem precisar desenhar
// primeiro nem desligar o teste de profundidade.
static const char* s_SkyboxVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_View;       // rotação só (translação removida em C++)
uniform mat4 u_Projection;

out vec3 v_LocalPos;

void main() {
    v_LocalPos = a_Position;
    vec4 pos = u_Projection * u_View * vec4(a_Position, 1.0);
    gl_Position = pos.xyww;
}
)";

static const char* s_SkyboxFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform samplerCube u_EnvironmentMap;
uniform vec3 u_SunDirection;   // direção que APONTA para o sol, normalizada
uniform bool u_AtmosphereSky;  // 1 = scattering físico (sem HDRI); 0 = amostra o cubemap

const float PI = 3.14159265359;
// Atmosfera em unidades NORMALIZADAS (raio do planeta = 1.0) pra evitar
// precisão de ponto flutuante em metros (o dot(o,o) com raio em metros ~4e13
// estoura float32 na interseção). Os coeficientes são re-escalados pela
// mesma razão: β_norm = β_m⁻¹ * R_planeta_m, H_norm = H_m / R_planeta_m.
// R_planeta=6371km, atmosfera=100km de espessura.
const float kPlanetRadius      = 1.0;
const float kAtmosphereRadius  = 1.0157;
const float kScaleHeightR      = 0.00125; // ~8 km (Rayleigh, azul)
const float kScaleHeightM      = 0.00019; // ~1.2 km (Mie, haze)
const vec3  kBetaR             = vec3(35.0, 83.0, 143.0); // Rayleigh β (normalizado)
const vec3  kBetaM             = vec3(134.0);              // Mie β (normalizado)
const float kSunIntensity      = 0.6;
const float kMieG              = 0.82;

// Interseção raio-esfera. Devolve a distância de ENTRADA (ou -1 se não bate).
float IntersectSphere(vec3 o, vec3 d, float r) {
    float b = dot(d, o);
    float c = dot(o, o) - r * r;
    float det = b * b - c;
    if (det < 0.0) return -1.0;
    det = sqrt(det);
    return -b - det;
}

// Espalhamento single-scattering de Rayleigh + Mie, marchando o raio de visão
// dentro da atmosfera e a luz do sol até cada ponto. Todos os loops têm TETO
// CONSTANTE (#define de compilação) — compila em GLSL 330 core em qualquer
// driver (nada de loop de comprimento variável).
vec3 ComputeAtmosphere(vec3 rd) {
    vec3 sun = u_SunDirection;
    vec3 origin = vec3(0.0, kPlanetRadius, 0.0); // observador na superfície
    float tIn = IntersectSphere(origin, rd, kAtmosphereRadius);
    if (tIn < 0.0) return vec3(0.0); // fora da atmosfera (não deveria aparecer no skybox)
    float tPlanet = IntersectSphere(origin, rd, kPlanetRadius);
    float tMax = (tPlanet > 0.0) ? tPlanet : tIn;
    float tFar = min(tMax, 200.0);

    const int kSteps = 16;
    const int kLightSteps = 8;
    float dt = tFar / float(kSteps);
    float t = dt * 0.5;

    vec3 inScatter = vec3(0.0);
    for (int i = 0; i < kSteps; ++i) {
        vec3 pos = origin + rd * t;
        float h = max(length(pos) - kPlanetRadius, 0.0);
        vec3 beta = kBetaR * exp(-h / kScaleHeightR) + kBetaM * exp(-h / kScaleHeightM);

        // Marcha da luz do sol até este ponto (transmitância).
        vec3 sunPos = pos + sun * (kAtmosphereRadius * 2.0);
        float tSun = IntersectSphere(sunPos, -sun, kAtmosphereRadius);
        float dtSun = tSun / float(kLightSteps);
        float ts = dtSun * 0.5;
        vec3 sunDepth = vec3(0.0);
        for (int j = 0; j < kLightSteps; ++j) {
            vec3 sp = sunPos - sun * ts;
            float sh = max(length(sp) - kPlanetRadius, 0.0);
            sunDepth += (kBetaR * exp(-sh / kScaleHeightR) + kBetaM * exp(-sh / kScaleHeightM)) * dtSun;
            ts += dtSun;
        }
        inScatter += exp(-sunDepth) * dt;
        t += dt;
    }

    float mu = dot(rd, sun);
    float phaseR = (3.0 / (16.0 * PI)) * (1.0 + mu * mu);
    float g2 = kMieG * kMieG;
    float phaseM = (3.0 / (8.0 * PI)) * (1.0 - g2) * (1.0 + mu * mu) /
                   ((2.0 + g2) * pow(max(1.0 + g2 - 2.0 * kMieG * mu, 0.0001), 1.5));

    vec3 color = inScatter * (kBetaR * phaseR + kBetaM * phaseM) * kSunIntensity;

    // Disco do sol (brilho > 1 vira "combustível" do bloom) + halo quente Mie.
    color += vec3(1.0, 0.92, 0.75) * pow(max(mu, 0.0), 1800.0) * 40.0;
    color += vec3(1.0, 0.62, 0.30) * pow(max(mu, 0.0), 28.0) * 0.6;

    // Estrelas celulares fracas no lado noturno (estáveis, sem shimmer).
    float nightFactor = smoothstep(-0.03, -0.5, mu);
    vec3 cell = floor(rd * 170.0);
    float starHash = fract(sin(dot(cell, vec3(127.1, 311.7, 74.7))) * 43758.5453);
    color += vec3(step(0.996, starHash)) * nightFactor * 1.2;

    return color;
}

void main() {
    vec3 rd = normalize(v_LocalPos);
    vec3 color;
    if (u_AtmosphereSky) {
        // Sem tonemap aqui de propósito — o sol sai bem acima de 1.0 e vira
        // combustível pro bright-pass do bloom no passe de composição.
        color = ComputeAtmosphere(rd);
    } else {
        color = texture(u_EnvironmentMap, rd).rgb;
    }
    if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0); // guarda anti-NaN
    o_Color = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------
// Pós-processamento: quad de tela cheia + bright-pass + blur + composição.
// ---------------------------------------------------------------------
static const char* s_FullscreenVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 a_Position;
layout(location = 1) in vec2 a_TexCoord;
out vec2 v_TexCoord;
void main() {
    v_TexCoord = a_TexCoord;
    gl_Position = vec4(a_Position, 0.0, 1.0);
}
)";

// Extrai só os pixels acima do limiar (com "joelho" suave, não corte duro) — o que sobra
// vira a semente do glow do bloom depois de borrado.
static const char* s_BrightPassFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 o_Color;
uniform sampler2D u_SceneColor;
uniform float u_Threshold;

void main() {
    vec3 color = texture(u_SceneColor, v_TexCoord).rgb;
    if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0); // guarda anti-NaN
    float brightness = max(color.r, max(color.g, color.b));
    const float knee = 0.1; // faixa fixa e pequena, só pra suavizar a borda — não desloca o corte de verdade
    float soft = clamp(brightness - u_Threshold + knee, 0.0, 2.0 * knee);
    soft = (soft * soft) / (4.0 * knee + 0.0001);
    float contribution = max(soft, brightness - u_Threshold) / max(brightness, 0.0001);
    o_Color = vec4(color * contribution, 1.0);
}
)";

// Blur gaussiano separável 9-tap — duas passadas (horizontal, vertical) por iteração,
// alternando entre os dois FBOs de ping-pong.
static const char* s_BlurFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 o_Color;
uniform sampler2D u_Image;
uniform bool u_Horizontal;
const float u_Weights[5] = float[](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);

void main() {
    vec2 texelSize = 1.0 / vec2(textureSize(u_Image, 0));
    vec3 result = texture(u_Image, v_TexCoord).rgb * u_Weights[0];
    vec2 dir = u_Horizontal ? vec2(texelSize.x, 0.0) : vec2(0.0, texelSize.y);
    for (int i = 1; i < 5; ++i) {
        result += texture(u_Image, v_TexCoord + dir * float(i)).rgb * u_Weights[i];
        result += texture(u_Image, v_TexCoord - dir * float(i)).rgb * u_Weights[i];
    }
    o_Color = vec4(result, 1.0);
}
)";

// Soma cena HDR + bloom, aplica oclusão + exposição, tonemap ACES, gamma e o
// "pós-cinema" sutil: aberração cromática, vinheta e grão de filme animado
// (pós-tonemap — não afeta o IBL, então não lava nada).
static const char* s_CompositeFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 o_Color;
uniform sampler2D u_SceneColor;
uniform sampler2D u_BloomBlur;
uniform sampler2D u_AOTexture;
uniform bool u_HasAO;
uniform sampler2D u_SSRTexture;
uniform bool u_HasSSR;
uniform sampler2D u_GodRaysTexture;
uniform bool u_HasGodRays;
uniform float u_BloomIntensity;
uniform float u_Exposure;
uniform float u_Vignette;
uniform float u_ChromaticAberration;
uniform float u_FilmGrain;
uniform float u_Time;
uniform int u_ToneMapping; // 0=ACES, 1=Reinhard, 2=Filmic

vec3 ACESFilm(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}
vec3 Reinhard(vec3 x) { return x / (1.0 + x); }
vec3 Filmic(vec3 x) {
    vec3 xx = max(x - vec3(0.004), vec3(0.0));
    return (xx * (6.2 * xx + 0.5)) / (xx * (6.2 * xx + 1.7) + 0.06);
}

void main() {
    vec2 uv = v_TexCoord;
    vec2 dir = uv - 0.5;
    vec2 caDir = length(dir) > 0.0001 ? normalize(dir) : vec2(0.0, 1.0);
    float ca = u_ChromaticAberration * length(dir) * 4.0;
    vec3 hdr;
    hdr.r = texture(u_SceneColor, uv + caDir * ca).r;
    hdr.g = texture(u_SceneColor, uv).g;
    hdr.b = texture(u_SceneColor, uv - caDir * ca).b;

    vec3 bloom = texture(u_BloomBlur, uv).rgb;
    vec3 color = hdr + bloom * u_BloomIntensity;
    if (u_HasSSR) {
        // Guarda anti-NaN: um reflexo degenerado não pode envenenar o frame
        // inteiro com NaN (tela preta).
        vec3 ssr = texture(u_SSRTexture, uv).rgb;
        if (any(isnan(ssr)) || any(isinf(ssr))) ssr = vec3(0.0);
        color += ssr;
    }
    if (u_HasGodRays) {
        vec3 gr = texture(u_GodRaysTexture, uv).rgb;
        if (any(isnan(gr)) || any(isinf(gr))) gr = vec3(0.0);
        color += gr;
    }
    if (u_HasAO) color *= texture(u_AOTexture, uv).r;
    // Guarda final: NENHUM passe de pós pode produzir NaN/Inf e apagar a tela.
    if (any(isnan(color)) || any(isinf(color))) color = vec3(0.0);
    color *= u_Exposure;
    if (u_ToneMapping == 1) color = Reinhard(color);
    else if (u_ToneMapping == 2) color = clamp(Filmic(color), 0.0, 1.0);
    else color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));

    float vig = 1.0 - u_Vignette * smoothstep(0.55, 1.35, length(dir * 2.0));
    color *= clamp(vig, 0.0, 1.0);

    if (u_FilmGrain > 0.0) {
        float g = fract(sin(dot(uv * vec2(1920.0, 1080.0), vec2(12.9898, 78.233)) + u_Time * 60.0) * 43758.5453) - 0.5;
        color += g * u_FilmGrain;
    }

    o_Color = vec4(color, 1.0);
}
)";

// ---------------------------------------------------------------------
// God rays / luz volumétrica em espaço de tela. Cada pixel marcha do próprio
// ponto até a posição do SOL na tela (u_SunUV), acumulando a cor brilhante
// da cena com atenuação (density/decay). O loop tem TETO CONSTANTE
// (GODRAY_MAX_STEPS é #define fixo) — compila em GLSL 330 core em qualquer
// driver. Limitação conhecida do método em espaço de tela: raios "atravessam"
// obstáculos finos (sem depth-aware); o default deixa o efeito sutil.
// ---------------------------------------------------------------------
static const char* s_GodRaysFragmentSrc = R"(
#version 330 core
#define GODRAY_MAX_STEPS 28

in vec2 v_TexCoord;
out vec4 o_Color;

uniform sampler2D u_SceneColor;
uniform vec2 u_SunUV;        // posição do sol em UV [0,1]
uniform float u_Density;
uniform float u_Decay;
uniform float u_Weight;
uniform float u_Intensity;

void main() {
    vec2 delta = u_SunUV - v_TexCoord;
    float dist = length(delta);
    // Sol fora da tela ou colado no pixel: sem raios.
    if (dist < 0.0001 || dist > 1.6) { o_Color = vec4(0.0); return; }

    vec2 stepUV = delta / float(GODRAY_MAX_STEPS);
    vec2 uv = v_TexCoord;
    float illuminationDecay = 1.0;
    vec3 sum = vec3(0.0);
    for (int i = 0; i < GODRAY_MAX_STEPS; ++i) {
        uv += stepUV;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) break;
        vec3 c = texture(u_SceneColor, uv).rgb;
        if (any(isnan(c)) || any(isinf(c))) c = vec3(0.0);
        sum += c * u_Density * illuminationDecay;
        illuminationDecay *= u_Decay;
    }
    o_Color = vec4(sum * u_Weight * u_Intensity, 1.0);
}
)";

// ---------------------------------------------------------------------
// SSAO (oclusão de ambiente em espaço de tela) — amostra a vizinhança de
// cada pixel num hemisfério orientado pela normal reconstruída do depth e
// mede quanta geometria oculta ele. A saída é um valor de oclusão em
// [0,1], borrado depois pra esconder o ruído da amostragem (os 4x4 vetores
// aleatórios u_Noise quebram a regularidade do kernel).
// ---------------------------------------------------------------------
static const char* s_SSAOFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out float o_AO;

uniform sampler2D u_Depth;
uniform mat4 u_Projection;
uniform mat4 u_InverseProjection;
uniform float u_Radius;
uniform float u_Power;
uniform int u_SampleCount;
uniform vec3 u_Samples[64];
uniform sampler2D u_Noise;
uniform vec2 u_NoiseScale;

// Reconstrução do ponto de vista do depth não-linear: mapeia a profundidade
// gravada de volta pro espaço de view via a inversa da projeção.
vec3 ReconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = u_InverseProjection * clip;
    return view.xyz / view.w;
}

void main() {
    float depth = texture(u_Depth, v_TexCoord).r;
    if (depth >= 0.9999) { o_AO = 1.0; return; } // fundo (sem geometria) não oclui
    vec3 fragPos = ReconstructViewPos(v_TexCoord, depth);

    // Normal por derivadas de tela da posição de view (barata, sem G-buffer).
    vec3 normal = normalize(cross(dFdx(fragPos), dFdy(fragPos)));

    // Base de tangente aleatória por pixel pra girar o kernel e esconder bandas.
    vec3 randomVec = normalize(texture(u_Noise, v_TexCoord * u_NoiseScale).xyz);
    vec3 tangent = normalize(randomVec - normal * dot(randomVec, normal));
    vec3 bitangent = cross(normal, tangent);
    mat3 TBN = mat3(tangent, bitangent, normal);

    float occlusion = 0.0;
    for (int i = 0; i < u_SampleCount; ++i) {
        vec3 samplePos = TBN * u_Samples[i];
        samplePos = fragPos + samplePos * u_Radius;

        vec4 offset = vec4(samplePos, 1.0);
        offset = u_Projection * offset;
        offset.xyz /= offset.w;
        offset.xyz = offset.xyz * 0.5 + 0.5;

        float sampleDepth = texture(u_Depth, offset.xy).r;
        // rangeCheck descarta amostras de fundo que "vazam" pra frente da silhueta
        float rangeCheck = smoothstep(0.0, 1.0, u_Radius / abs(fragPos.z - ReconstructViewPos(offset.xy, sampleDepth).z));
        occlusion += (sampleDepth >= offset.z + 0.03 ? 1.0 : 0.0) * rangeCheck;
    }
    occlusion = 1.0 - occlusion / float(u_SampleCount);
    // Teto de escurecimento: mesmo que a estimativa de oclusão quebre (ex.:
    // reconstrução de profundidade imprecisa em algumas GPUs, que dava ~0.1
    // e escurecia a cena ~10x), o AO nunca passa de 0.35 — o efeito fica
    // visível mas não deixa a cena escura.
    o_AO = clamp(pow(occlusion, u_Power), 0.35, 1.0);
}
)";

// ---------------------------------------------------------------------
// SSR — reflexos em espaço de tela (GLSL 330 core SEGURO). Para cada pixel,
// projeta o raio refletido (normal reconstruída do depth) no espaço de tela
// e marcha contra o depth buffer até acertar geometria; a cor do impacto
// vira a reflexão (com Fresnel — ângulos rasantes refletem mais).
//
// A diferença pro SSR antigo (que exigia GL 4.0+): o loop de marcha tem
// TETO CONSTANTE (SSR_MAX_STEPS é #define fixo) e o número de passos é
// clampado nele. Em GLSL 3.30 core o compilador exige loops com contagem
// determinável em compile-time; o loop antigo derivava o teto de um
// uniform (comprimento VARIÁVEL) e por isso só rodava em 4.x. Aqui o
// `for (int i = 0; i < SSR_MAX_STEPS; ++i)` é unrollável e compila em
// qualquer driver 3.3 (Wine, iGPU, VM) — sem risco de viewport preto.
// ---------------------------------------------------------------------
static const char* s_SSRFragmentSrc = R"(
#version 330 core
#define SSR_MAX_STEPS 48

in vec2 v_TexCoord;
out vec4 o_Color;

uniform sampler2D u_SceneColor;
uniform sampler2D u_Depth;
uniform mat4 u_Projection;
uniform mat4 u_InverseProjection;
uniform int u_MaxSteps;      // clampado em [8, SSR_MAX_STEPS] no C++
uniform float u_Thickness;
uniform float u_Intensity;
uniform float u_MarchDistance;
uniform vec2 u_ViewportSize;

vec3 ReconstructViewPos(vec2 uv, float depth) {
    vec4 clip = vec4(uv * 2.0 - 1.0, depth * 2.0 - 1.0, 1.0);
    vec4 view = u_InverseProjection * clip;
    return view.xyz / view.w;
}

void main() {
    o_Color = vec4(0.0);
    float depth = texture(u_Depth, v_TexCoord).r;
    if (depth >= 0.9999) return; // céu: já tem reflexo do IBL, não duplica

    vec3 fragPos = ReconstructViewPos(v_TexCoord, depth);
    vec3 n = cross(dFdx(fragPos), dFdy(fragPos));
    // Silhuetas/bordas podem ter derivadas quase paralelas -> cross ~0 ->
    // normalize() vira NaN, que POISONA o composite (tela preta). Descarta.
    if (dot(n, n) < 1e-8) return;
    vec3 normal = normalize(n);
    if (dot(normal, -fragPos) < 0.001) return; // face voltada pra longe da câmera

    vec3 viewDir = normalize(fragPos);
    vec3 reflDir = reflect(viewDir, normal);

    // Origem levemente afastada da superfície pra não "auto-acertar" o próprio pixel.
    vec3 rayOrigin = fragPos + reflDir * 0.05;

    vec4 startClip = u_Projection * vec4(rayOrigin, 1.0);
    vec4 endClip   = u_Projection * vec4(rayOrigin + reflDir * u_MarchDistance, 1.0);
    if (startClip.w <= 0.0 || endClip.w <= 0.0) return; // algum ponto atrás da câmera

    vec2 startNDC = startClip.xy / startClip.w;
    vec2 endNDC   = endClip.xy / endClip.w;
    vec2 deltaNDC = endNDC - startNDC;

    // Passos proporcionais à distância em pixels do raio na tela, SEMPRE
    // clampado no teto constante — a contagem fica determinística.
    float pixelDist = length(deltaNDC * 0.5 * u_ViewportSize);
    int steps = clamp(int(pixelDist / 2.0), 4, min(max(u_MaxSteps, 4), SSR_MAX_STEPS));
    if (steps < 2) return;

    vec2 stepNDC = deltaNDC / float(steps);
    float z0 = startClip.z / startClip.w;         // z linear de view (negativo)
    float zStep = (endClip.z / endClip.w - z0) / float(steps);

    vec2 uv = startNDC * 0.5 + 0.5;
    float rayZ = z0;
    for (int i = 0; i < SSR_MAX_STEPS; ++i) {
        if (i >= steps) break; // teto constante; saída antecipada sem loop variável
        uv += stepNDC * 0.5;
        rayZ += zStep;
        if (uv.x < 0.0 || uv.x > 1.0 || uv.y < 0.0 || uv.y > 1.0) return;

        float hitDepth = texture(u_Depth, uv).r;
        if (hitDepth >= 0.9999) continue; // ainda voando sobre o céu

        vec3 hitPos = ReconstructViewPos(uv, hitDepth);
        float diff = hitPos.z - rayZ;
        if (diff < 0.0 && abs(diff) < u_Thickness) {
            vec3 hitColor = texture(u_SceneColor, uv).rgb;
            if (any(isnan(hitColor)) || any(isinf(hitColor))) hitColor = vec3(0.0);
            float fresnel = pow(1.0 - clamp(dot(normal, -viewDir), 0.0, 1.0), 5.0);
            float strength = mix(0.04, 1.0, fresnel);
            o_Color = vec4(clamp(hitColor * strength * u_Intensity, 0.0, 32.0), 1.0);
            return;
        }
    }
}
)";

// ---------------------------------------------------------------------
// TAA — anti-aliasing temporal. O composite escreve LDR num alvo
// intermediário (s_TAAComposite); aqui misturamos com o histórico do frame
// anterior usando um clamp em caixa (AABB dos 3x3 vizinhos do frame atual)
// pra não vazar cores/ghosting. Roda em LDR (pós-tonemap), 100% GLSL 330.
// Sem velocity buffer nesta v1: o clamp de vizinhança limita o ghosting em
// movimento de câmera; com câmera parada a convergência é total.
// ---------------------------------------------------------------------
static const char* s_TAAFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 o_Color;

uniform sampler2D u_Current;
uniform sampler2D u_History;
uniform vec2 u_InvResolution;
uniform float u_HistoryWeight; // 0 = frame atual (primeiro frame)
uniform bool u_UseHistory;

void main() {
    vec2 uv = v_TexCoord;
    vec3 cur = texture(u_Current, uv).rgb;
    vec3 hist = texture(u_History, uv).rgb;

    if (!u_UseHistory) {
        o_Color = vec4(cur, 1.0);
        return;
    }

    // AABB de vizinhança 3x3 (mais os 4 em cruz do 5x5) do frame ATUAL:
    // o histórico é clampado dentro dessa caixa, então um pixel novo muito
    // diferente (objeto/movimento) puxa o histórico pra si sem vazar cor.
    vec2 o = u_InvResolution;
    vec3 c00 = texture(u_Current, uv + vec2(-o.x, -o.y)).rgb;
    vec3 c10 = texture(u_Current, uv + vec2(0.0,  -o.y)).rgb;
    vec3 c20 = texture(u_Current, uv + vec2(o.x,  -o.y)).rgb;
    vec3 c01 = texture(u_Current, uv + vec2(-o.x, 0.0)).rgb;
    vec3 c21 = texture(u_Current, uv + vec2(o.x,  0.0)).rgb;
    vec3 c02 = texture(u_Current, uv + vec2(-o.x, o.y)).rgb;
    vec3 c12 = texture(u_Current, uv + vec2(0.0,  o.y)).rgb;
    vec3 c22 = texture(u_Current, uv + vec2(o.x,  o.y)).rgb;

    vec3 mn = min(min(min(c00, c10), min(c20, c01)), min(min(c21, c02), min(c12, c22)));
    vec3 mx = max(max(max(c00, c10), max(c20, c01)), max(max(c21, c02), max(c12, c22)));
    // Amplia a caixa em ~2% pra evitar "shimmer" no limite de saturação.
    vec3 ext = (mx - mn) * 0.02;
    mn -= ext;
    mx += ext;

    vec3 clmp = clamp(hist, mn, mx);
    vec3 result = mix(cur, clmp, u_HistoryWeight);
    o_Color = vec4(result, 1.0);
}
)";

// Converte equirectangular (latitude/longitude, textura 2D) pra cubemap —
// roda no bake do ambiente quando o usuário carrega um .hdr de céu.
static const char* s_EquirectFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform sampler2D u_Equirect;

const vec2 invAtan = vec2(0.1591, 0.3183);
vec2 SampleSphericalMap(vec3 v) {
    vec2 uv = vec2(atan(v.z, v.x), asin(v.y));
    uv *= invAtan;
    uv += 0.5;
    return uv;
}

void main() {
    vec2 uv = SampleSphericalMap(normalize(v_LocalPos));
    o_Color = vec4(texture(u_Equirect, uv).rgb, 1.0);
}
)";

// ---------------------------------------------------------------------
// Partículas: quad billboard (sempre de frente pra câmera, via u_CameraRight/Up)
// instanciado — atributos 0/1 vêm do quad estático, 2/3/4 vêm do buffer de
// instância (glVertexAttribDivisor=1, um valor por partícula, não por vértice).
// ---------------------------------------------------------------------
static const char* s_ParticleVertexSrc = R"(
#version 330 core
layout(location = 0) in vec2 a_LocalPos;
layout(location = 1) in vec2 a_TexCoord;
layout(location = 2) in vec3 a_InstancePos;
layout(location = 3) in float a_InstanceSize;
layout(location = 4) in vec4 a_InstanceColor;

uniform mat4 u_ViewProjection;
uniform vec3 u_CameraRight;
uniform vec3 u_CameraUp;

out vec2 v_TexCoord;
out vec4 v_Color;

void main() {
    vec3 worldPos = a_InstancePos + (u_CameraRight * a_LocalPos.x + u_CameraUp * a_LocalPos.y) * a_InstanceSize;
    gl_Position = u_ViewProjection * vec4(worldPos, 1.0);
    v_TexCoord = a_TexCoord;
    v_Color = a_InstanceColor;
}
)";

// Sem textura de propósito (v1): um degradê radial suave já dá uma partícula redonda
// decente sem precisar de nenhum asset — mesma filosofia do céu procedural.
// Textura opcional de partícula (uniform): sem textura usa o degradê radial
// procedural (v1); com textura, a cor vem dela (alpha = intensidade).
static const char* s_ParticleFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
in vec4 v_Color;
out vec4 o_Color;

uniform sampler2D u_ParticleTexture;
uniform bool u_HasTexture;

void main() {
    vec4 tex = vec4(1.0);
    if (u_HasTexture) tex = texture(u_ParticleTexture, v_TexCoord);
    float dist = length(v_TexCoord - vec2(0.5)) * 2.0;
    float falloff = 1.0 - smoothstep(0.0, 1.0, dist);
    falloff *= falloff;
    o_Color = vec4(v_Color.rgb * tex.rgb, v_Color.a * tex.a * falloff);
}
)";

// Shader do passe de sombra: só precisa escrever profundidade, não tem
// saída de cor nenhuma. Com skinning (u_Animated) a pose animada do
// personagem projeta a sombra certa — antes ficava na pose de repouso.
static const char* s_ShadowVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 3) in vec4 a_JointIndices;
layout(location = 4) in vec4 a_JointWeights;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;
uniform bool u_Animated;
uniform mat4 u_JointMatrices[64];

void main() {
    vec3 pos = a_Position;
    if (u_Animated) {
        mat4 skin = u_JointMatrices[int(a_JointIndices.x)] * a_JointWeights.x
                  + u_JointMatrices[int(a_JointIndices.y)] * a_JointWeights.y
                  + u_JointMatrices[int(a_JointIndices.z)] * a_JointWeights.z
                  + u_JointMatrices[int(a_JointIndices.w)] * a_JointWeights.w;
        pos = vec3(skin * vec4(a_Position, 1.0));
    }
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(pos, 1.0);
}
)";

static const char* s_ShadowFragmentSrc = R"(
#version 330 core
void main() { }
)";

static const char* s_LineVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Color;

uniform mat4 u_ViewProjection;

out vec3 v_Color;

void main() {
    v_Color = a_Color;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

static const char* s_LineFragmentSrc = R"(
#version 330 core
layout(location = 0) out vec4 o_Color;

in vec3 v_Color;

void main() {
    o_Color = vec4(v_Color, 1.0);
}
)";

void Renderer3D::Init() {
    KZ_TRACE_SCOPE("Renderer3D::Init");

    // Auto-tune por hardware: extrai o máximo de cada versão do OpenGL
    // (3.3 conservador, 4.0+ agressivo, 4.5+ teto). O editor pode
    // sobrescrever via settings.json.
    s_Settings.TuneToHardware();

    s_MeshShader = CreateRef<Shader>("Renderer3D_Mesh", s_MeshVertexSrc, s_MeshFragmentSrc);
    s_LineShader = CreateRef<Shader>("Renderer3D_Line", s_LineVertexSrc, s_LineFragmentSrc);

    // Grid fixo de 100x100 unidades, uma linha por unidade — construído
    // uma vez aqui e reutilizado em todo DrawGrid(). Cor cinza padrão, com
    // a linha em x=0 pintada de azul (eixo Z) e a linha em z=0 de vermelho
    // (eixo X), pra dar noção de orientação além de escala.
    const float halfSize = 50.0f;
    const glm::vec3 gridColor(0.32f, 0.32f, 0.35f);
    const glm::vec3 xAxisColor(0.75f, 0.25f, 0.25f);
    const glm::vec3 zAxisColor(0.25f, 0.4f, 0.8f);

    struct LineVertex { glm::vec3 Position; glm::vec3 Color; };
    std::vector<LineVertex> verts;
    verts.reserve((size_t)(halfSize * 2 + 1) * 4);

    for (int i = -(int)halfSize; i <= (int)halfSize; ++i) {
        float x = (float)i;
        glm::vec3 c = (i == 0) ? zAxisColor : gridColor; // x=0 é o eixo Z
        verts.push_back({ { x, 0.0f, -halfSize }, c });
        verts.push_back({ { x, 0.0f,  halfSize }, c });
    }
    for (int i = -(int)halfSize; i <= (int)halfSize; ++i) {
        float z = (float)i;
        glm::vec3 c = (i == 0) ? xAxisColor : gridColor; // z=0 é o eixo X
        verts.push_back({ { -halfSize, 0.0f, z }, c });
        verts.push_back({ {  halfSize, 0.0f, z }, c });
    }

    s_GridVertexCount = (uint32_t)verts.size();
    s_GridVAO = CreateRef<VertexArray>();
    auto vb = CreateRef<VertexBuffer>((float*)verts.data(), (uint32_t)(verts.size() * sizeof(LineVertex)));
    vb->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float3, "a_Color" },
    });
    s_GridVAO->AddVertexBuffer(vb);

    // Framebuffer de sombra: só profundidade, sem color attachment — uma cópia por cascata.
    // O tamanho vem das configurações gráficas (shadow map size do preset).
    s_ShadowShader = CreateRef<Shader>("Renderer3D_Shadow", s_ShadowVertexSrc, s_ShadowFragmentSrc);
    EnsureShadowMaps((uint32_t)s_Settings.ShadowMapSize);

    // Shadow map de luz pontual (depth cubemap com profundidade LINEAR).
    s_EquirectShader = CreateRef<Shader>("Renderer3D_Equirect", s_CaptureVertexSrc, s_EquirectFragmentSrc);

    GenerateEnvironment();

    // Quad de tela cheia (NDC -1..1) reaproveitado pelos 3 passes de pós-processamento.
    float quadVertices[] = {
        -1.0f,  1.0f,  0.0f, 1.0f,
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
    };
    uint32_t quadIndices[] = { 0, 1, 2, 2, 3, 0 };
    s_FullscreenQuad = CreateRef<VertexArray>();
    auto quadVB = CreateRef<VertexBuffer>(quadVertices, sizeof(quadVertices));
    quadVB->SetLayout({ { ShaderDataType::Float2, "a_Position" }, { ShaderDataType::Float2, "a_TexCoord" } });
    s_FullscreenQuad->AddVertexBuffer(quadVB);
    s_FullscreenQuad->SetIndexBuffer(CreateRef<IndexBuffer>(quadIndices, 6));

    s_BrightPassShader = CreateRef<Shader>("Renderer3D_BrightPass", s_FullscreenVertexSrc, s_BrightPassFragmentSrc);
    s_BlurShader = CreateRef<Shader>("Renderer3D_Blur", s_FullscreenVertexSrc, s_BlurFragmentSrc);
    s_CompositeShader = CreateRef<Shader>("Renderer3D_Composite", s_FullscreenVertexSrc, s_CompositeFragmentSrc);

    // SSAO: kernel de hemisfério (64 amostras) + textura de ruído 4x4 que
    // gira o kernel por pixel e esconde o padrão de amostragem.
    s_SSAOShader = CreateRef<Shader>("Renderer3D_SSAO", s_FullscreenVertexSrc, s_SSAOFragmentSrc);
    // SSR (reflexos em espaço de tela) — loop de passos FIXOS, compila em
    // GLSL 330 core em qualquer driver (o SSR antigo, de loop variável,
    // precisava de 4.x e foi removido; este é 3.3 puro e sempre disponível).
    s_SSRShader = CreateRef<Shader>("Renderer3D_SSR", s_FullscreenVertexSrc, s_SSRFragmentSrc);
    // TAA — anti-aliasing temporal (jitter + histórico com clamp de vizinhança).
    s_TAAShader = CreateRef<Shader>("Renderer3D_TAA", s_FullscreenVertexSrc, s_TAAFragmentSrc);
    // God rays — luz volumétrica em espaço de tela (marcha radial fixa).
    s_GodRaysShader = CreateRef<Shader>("Renderer3D_GodRays", s_FullscreenVertexSrc, s_GodRaysFragmentSrc);
    std::srand(2026u);
    s_SSAOKernel.resize(64);
    for (int i = 0; i < 64; ++i) {
        glm::vec3 s((float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f,
                    (float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f,
                    (float)std::rand() / (float)RAND_MAX);
        s = glm::normalize(s);
        float scale = (float)i / 64.0f;
        scale = 0.1f + 0.9f * scale * scale; // concentra as amostras perto do fragmento
        s_SSAOKernel[i] = s * scale;
    }

    std::vector<glm::vec4> noise(16);
    for (int i = 0; i < 16; ++i) {
        noise[i] = glm::vec4((float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f,
                             (float)std::rand() / (float)RAND_MAX * 2.0f - 1.0f, 0.0f, 1.0f);
    }
    glGenTextures(1, &s_NoiseTexture);
    glBindTexture(GL_TEXTURE_2D, s_NoiseTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 4, 4, 0, GL_RGBA, GL_FLOAT, noise.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    // Partículas: quad unitário (-0.5..0.5) nos attribs 0/1, buffer de instância vazio (preenchido
    // por lote em EndScene) nos attribs 2/3/4 com divisor 1 — 1 valor por instância, não por vértice.
    float particleQuad[] = {
        -0.5f, -0.5f,  0.0f, 0.0f,
         0.5f, -0.5f,  1.0f, 0.0f,
         0.5f,  0.5f,  1.0f, 1.0f,
        -0.5f,  0.5f,  0.0f, 1.0f,
    };
    uint32_t particleIndices[] = { 0, 1, 2, 2, 3, 0 };

    glGenVertexArrays(1, &s_ParticleVAO);
    glBindVertexArray(s_ParticleVAO);

    glGenBuffers(1, &s_ParticleQuadVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_ParticleQuadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(particleQuad), particleQuad, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glGenBuffers(1, &s_ParticleEBO);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ParticleEBO);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(particleIndices), particleIndices, GL_STATIC_DRAW);

    glGenBuffers(1, &s_ParticleInstanceVBO);
    glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);
    glBufferData(GL_ARRAY_BUFFER, kMaxParticlesPerBatch * sizeof(ParticleInstance), nullptr, GL_DYNAMIC_DRAW);
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, Position));
    glVertexAttribDivisor(2, 1);
    glEnableVertexAttribArray(3);
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, Size));
    glVertexAttribDivisor(3, 1);
    glEnableVertexAttribArray(4);
    glVertexAttribPointer(4, 4, GL_FLOAT, GL_FALSE, sizeof(ParticleInstance), (void*)offsetof(ParticleInstance, Color));
    glVertexAttribDivisor(4, 1);

    glBindVertexArray(0);
    s_ParticleShader = CreateRef<Shader>("Renderer3D_Particle", s_ParticleVertexSrc, s_ParticleFragmentSrc);
}

// (Re)cria o framebuffer HDR e os dois de bloom (meia resolução) se o tamanho
// OU o MSAA mudou desde a última chamada — evita realocar textura todo frame
// à toa. Com MSAA>1 a cena renderiza num FBO multisample (s_MSAAHDRFBO) e um
// blit resolve cor+profundidade pro s_HDRFBO simples, que é o que os passes
// de pós (SSAO, bright-pass, composite) amostram.
void Renderer3D::EnsurePostBuffers(uint32_t width, uint32_t height, int msaa) {
    // Memória da última recusa: uma GPU que rejeitou 8x continua rejeitando —
    // não pode recriar/refalhar o FBO todo frame (só loga uma vez e segue).
    static int s_MSAARejected = 0;
    if (s_MSAARejected > 0 && msaa >= s_MSAARejected) msaa = 1;
    msaa = (msaa > 1) ? msaa : 1;
    if (width == s_PostWidth && height == s_PostHeight && msaa == s_CurrentMSAA && s_HDRFBO != 0) return;
    s_PostWidth = width;
    s_PostHeight = height;
    s_CurrentMSAA = msaa;
    uint32_t bloomW = glm::max(1u, width / 2), bloomH = glm::max(1u, height / 2);

    if (s_HDRFBO != 0) {
        glDeleteFramebuffers(1, &s_HDRFBO);
        glDeleteTextures(1, &s_HDRColorBuffer);
        glDeleteTextures(1, &s_HDRDepthTexture);
        glDeleteFramebuffers(1, &s_MSAAHDRFBO);
        glDeleteTextures(1, &s_MSAAHDRColor);
        glDeleteRenderbuffers(1, &s_MSAAHDRDepthRBO);
        glDeleteFramebuffers(2, s_BloomFBO);
        glDeleteTextures(2, s_BloomColorBuffer);
        glDeleteFramebuffers(1, &s_SSRFBO);
        glDeleteTextures(1, &s_SSRColorBuffer);
        glDeleteFramebuffers(1, &s_TAACompositeFBO);
        glDeleteTextures(1, &s_TAACompositeTex);
        glDeleteFramebuffers(2, s_TAAHistoryFBO);
        glDeleteTextures(2, s_TAAHistoryTex);
        glDeleteFramebuffers(1, &s_GodRaysFBO);
        glDeleteTextures(1, &s_GodRaysColorBuffer);
    }

    // --- Destino simples (o que os passes de pós amostram): cor HDR + depth textura ---
    glGenTextures(1, &s_HDRColorBuffer);
    glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei)width, (GLsizei)height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenTextures(1, &s_HDRDepthTexture);
    glBindTexture(GL_TEXTURE_2D, s_HDRDepthTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, (GLsizei)width, (GLsizei)height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenFramebuffers(1, &s_HDRFBO);
    glBindFramebuffer(GL_FRAMEBUFFER, s_HDRFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_HDRColorBuffer, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_HDRDepthTexture, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        KZ_CORE_ERROR("Framebuffer HDR incompleto.");
        SetShaderDiagnostic("Framebuffer HDR INCOMPLETO");
    }

    // --- Destino multisample (se MSAA>1): a cena renderiza aqui; o blit resolve ---
    // Fallback obrigatório: se a GPU não suportar o nº de amostras no FBO HDR
    // (iGPU antiga, drivers limitados), o FBO fica INCOMPLETO e renderizar nele
    // dá viewport preto. Nesse caso cai pra sem-MSAA em vez de quebrar a tela.
    int requestedMsaa = msaa;
    if (msaa > 1) {
        GLint maxSamples = 1;
        glGetIntegerv(GL_MAX_SAMPLES, &maxSamples);
        if ((GLint)msaa > maxSamples) {
            KZ_CORE_WARN("MSAA {0} não suportado (máximo {1}); usando sem MSAA.", msaa, maxSamples);
            msaa = 1;
        }
    }
    if (msaa > 1) {
        glGenTextures(1, &s_MSAAHDRColor);
        glBindTexture(GL_TEXTURE_2D_MULTISAMPLE, s_MSAAHDRColor);
        glTexImage2DMultisample(GL_TEXTURE_2D_MULTISAMPLE, (GLsizei)msaa, GL_RGB16F,
                                (GLsizei)width, (GLsizei)height, GL_TRUE);
        glGenRenderbuffers(1, &s_MSAAHDRDepthRBO);
        glBindRenderbuffer(GL_RENDERBUFFER, s_MSAAHDRDepthRBO);
        glRenderbufferStorageMultisample(GL_RENDERBUFFER, (GLsizei)msaa, GL_DEPTH_COMPONENT24,
                                         (GLsizei)width, (GLsizei)height);
        glGenFramebuffers(1, &s_MSAAHDRFBO);
        glBindFramebuffer(GL_FRAMEBUFFER, s_MSAAHDRFBO);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D_MULTISAMPLE, s_MSAAHDRColor, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_MSAAHDRDepthRBO);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            // GPU recusou o MSAA mesmo dentro do máximo reportado: desliga sem
            // renderizar no FBO quebrado (senão o viewport fica preto).
            KZ_CORE_ERROR("Framebuffer HDR MSAA incompleto — desabilitando MSAA (fallback).");
            SetShaderDiagnostic("Framebuffer HDR MSAA INCOMPLETO — MSAA desligado");
            s_MSAARejected = requestedMsaa;
            glDeleteFramebuffers(1, &s_MSAAHDRFBO);   s_MSAAHDRFBO = 0;
            glDeleteTextures(1, &s_MSAAHDRColor);     s_MSAAHDRColor = 0;
            glDeleteRenderbuffers(1, &s_MSAAHDRDepthRBO); s_MSAAHDRDepthRBO = 0;
            msaa = 1;
        }
    }
    s_CurrentMSAA = msaa; // reflete o que de fato ficou (pode ter caído pra 1)

    glGenFramebuffers(2, s_BloomFBO);
    glGenTextures(2, s_BloomColorBuffer);
    for (int i = 0; i < 2; ++i) {
        glBindTexture(GL_TEXTURE_2D, s_BloomColorBuffer[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei)bloomW, (GLsizei)bloomH, 0, GL_RGB, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_BloomColorBuffer[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            KZ_CORE_ERROR("Framebuffer de bloom {0} incompleto.", i);
            SetShaderDiagnostic("Framebuffer de bloom incompleto");
        }
    }

    // --- SSR (reflexos em espaço de tela): cor HDR + depth -> RGBA16F ---
    // Textura só cor (a profundidade vem do s_HDRDepthTexture, já resolvido).
    glGenFramebuffers(1, &s_SSRFBO);
    glGenTextures(1, &s_SSRColorBuffer);
    glBindTexture(GL_TEXTURE_2D, s_SSRColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, s_SSRFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_SSRColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        KZ_CORE_ERROR("Framebuffer SSR incompleto.");
        SetShaderDiagnostic("Framebuffer SSR incompleto");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // --- TAA (anti-aliasing temporal): alvo intermediário do composite + 2
    // históricos ping-pong (RGBA8, espaço LDR pós-tonemap) ---
    auto makeTex = [](uint32_t& tex, uint32_t w, uint32_t h) {
        glGenTextures(1, &tex);
        glBindTexture(GL_TEXTURE_2D, tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, (GLsizei)w, (GLsizei)h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    };
    glGenFramebuffers(1, &s_TAACompositeFBO);
    makeTex(s_TAACompositeTex, width, height);
    glBindFramebuffer(GL_FRAMEBUFFER, s_TAACompositeFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_TAACompositeTex, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        SetShaderDiagnostic("Framebuffer TAA composite INCOMPLETO");
    for (int i = 0; i < 2; ++i) {
        glGenFramebuffers(1, &s_TAAHistoryFBO[i]);
        makeTex(s_TAAHistoryTex[i], width, height);
        glBindFramebuffer(GL_FRAMEBUFFER, s_TAAHistoryFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_TAAHistoryTex[i], 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            SetShaderDiagnostic("Framebuffer TAA histórico INCOMPLETO");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    s_TAAHistoryValid = false; // resolução/alvo mudou: histórico antigo é inválido
    s_TAACounter = 0;

    // --- God rays: meia resolução, RGBA16F (cor brilhante acumulada) ---
    uint32_t grW = glm::max(1u, width / 2), grH = glm::max(1u, height / 2);
    glGenFramebuffers(1, &s_GodRaysFBO);
    glGenTextures(1, &s_GodRaysColorBuffer);
    glBindTexture(GL_TEXTURE_2D, s_GodRaysColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, (GLsizei)grW, (GLsizei)grH, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindFramebuffer(GL_FRAMEBUFFER, s_GodRaysFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_GodRaysColorBuffer, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        SetShaderDiagnostic("Framebuffer God rays INCOMPLETO");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// (Re)cria o FBO da reflexão planar (espelho real): cor RGBA16F + depth.
// Resolução interna do frame (qualidade de espelho). Só é criado se há
// material com PlanarReflect no frame (checado no EndScene).
void Renderer3D::EnsurePlanarBuffers(uint32_t width, uint32_t height) {
    if (s_PlanarFBO != 0 && width == s_PlanarWidth && height == s_PlanarHeight) return;
    s_PlanarWidth = width;
    s_PlanarHeight = height;

    if (s_PlanarFBO != 0) {
        glDeleteFramebuffers(1, &s_PlanarFBO);
        glDeleteTextures(1, &s_PlanarColor);
        glDeleteTextures(1, &s_PlanarDepth);
    }
    glGenFramebuffers(1, &s_PlanarFBO);
    glGenTextures(1, &s_PlanarColor);
    glBindTexture(GL_TEXTURE_2D, s_PlanarColor);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, (GLsizei)width, (GLsizei)height, 0, GL_RGBA, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glGenTextures(1, &s_PlanarDepth);
    glBindTexture(GL_TEXTURE_2D, s_PlanarDepth);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, (GLsizei)width, (GLsizei)height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindFramebuffer(GL_FRAMEBUFFER, s_PlanarFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_PlanarColor, 0);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_PlanarDepth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        SetShaderDiagnostic("Framebuffer reflexão planar INCOMPLETO");
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Reflete a cena vista pela câmera na face do espelho (pré-passe). Desenha o
// skybox + todas as meshes (exceto o próprio espelho) com a câmera refletida,
// numa projeção com near plane oblíquo na face do espelho (sem isso a geometria
// abaixo do espelho "vaza" pro reflexo).
void Renderer3D::RenderPlanarReflection(const glm::vec3& planePoint, const glm::vec3& planeNormal,
                                        int mirrorIndex, uint32_t width, uint32_t height,
                                        const glm::mat4& reflectView, const glm::mat4& reflectProj,
                                        const glm::mat4& reflectVP, const glm::vec3& reflectCam) {
    EnsurePlanarBuffers(width, height);
    s_ReflectionViewProjection = reflectVP;
    s_HasPlanarReflection = true;

    glBindFramebuffer(GL_FRAMEBUFFER, s_PlanarFBO);
    glViewport(0, 0, (GLsizei)width, (GLsizei)height);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // Skybox (LEQUAL, sem escrita de profundidade).
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    s_SkyboxShader->Bind();
    s_SkyboxShader->SetMat4("u_View", glm::mat4(glm::mat3(reflectView)));
    s_SkyboxShader->SetMat4("u_Projection", reflectProj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_EnvironmentCubemap);
    s_SkyboxShader->SetInt("u_EnvironmentMap", 0);
    glm::vec3 sunDir = s_HasShadowCaster
        ? glm::normalize(-s_ShadowCaster.Direction)
        : glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
    s_SkyboxShader->SetFloat3("u_SunDirection", sunDir);
    s_SkyboxShader->SetInt("u_AtmosphereSky", s_EnvironmentHDRIPath.empty() ? 1 : 0);
    RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // Meshes (sem o espelho, que não pode refletir a si mesmo).
    if (!s_DrawList.empty()) {
        s_MeshShader->Bind();
        s_MeshShader->SetMat4("u_ViewProjection", reflectVP);
        s_MeshShader->SetMat4("u_View", reflectView);
        s_MeshShader->SetFloat3("u_ViewPos", reflectCam);
        s_MeshShader->SetInt("u_HasShadow", s_HasShadowCaster ? 1 : 0);
        s_MeshShader->SetInt("u_ShadowPCF", s_Settings.ShadowPCFRadius);
        s_MeshShader->SetFloat("u_ShadowSoftness", s_Settings.ShadowSoftness);
        if (s_HasShadowCaster) {
            for (int c = 0; c < kCascadeCount; ++c) {
                std::string idx = "[" + std::to_string(c) + "]";
                s_MeshShader->SetMat4("u_LightSpaceMatrix" + idx, s_LightSpaceMatrix[c]);
                s_MeshShader->SetFloat("u_CascadeSplits" + idx, s_CascadeSplits[c]);
            }
        }
        for (int c = 0; c < kCascadeCount; ++c) {
            glActiveTexture(GL_TEXTURE1 + c);
            glBindTexture(GL_TEXTURE_2D, s_ShadowMap[c]);
            s_MeshShader->SetInt("u_ShadowMap" + std::to_string(c), 1 + c);
        }
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_IrradianceCubemap);
        s_MeshShader->SetInt("u_IrradianceMap", 5);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_PrefilterCubemap);
        s_MeshShader->SetInt("u_PrefilterMap", 6);
        s_MeshShader->SetFloat("u_MaxPrefilterLod", (float)(kPrefilterMipLevels - 1));

        s_MeshShader->SetInt("u_FogEnabled", s_Settings.FogEnabled ? 1 : 0);
        s_MeshShader->SetFloat3("u_FogColor",
            glm::vec3(s_Settings.FogColor[0], s_Settings.FogColor[1], s_Settings.FogColor[2]));
        s_MeshShader->SetFloat("u_FogDensity", s_Settings.FogDensity);

        s_MeshShader->SetInt("u_LightCount", (int)s_LightList.size());
        for (size_t i = 0; i < s_LightList.size(); ++i) {
            const Light& l = s_LightList[i];
            std::string idx = "[" + std::to_string(i) + "]";
            s_MeshShader->SetInt("u_LightTypes" + idx, (int)l.Type);
            s_MeshShader->SetFloat3("u_LightPositions" + idx, l.Position);
            s_MeshShader->SetFloat3("u_LightDirections" + idx, l.Direction);
            s_MeshShader->SetFloat3("u_LightColors" + idx, l.Color);
            s_MeshShader->SetFloat("u_LightIntensities" + idx, l.Intensity);
            s_MeshShader->SetFloat("u_LightRanges" + idx, l.Range);
            s_MeshShader->SetFloat("u_LightInnerCos" + idx, glm::cos(glm::radians(l.InnerConeDeg)));
            s_MeshShader->SetFloat("u_LightOuterCos" + idx, glm::cos(glm::radians(l.OuterConeDeg)));
        }

        // No reflexo, nenhum material é "espelho" (evita recursão infinita).
        s_MeshShader->SetInt("u_IsPlanarMirror", 0);
        for (int i = 0; i < (int)s_DrawList.size(); ++i) {
            if (i == mirrorIndex) continue;
            auto& cmd = s_DrawList[i];
            s_MeshShader->SetMat4("u_Transform", cmd.Transform);
            s_MeshShader->SetMat3("u_NormalMatrix", glm::mat3(glm::transpose(glm::inverse(cmd.Transform))));
            s_MeshShader->SetFloat3("u_Albedo", cmd.Mat.Albedo);
            s_MeshShader->SetFloat("u_Metallic", cmd.Mat.Metallic);
            s_MeshShader->SetFloat("u_Roughness", cmd.Mat.Roughness);
            s_MeshShader->SetFloat("u_AO", cmd.Mat.AO);

            if (cmd.Joints.empty()) {
                s_MeshShader->SetInt("u_Animated", 0);
            } else {
                s_MeshShader->SetInt("u_Animated", 1);
                for (uint32_t j = 0; j < kMaxSkinJoints; ++j) {
                    glm::mat4 m = (j < cmd.Joints.size()) ? cmd.Joints[j] : glm::mat4(1.0f);
                    s_MeshShader->SetMat4("u_JointMatrices[" + std::to_string(j) + "]", m);
                }
            }

            bool hasAlbedo = (bool)cmd.Mat.AlbedoMap;
            s_MeshShader->SetInt("u_HasAlbedoMap", hasAlbedo ? 1 : 0);
            if (hasAlbedo) { cmd.Mat.AlbedoMap->Bind(0); s_MeshShader->SetInt("u_AlbedoMap", 0); }
            bool hasNormal = (bool)cmd.Mat.NormalMap;
            s_MeshShader->SetInt("u_HasNormalMap", hasNormal ? 1 : 0);
            if (hasNormal) { cmd.Mat.NormalMap->Bind(4); s_MeshShader->SetInt("u_NormalMap", 4); }
            bool hasMR = (bool)cmd.Mat.MetallicRoughnessMap;
            s_MeshShader->SetInt("u_HasMetallicRoughnessMap", hasMR ? 1 : 0);
            if (hasMR) { cmd.Mat.MetallicRoughnessMap->Bind(7); s_MeshShader->SetInt("u_MetallicRoughnessMap", 7); }
            bool hasEmissive = (bool)cmd.Mat.EmissiveMap;
            s_MeshShader->SetInt("u_HasEmissiveMap", hasEmissive ? 1 : 0);
            if (hasEmissive) { cmd.Mat.EmissiveMap->Bind(8); s_MeshShader->SetInt("u_EmissiveMap", 8); }
            bool hasHeight = (bool)cmd.Mat.HeightMap;
            s_MeshShader->SetInt("u_HasHeightMap", hasHeight ? 1 : 0);
            s_MeshShader->SetFloat("u_HeightScale", cmd.Mat.HeightScale);
            if (hasHeight) { cmd.Mat.HeightMap->Bind(9); s_MeshShader->SetInt("u_HeightMap", 9); }
            s_MeshShader->SetFloat3("u_Emissive", cmd.Mat.Emissive);
            s_MeshShader->SetFloat("u_EmissiveStrength", cmd.Mat.EmissiveStrength);

            RenderCommand::DrawIndexed(cmd.MeshAsset->GetVertexArray(), cmd.MeshAsset->GetIndexCount());
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// (Re)cria os 3 shadow maps se a resolução mudou (preset de qualidade pode
// trocar entre 512..4096 em runtime sem reiniciar). Tamanho estático entre
// frames (checagem barata no início do EndScene).
void Renderer3D::EnsureShadowMaps(uint32_t size) {
    size = glm::max(512u, size);
    static uint32_t s_CurrentShadowSize = 0;
    if (size == s_CurrentShadowSize && s_ShadowFBO[0] != 0) return;
    s_CurrentShadowSize = size;

    if (s_ShadowFBO[0] != 0) {
        for (int i = 0; i < kCascadeCount; ++i) {
            glDeleteFramebuffers(1, &s_ShadowFBO[i]);
            glDeleteTextures(1, &s_ShadowMap[i]);
            s_ShadowFBO[i] = 0;
            s_ShadowMap[i] = 0;
        }
    }

    float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // fora do shadow map = nunca em sombra
    for (int i = 0; i < kCascadeCount; ++i) {
        glGenFramebuffers(1, &s_ShadowFBO[i]);
        glGenTextures(1, &s_ShadowMap[i]);
        glBindTexture(GL_TEXTURE_2D, s_ShadowMap[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, (GLsizei)size, (GLsizei)size,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        // GL_LINEAR na profundidade = PCF bilinear de hardware (borda de sombra
        // mais suave que o sample único NEAREST; o loop manual do shader soma
        // por cima). Filtragem de textura de profundidade é GL 3.0+.
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);

        glBindFramebuffer(GL_FRAMEBUFFER, s_ShadowFBO[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, s_ShadowMap[i], 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            KZ_CORE_ERROR("Framebuffer de sombra da cascata {0} incompleto.", i);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// (Re)cria os dois FBOs de SSAO (oclusão bruta + borrada), meia resolução,
// se o tamanho mudou desde a última vez.
void Renderer3D::EnsureSSAOBuffers(uint32_t width, uint32_t height) {
    uint32_t w = glm::max(1u, width / 2), h = glm::max(1u, height / 2);
    if (s_SSAOColorBuffer != 0 && w == s_SSAOWidth && h == s_SSAOHeight) return;
    s_SSAOWidth = w;
    s_SSAOHeight = h;

    if (s_SSAOColorBuffer != 0) {
        glDeleteFramebuffers(1, &s_SSAOFBO);
        glDeleteTextures(1, &s_SSAOColorBuffer);
        glDeleteFramebuffers(1, &s_SSAOBlurFBO);
        glDeleteTextures(1, &s_SSAOBlurBuffer);
    }

    for (int pass = 0; pass < 2; ++pass) {
        uint32_t* fbo = (pass == 0) ? &s_SSAOFBO : &s_SSAOBlurFBO;
        uint32_t* tex = (pass == 0) ? &s_SSAOColorBuffer : &s_SSAOBlurBuffer;
        glGenFramebuffers(1, fbo);
        glGenTextures(1, tex);
        glBindTexture(GL_TEXTURE_2D, *tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, (GLsizei)w, (GLsizei)h, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
            KZ_CORE_ERROR("Framebuffer SSAO {0} incompleto.", pass);
            SetShaderDiagnostic("Framebuffer SSAO incompleto");
        }
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}


// único, síncrono. A fonte do ambiente é o céu procedural OU uma imagem HDR
// equirectangular (Renderer3D::SetEnvironmentHDRIPath) — pra HDRI a imagem é
// carregada, convertida pra cubemap e a partir daí o restante do pipeline
// (irradiância + pré-filtro GGX) é idêntico. O bake roda uma vez no Init() e
// de novo sob demanda quando o usuário troca a fonte (mudar a direção do sol
// em runtime continua sendo a limitação registrada: recalcular convolução por
// frame é caro demais).
void Renderer3D::GenerateEnvironment() {
    KZ_TRACE_SCOPE("Renderer3D::GenerateEnvironment");

    // Rebake: libera os cubemaps anteriores antes de criar os novos.
    if (s_EnvironmentCubemap) glDeleteTextures(1, &s_EnvironmentCubemap);
    if (s_IrradianceCubemap) glDeleteTextures(1, &s_IrradianceCubemap);
    if (s_PrefilterCubemap) glDeleteTextures(1, &s_PrefilterCubemap);
    s_EnvironmentCubemap = s_IrradianceCubemap = s_PrefilterCubemap = 0;

    if (!s_CaptureCube) s_CaptureCube = Mesh::CreateCube();
    if (!s_SkyboxShader) s_SkyboxShader = CreateRef<Shader>("Renderer3D_Skybox", s_SkyboxVertexSrc, s_SkyboxFragmentSrc);
    auto captureShaderSky        = CreateRef<Shader>("Renderer3D_CaptureSky", s_CaptureVertexSrc, s_SkyFragmentSrc);
    auto captureShaderIrradiance = CreateRef<Shader>("Renderer3D_CaptureIrradiance", s_CaptureVertexSrc, s_IrradianceFragmentSrc);
    auto captureShaderPrefilter  = CreateRef<Shader>("Renderer3D_CapturePrefilter", s_CaptureVertexSrc, s_PrefilterFragmentSrc);

    // As 6 matrizes de vista, uma por face do cubemap, todas olhando da
    // origem (o "olho" da captura nunca sai do centro — só a orientação
    // muda) — ordem e "up" seguem a convenção do OpenGL pra
    // GL_TEXTURE_CUBE_MAP_POSITIVE_X..NEGATIVE_Z.
    glm::mat4 captureProjection = glm::perspective(glm::radians(90.0f), 1.0f, 0.1f, 10.0f);
    glm::mat4 captureViews[6] = {
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3(-1.0f,  0.0f,  0.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  1.0f,  0.0f), glm::vec3(0.0f,  0.0f,  1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f, -1.0f,  0.0f), glm::vec3(0.0f,  0.0f, -1.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f,  1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
        glm::lookAt(glm::vec3(0.0f), glm::vec3( 0.0f,  0.0f, -1.0f), glm::vec3(0.0f, -1.0f,  0.0f)),
    };

    GLint prevFBO = 0;
    GLint prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);

    uint32_t captureFBO, captureRBO;
    glGenFramebuffers(1, &captureFBO);
    glGenRenderbuffers(1, &captureRBO);
    glBindFramebuffer(GL_FRAMEBUFFER, captureFBO);
    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);

    // Sol padrão pro bake (caminho procedural): mesma direção default de
    // DirectionalLight (ver Renderer3D.hpp) — se a cena não tiver luz nenhuma
    // ainda quando o ambiente é gerado, o céu já nasce coerente com o sol default.
    DirectionalLight defaultSun;
    glm::vec3 sunDirToLight = glm::normalize(-defaultSun.Direction);

    // HDRI: se há caminho configurado E a imagem carrega, o céu vem dela;
    // caso contrário cai pro procedural (com erro logado, sem quebrar o bake).
    bool useHDRI = (!s_EnvironmentHDRIPath.empty() && LoadHDRI(s_EnvironmentHDRIPath));

    // --- 1. Cubemap de ambiente (céu procedural ou HDRI) ----------------
    glGenTextures(1, &s_EnvironmentCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_EnvironmentCubemap);
    for (uint32_t i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     (GLsizei)kEnvironmentSize, (GLsizei)kEnvironmentSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)kEnvironmentSize, (GLsizei)kEnvironmentSize);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, captureRBO);
    glViewport(0, 0, (GLsizei)kEnvironmentSize, (GLsizei)kEnvironmentSize);

    // Sem cull de face aqui: é mais simples/seguro desenhar os dois lados
    // do cubo de captura do que garantir a winding certa vista de dentro.
    glDisable(GL_CULL_FACE);
    if (useHDRI) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_EquirectTexture);
        s_EquirectShader->Bind();
        s_EquirectShader->SetInt("u_Equirect", 0);
        for (uint32_t i = 0; i < 6; ++i) {
            s_EquirectShader->SetMat4("u_ViewProjection", captureProjection * captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, s_EnvironmentCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
        }
    } else {
        captureShaderSky->Bind();
        captureShaderSky->SetFloat3("u_SunDir", sunDirToLight);
        captureShaderSky->SetFloat3("u_SunColor", defaultSun.Color * defaultSun.Intensity);
        for (uint32_t i = 0; i < 6; ++i) {
            captureShaderSky->SetMat4("u_ViewProjection", captureProjection * captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, s_EnvironmentCubemap, 0);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
        }
    }

    // --- 2. Cubemap de irradiância (difusa) -----------------------------
    glGenTextures(1, &s_IrradianceCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_IrradianceCubemap);
    for (uint32_t i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     (GLsizei)kIrradianceSize, (GLsizei)kIrradianceSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)kIrradianceSize, (GLsizei)kIrradianceSize);
    glViewport(0, 0, (GLsizei)kIrradianceSize, (GLsizei)kIrradianceSize);

    captureShaderIrradiance->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_EnvironmentCubemap);
    captureShaderIrradiance->SetInt("u_EnvironmentMap", 0);
    for (uint32_t i = 0; i < 6; ++i) {
        captureShaderIrradiance->SetMat4("u_ViewProjection", captureProjection * captureViews[i]);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, s_IrradianceCubemap, 0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
    }

    // --- 3. Cubemap especular pré-filtrado, um mip por faixa de rugosidade ---
    glGenTextures(1, &s_PrefilterCubemap);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_PrefilterCubemap);
    for (uint32_t i = 0; i < 6; ++i) {
        glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, 0, GL_RGB16F,
                     (GLsizei)kPrefilterBaseSize, (GLsizei)kPrefilterBaseSize, 0, GL_RGB, GL_FLOAT, nullptr);
    }
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
    glGenerateMipmap(GL_TEXTURE_CUBE_MAP); // aloca a cadeia de mip; cada nível é preenchido abaixo

    captureShaderPrefilter->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_EnvironmentCubemap);
    captureShaderPrefilter->SetInt("u_EnvironmentMap", 0);
    for (uint32_t mip = 0; mip < kPrefilterMipLevels; ++mip) {
        uint32_t mipSize = kPrefilterBaseSize >> mip; // 128, 64, 32, 16, 8
        glBindRenderbuffer(GL_RENDERBUFFER, captureRBO);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)mipSize, (GLsizei)mipSize);
        glViewport(0, 0, (GLsizei)mipSize, (GLsizei)mipSize);

        float roughness = (float)mip / (float)(kPrefilterMipLevels - 1);
        captureShaderPrefilter->SetFloat("u_Roughness", roughness);
        for (uint32_t i = 0; i < 6; ++i) {
            captureShaderPrefilter->SetMat4("u_ViewProjection", captureProjection * captureViews[i]);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0,
                                    GL_TEXTURE_CUBE_MAP_POSITIVE_X + i, s_PrefilterCubemap, (GLint)mip);
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
            RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
        }
    }

    // Restaura GL_CULL_FACE pro estado real de antes desse bake: OFF. O
    // resto do renderer (mesh normal, shadow pass) nunca chama
    // glEnable(GL_CULL_FACE) em lugar nenhum — só glCullFace() pra trocar
    // o MODO, que não tem efeito com o teste desligado. Deixar ligado
    // aqui mudaria silenciosamente o comportamento de toda mesh desenhada
    // depois disso (culling de back-face passaria a valer pra cena
    // inteira, não só pra esse bake) — por isso desliga de novo, não liga.
    glDisable(GL_CULL_FACE);
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevViewport[0], prevViewport[1], (GLsizei)prevViewport[2], (GLsizei)prevViewport[3]);
    glDeleteFramebuffers(1, &captureFBO);
    glDeleteRenderbuffers(1, &captureRBO);

    KZ_CORE_INFO("Renderer3D::GenerateEnvironment: bake concluído (ambiente {0}x{1}, irradiância {2}x{3}, pré-filtro {4}x{4} com {5} mips{6})",
                 kEnvironmentSize, kEnvironmentSize, kIrradianceSize, kIrradianceSize, kPrefilterBaseSize, kPrefilterMipLevels,
                 useHDRI ? " — fonte HDRI" : " — céu procedural");
}

// Carrega uma imagem HDR equirectangular (ex.: .hdr Radiance) numa textura
// 2D float; a conversão pra cubemap acontece no bake (GenerateEnvironment).
bool Renderer3D::LoadHDRI(const std::string& path) {
    int w = 0, h = 0, comp = 0;
    float* data = nullptr;

    if (IsEmbeddedPath(path)) {
        EmbeddedBuffer buf;
        if (!GetEmbeddedResource(EmbeddedNameFromPath(path), buf)) {
            KZ_CORE_ERROR("Renderer3D::LoadHDRI: recurso embutido não encontrado '{0}'.", path);
            return false;
        }
        data = stbi_loadf_from_memory((const stbi_uc*)buf.Data, (int)buf.Size, &w, &h, &comp, 4);
    } else {
        data = stbi_loadf(path.c_str(), &w, &h, &comp, 4);
    }

    if (!data) {
        KZ_CORE_ERROR("Renderer3D::LoadHDRI: falha ao carregar '{0}' (não é imagem HDR suportada?).", path);
        return false;
    }
    if (s_EquirectTexture == 0) glGenTextures(1, &s_EquirectTexture);
    glBindTexture(GL_TEXTURE_2D, s_EquirectTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, w, h, 0, GL_RGBA, GL_FLOAT, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    stbi_image_free(data);
    KZ_CORE_INFO("Renderer3D::LoadHDRI: '{0}' ({1}x{2}) carregado.", path, w, h);
    return true;
}

const GraphicsSettings& Renderer3D::GetGraphicsSettings() { return s_Settings; }

void Renderer3D::SetGraphicsSettings(const GraphicsSettings& settings) {
    s_Settings = settings;
    s_Settings.Clamp();
}

void Renderer3D::SetEnvironmentHDRIPath(const std::string& path) {
    if (path == s_EnvironmentHDRIPath) return;
    s_EnvironmentHDRIPath = path;
    if (s_EnvironmentCubemap != 0) GenerateEnvironment(); // rebakeia com a nova fonte
}

const std::string& Renderer3D::GetEnvironmentHDRIPath() { return s_EnvironmentHDRIPath; }

// Não libera shadow FBO/map nem os cubemaps de IBL — mesma lacuna que já
// existia aqui antes desta v1 de PBR (só documentando, não é regressão
// nova). Inofensivo hoje porque Shutdown() só roda no fim do processo
// (driver recupera tudo), mas vira problema de verdade no dia em que a
// engine ganhar "trocar de cena/projeto sem reiniciar o processo" —
// registrar aqui pra não se perder.
void Renderer3D::Shutdown() { KZ_TRACE_SCOPE("Renderer3D::Shutdown"); }

// Divide o frustum da câmera em kCascadeCount faixas de distância (mistura log/uniforme,
// lambda=0.5) e ajusta uma matriz luz ortográfica "apertada" em cada uma — sombra nítida
// perto, mais grosseira longe, e a área sempre acompanha a câmera (não fica mais fixa na origem).
void Renderer3D::ComputeCascades(const glm::vec3& lightDir) {
    constexpr float kLambda = 0.5f;
    float near = s_CamNear, far = s_CamFar;
    float splits[kCascadeCount];
    for (int i = 0; i < kCascadeCount; ++i) {
        float p = (float)(i + 1) / (float)kCascadeCount;
        float logSplit = near * glm::pow(far / near, p);
        float uniSplit = near + (far - near) * p;
        splits[i] = kLambda * logSplit + (1.0f - kLambda) * uniSplit;
    }

    glm::vec3 dir = glm::normalize(lightDir);
    glm::vec3 up = (glm::abs(dir.y) > 0.99f) ? glm::vec3(0.0f, 0.0f, 1.0f) : glm::vec3(0.0f, 1.0f, 0.0f);

    float prevSplit = near;
    for (int i = 0; i < kCascadeCount; ++i) {
        // 8 cantos do sub-frustum [prevSplit, splits[i]] em NDC -> mundo, via a
        // câmera de verdade (reaproveita a projeção da cena, só troca near/far).
        glm::mat4 subProj = glm::perspective(glm::radians(s_CamFOV), s_CamAspect, prevSplit, splits[i]);
        glm::mat4 subInv = glm::inverse(subProj * s_View);

        glm::vec3 corners[8];
        int c = 0;
        for (int x = -1; x <= 1; x += 2)
            for (int y = -1; y <= 1; y += 2)
                for (int z = -1; z <= 1; z += 2) {
                    glm::vec4 p = subInv * glm::vec4((float)x, (float)y, (float)z, 1.0f);
                    corners[c++] = glm::vec3(p) / p.w;
                }

        glm::vec3 center(0.0f);
        for (auto& corn : corners) center += corn;
        center /= 8.0f;

        // Raio da esfera que envolve o sub-frustum: usar esfera (não AABB apertado) evita
        // "tremida" da sombra quando a câmera gira, ao custo de desperdiçar um pouco de resolução.
        float radius = 0.0f;
        for (auto& corn : corners) radius = glm::max(radius, glm::length(corn - center));
        radius = glm::ceil(radius * 16.0f) / 16.0f;

        glm::vec3 eye = center - dir * radius * 2.0f;
        glm::mat4 lightView = glm::lookAt(eye, center, up);
        glm::mat4 lightProj = glm::ortho(-radius, radius, -radius, radius, 0.01f, radius * 4.0f);
        s_LightSpaceMatrix[i] = lightProj * lightView;

        // Distância guardada em view-space (Z da câmera, positivo pra frente) — é o que o
        // fragment shader compara pra escolher a cascata certa por pixel.
        s_CascadeSplits[i] = splits[i];
        prevSplit = splits[i];
    }
}

void Renderer3D::BeginScene(const PerspectiveCamera& camera) {
    s_ViewProjection = camera.GetViewProjectionMatrix();
    s_View = camera.GetViewMatrix();
    s_Projection = camera.GetProjectionMatrix();
    s_CameraPos = camera.GetPosition();
    s_CamFOV = camera.GetFOV();
    s_CamAspect = camera.GetAspect();
    s_CamNear = camera.GetNearClip();
    s_CamFar = camera.GetFarClip();
    s_DrawList.clear();
    s_LightList.clear();
    s_HasShadowCaster = false;
    s_DrawGridFlag = false;
    s_ParticleBatches.clear();
}

void Renderer3D::SubmitParticles(const std::vector<ParticleInstance>& instances, bool additive,
                                 const Ref<Texture2D>& texture) {
    if (instances.empty()) return;
    if (instances.size() <= kMaxParticlesPerBatch) {
        s_ParticleBatches.push_back({ instances, additive, texture });
    } else {
        s_ParticleBatches.push_back({ std::vector<ParticleInstance>(instances.begin(), instances.begin() + kMaxParticlesPerBatch), additive, texture });
    }
}

void Renderer3D::SubmitLight(const Light& light) {
    if (s_LightList.size() >= kMaxLights) return;
    if (light.Type == LightType::Directional && !s_HasShadowCaster) {
        s_ShadowCaster = light;
        s_HasShadowCaster = true;
        s_LightList.insert(s_LightList.begin(), light); // shadow caster sempre no índice 0
        return;
    }
    s_LightList.push_back(light);
}

void Renderer3D::EndScene() {
    GLint prevFBO = 0;
    GLint prevViewport[4];
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &prevFBO);
    glGetIntegerv(GL_VIEWPORT, prevViewport);
    uint32_t targetW = (uint32_t)prevViewport[2], targetH = (uint32_t)prevViewport[3];
    if (targetW == 0 || targetH == 0) { s_DrawList.clear(); return; } // painel/janela minimizado

    // Resolução interna respeita o render scale do preset — o passe de
    // composição upscale/downscale pro destino do chamador automaticamente.
    uint32_t internalW = glm::max(1u, (uint32_t)glm::round((float)targetW * s_Settings.RenderScale));
    uint32_t internalH = glm::max(1u, (uint32_t)glm::round((float)targetH * s_Settings.RenderScale));
    EnsurePostBuffers(internalW, internalH, s_Settings.MSAA);
    EnsureShadowMaps((uint32_t)s_Settings.ShadowMapSize);

    // Jitter da câmera (TAA): desloca a projeção por ±1 pixel em cada eixo,
    // em sequência Halton de baixa discrepância, pra cada frame amostrar uma
    // subposição de pixel diferente — o misturador temporal junta tudo.
    // Aplicado ANTES de renderizar a cena, pra cor+depth+AO/SSR ficarem
    // consistentes com a mesma projeção jitterada.
    bool taaEnabled = s_Settings.TAAEnabled;
    if (taaEnabled) {
        ++s_TAACounter;
        auto halton = [](uint32_t index, uint32_t base) {
            float result = 0.0f, f = 1.0f / (float)base;
            uint32_t i = index;
            while (i > 0) { result += f * (float)(i % base); i /= base; f /= (float)base; }
            return result;
        };
        float jx = (halton(s_TAACounter, 2u) - 0.5f) * 2.0f / (float)internalW;
        float jy = (halton(s_TAACounter, 3u) - 0.5f) * 2.0f / (float)internalH;
        s_Projection[2][0] += jx;
        s_Projection[2][1] += jy;
        s_ViewProjection = s_Projection * s_View;
    }

    // --- Pré-passe 0.5: reflexão planar (espelho real). Se alguma entidade do
    // frame é um espelho, renderiza a cena refletida numa câmera espelhada e o
    // material espelhado amostra essa textura no passe principal. ---
    s_HasPlanarReflection = false;
    int mirrorIndex = -1;
    glm::vec3 mirrorPoint(0.0f), mirrorNormal(0.0f, 1.0f, 0.0f);
    for (size_t i = 0; i < s_DrawList.size(); ++i) {
        if (s_DrawList[i].Mat.PlanarReflect) { mirrorIndex = (int)i; break; }
    }
    if (mirrorIndex >= 0) {
        const auto& mirror = s_DrawList[mirrorIndex];
        mirrorPoint = glm::vec3(mirror.Transform[3]);
        mirrorNormal = glm::normalize(glm::mat3(mirror.Transform) * glm::vec3(0.0f, 1.0f, 0.0f));
        float side = glm::dot(mirrorNormal, s_CameraPos - mirrorPoint);
        if (side > 0.001f) {
            // Câmera refletida (posição + view) e projeção com near oblíquo.
            glm::vec3 reflectCam = glm::reflect(s_CameraPos - mirrorPoint, mirrorNormal) + mirrorPoint;
            glm::mat4 R(1.0f);
            R[0] = { 1.0f - 2.0f*mirrorNormal.x*mirrorNormal.x, -2.0f*mirrorNormal.x*mirrorNormal.y, -2.0f*mirrorNormal.x*mirrorNormal.z, 0.0f };
            R[1] = { -2.0f*mirrorNormal.y*mirrorNormal.x, 1.0f - 2.0f*mirrorNormal.y*mirrorNormal.y, -2.0f*mirrorNormal.y*mirrorNormal.z, 0.0f };
            R[2] = { -2.0f*mirrorNormal.z*mirrorNormal.x, -2.0f*mirrorNormal.z*mirrorNormal.y, 1.0f - 2.0f*mirrorNormal.z*mirrorNormal.z, 0.0f };
            float d = glm::dot(mirrorNormal, mirrorPoint);
            R[3] = { -2.0f*mirrorNormal.x*d, -2.0f*mirrorNormal.y*d, -2.0f*mirrorNormal.z*d, 1.0f };
            glm::mat4 reflectView = s_View * R;

            // Near plane oblíquo na face do espelho (Lengyel): a geometria do
            // outro lado do espelho é cortada em vez de "vazar" pro reflexo.
            glm::vec4 planeWorld(mirrorNormal, -d);
            glm::vec4 cp = glm::transpose(glm::inverse(s_View)) * planeWorld;
            glm::mat4 reflectProj = s_Projection;
            glm::vec4 q(
                (glm::sign(cp.x) + reflectProj[2][0]) / reflectProj[0][0],
                (glm::sign(cp.y) + reflectProj[2][1]) / reflectProj[1][1],
                -1.0f,
                (1.0f + reflectProj[2][2]) / reflectProj[3][2]);
            glm::vec4 cvec = cp * (2.0f / glm::dot(cp, q));
            reflectProj[2] = cvec - reflectProj[3];
            glm::mat4 reflectVP = reflectProj * reflectView;

            if (s_HasShadowCaster) ComputeCascades(s_ShadowCaster.Direction);
            RenderPlanarReflection(mirrorPoint, mirrorNormal, mirrorIndex,
                                   internalW, internalH, reflectView, reflectProj, reflectVP, reflectCam);
        }
    }

    // --- Passe 0: profundidade vista da luz, uma vez por cascata (só se há shadow caster) ---
    if (!s_DrawList.empty() && s_HasShadowCaster) {
        ComputeCascades(s_ShadowCaster.Direction);
        glCullFace(GL_FRONT); // reduz acne de sombra sem precisar de bias grande
        s_ShadowShader->Bind();
        uint32_t shadowSize = (uint32_t)s_Settings.ShadowMapSize;
        for (int c = 0; c < kCascadeCount; ++c) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_ShadowFBO[c]);
            glViewport(0, 0, (GLsizei)shadowSize, (GLsizei)shadowSize);
            glClear(GL_DEPTH_BUFFER_BIT);
            s_ShadowShader->SetMat4("u_LightSpaceMatrix", s_LightSpaceMatrix[c]);
            for (auto& cmd : s_DrawList) {
                s_ShadowShader->SetMat4("u_Model", cmd.Transform);
                // Malha esquelética: sobe as juntas e liga o skinning pra a
                // sombra seguir a pose animada (não a de repouso).
                if (cmd.Joints.empty()) {
                    s_ShadowShader->SetInt("u_Animated", 0);
                } else {
                    s_ShadowShader->SetInt("u_Animated", 1);
                    for (uint32_t j = 0; j < kMaxSkinJoints; ++j) {
                        glm::mat4 m = (j < cmd.Joints.size()) ? cmd.Joints[j] : glm::mat4(1.0f);
                        s_ShadowShader->SetMat4("u_JointMatrices[" + std::to_string(j) + "]", m);
                    }
                }
                RenderCommand::DrawIndexed(cmd.MeshAsset->GetVertexArray(), cmd.MeshAsset->GetIndexCount());
            }
        }
        glCullFace(GL_BACK);
    }

    // --- Passe 1: cena inteira (mesh + skybox), pro framebuffer HDR interno ---
    uint32_t sceneFBO = (s_CurrentMSAA > 1) ? s_MSAAHDRFBO : s_HDRFBO;
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFBO);
    glViewport(0, 0, (GLsizei)internalW, (GLsizei)internalH);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    if (s_DrawGridFlag) {
        s_LineShader->Bind();
        s_LineShader->SetMat4("u_ViewProjection", s_ViewProjection);
        RenderCommand::DrawLines(s_GridVAO, s_GridVertexCount);
    }

    if (!s_DrawList.empty()) {
        s_MeshShader->Bind();
        s_MeshShader->SetMat4("u_ViewProjection", s_ViewProjection);
        s_MeshShader->SetMat4("u_View", s_View);
        s_MeshShader->SetFloat3("u_ViewPos", s_CameraPos);
        s_MeshShader->SetInt("u_HasShadow", s_HasShadowCaster ? 1 : 0);
        s_MeshShader->SetInt("u_ShadowPCF", s_Settings.ShadowPCFRadius);
        s_MeshShader->SetFloat("u_ShadowSoftness", s_Settings.ShadowSoftness);
        if (s_HasShadowCaster) {
            for (int c = 0; c < kCascadeCount; ++c) {
                std::string idx = "[" + std::to_string(c) + "]";
                s_MeshShader->SetMat4("u_LightSpaceMatrix" + idx, s_LightSpaceMatrix[c]);
                s_MeshShader->SetFloat("u_CascadeSplits" + idx, s_CascadeSplits[c]);
            }
        }

        // Unidade 0 é o AlbedoMap (setado por draw call); 1-3=sombra (cascatas), 4=normal, 5=irradiância, 6=prefiltro.
        for (int c = 0; c < kCascadeCount; ++c) {
            glActiveTexture(GL_TEXTURE1 + c);
            glBindTexture(GL_TEXTURE_2D, s_ShadowMap[c]);
            s_MeshShader->SetInt("u_ShadowMap" + std::to_string(c), 1 + c);
        }
        glActiveTexture(GL_TEXTURE5);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_IrradianceCubemap);
        s_MeshShader->SetInt("u_IrradianceMap", 5);
        glActiveTexture(GL_TEXTURE6);
        glBindTexture(GL_TEXTURE_CUBE_MAP, s_PrefilterCubemap);
        s_MeshShader->SetInt("u_PrefilterMap", 6);
        s_MeshShader->SetFloat("u_MaxPrefilterLod", (float)(kPrefilterMipLevels - 1));

        s_MeshShader->SetInt("u_FogEnabled", s_Settings.FogEnabled ? 1 : 0);
        s_MeshShader->SetFloat3("u_FogColor",
            glm::vec3(s_Settings.FogColor[0], s_Settings.FogColor[1], s_Settings.FogColor[2]));
        s_MeshShader->SetFloat("u_FogDensity", s_Settings.FogDensity);

        s_MeshShader->SetInt("u_LightCount", (int)s_LightList.size());
        for (size_t i = 0; i < s_LightList.size(); ++i) {
            const Light& l = s_LightList[i];
            std::string idx = "[" + std::to_string(i) + "]";
            s_MeshShader->SetInt("u_LightTypes" + idx, (int)l.Type);
            s_MeshShader->SetFloat3("u_LightPositions" + idx, l.Position);
            s_MeshShader->SetFloat3("u_LightDirections" + idx, l.Direction);
            s_MeshShader->SetFloat3("u_LightColors" + idx, l.Color);
            s_MeshShader->SetFloat("u_LightIntensities" + idx, l.Intensity);
            s_MeshShader->SetFloat("u_LightRanges" + idx, l.Range);
            s_MeshShader->SetFloat("u_LightInnerCos" + idx, glm::cos(glm::radians(l.InnerConeDeg)));
            s_MeshShader->SetFloat("u_LightOuterCos" + idx, glm::cos(glm::radians(l.OuterConeDeg)));
        }

        for (auto& cmd : s_DrawList) {
            s_MeshShader->SetMat4("u_Transform", cmd.Transform);
            s_MeshShader->SetMat3("u_NormalMatrix", glm::mat3(glm::transpose(glm::inverse(cmd.Transform))));
            s_MeshShader->SetFloat3("u_Albedo", cmd.Mat.Albedo);
            s_MeshShader->SetFloat("u_Metallic", cmd.Mat.Metallic);
            s_MeshShader->SetFloat("u_Roughness", cmd.Mat.Roughness);
            s_MeshShader->SetFloat("u_AO", cmd.Mat.AO);

            // Skinning: se o comando trouxe juntas, sobe as matrizes e liga
            // o branch no vertex shader (malha estática = weights 0 = identidade).
            if (cmd.Joints.empty()) {
                s_MeshShader->SetInt("u_Animated", 0);
            } else {
                s_MeshShader->SetInt("u_Animated", 1);
                for (uint32_t j = 0; j < kMaxSkinJoints; ++j) {
                    glm::mat4 m = (j < cmd.Joints.size()) ? cmd.Joints[j] : glm::mat4(1.0f);
                    s_MeshShader->SetMat4("u_JointMatrices[" + std::to_string(j) + "]", m);
                }
            }

            bool hasAlbedo = (bool)cmd.Mat.AlbedoMap;
            s_MeshShader->SetInt("u_HasAlbedoMap", hasAlbedo ? 1 : 0);
            if (hasAlbedo) {
                cmd.Mat.AlbedoMap->Bind(0);
                s_MeshShader->SetInt("u_AlbedoMap", 0);
            }
            bool hasNormal = (bool)cmd.Mat.NormalMap;
            s_MeshShader->SetInt("u_HasNormalMap", hasNormal ? 1 : 0);
            if (hasNormal) {
                cmd.Mat.NormalMap->Bind(4);
                s_MeshShader->SetInt("u_NormalMap", 4);
            }
            bool hasMR = (bool)cmd.Mat.MetallicRoughnessMap;
            s_MeshShader->SetInt("u_HasMetallicRoughnessMap", hasMR ? 1 : 0);
            if (hasMR) {
                cmd.Mat.MetallicRoughnessMap->Bind(7);
                s_MeshShader->SetInt("u_MetallicRoughnessMap", 7);
            }
            bool hasEmissive = (bool)cmd.Mat.EmissiveMap;
            s_MeshShader->SetInt("u_HasEmissiveMap", hasEmissive ? 1 : 0);
            if (hasEmissive) {
                cmd.Mat.EmissiveMap->Bind(8);
                s_MeshShader->SetInt("u_EmissiveMap", 8);
            }
            bool hasHeight = (bool)cmd.Mat.HeightMap;
            s_MeshShader->SetInt("u_HasHeightMap", hasHeight ? 1 : 0);
            s_MeshShader->SetFloat("u_HeightScale", cmd.Mat.HeightScale);
            if (hasHeight) {
                cmd.Mat.HeightMap->Bind(9);
                s_MeshShader->SetInt("u_HeightMap", 9);
            }
            s_MeshShader->SetFloat3("u_Emissive", cmd.Mat.Emissive);
            s_MeshShader->SetFloat("u_EmissiveStrength", cmd.Mat.EmissiveStrength);

            // Espelho real: só o material marcado amostra a textura da câmera
            // refletida (os demais ficam com o caminho IBL/SSR normal).
            bool isMirror = cmd.Mat.PlanarReflect && s_HasPlanarReflection;
            s_MeshShader->SetInt("u_IsPlanarMirror", isMirror ? 1 : 0);
            if (isMirror) {
                s_MeshShader->SetMat4("u_ReflectionViewProjection", s_ReflectionViewProjection);
                glActiveTexture(GL_TEXTURE10);
                glBindTexture(GL_TEXTURE_2D, s_PlanarColor);
                s_MeshShader->SetInt("u_PlanarReflectionMap", 10);
            }

            RenderCommand::DrawIndexed(cmd.MeshAsset->GetVertexArray(), cmd.MeshAsset->GetIndexCount());
        }
    }

    // Skybox por último — LEQUAL + sem escrita de profundidade, só aparece onde nada mais desenhou.
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    s_SkyboxShader->Bind();
    s_SkyboxShader->SetMat4("u_View", glm::mat4(glm::mat3(s_View)));
    s_SkyboxShader->SetMat4("u_Projection", s_Projection);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_CUBE_MAP, s_EnvironmentCubemap);
    s_SkyboxShader->SetInt("u_EnvironmentMap", 0);
    // Céu atmosférico: o sol segue a luz direcional (sombras batem na mesma
    // direção do disco do sol). Sem luz direcional, usa um sol padrão.
    glm::vec3 sunDir = s_HasShadowCaster
        ? glm::normalize(-s_ShadowCaster.Direction)
        : glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
    s_SkyboxShader->SetFloat3("u_SunDirection", sunDir);
    // HDRI carregado = o usuário quer ver o ambiente dele; senão, scattering.
    s_SkyboxShader->SetInt("u_AtmosphereSky", s_EnvironmentHDRIPath.empty() ? 1 : 0);
    RenderCommand::DrawIndexed(s_CaptureCube->GetVertexArray(), s_CaptureCube->GetIndexCount());
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LESS);

    // Partículas: testam profundidade contra a geometria real (ficam atrás de paredes),
    // mas não escrevem (partículas sobrepostas se misturam entre si, não se ocluem).
    if (!s_ParticleBatches.empty()) {
        glm::vec3 cameraRight(s_View[0][0], s_View[1][0], s_View[2][0]);
        glm::vec3 cameraUp(s_View[0][1], s_View[1][1], s_View[2][1]);
        s_ParticleShader->Bind();
        s_ParticleShader->SetMat4("u_ViewProjection", s_ViewProjection);
        s_ParticleShader->SetFloat3("u_CameraRight", cameraRight);
        s_ParticleShader->SetFloat3("u_CameraUp", cameraUp);
        glDepthMask(GL_FALSE);
        RenderCommand::SetBlending(true);
        glBindVertexArray(s_ParticleVAO);
        for (auto& batch : s_ParticleBatches) {
            glBlendFunc(GL_SRC_ALPHA, batch.Additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA);
            s_ParticleShader->SetInt("u_HasTexture", (batch.Texture) ? 1 : 0);
            if (batch.Texture) {
                batch.Texture->Bind(0);
                s_ParticleShader->SetInt("u_ParticleTexture", 0);
            }
            glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(batch.Instances.size() * sizeof(ParticleInstance)), batch.Instances.data());
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, (GLsizei)batch.Instances.size());
            RenderCommand::AddInstancedStats((uint32_t)batch.Instances.size()); // 1 draw + instâncias*2 tris
        }
        glBindVertexArray(0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // volta pro padrão
        RenderCommand::SetBlending(false);
        glDepthMask(GL_TRUE);
    }

    // Resolve MSAA -> destino simples (cor + profundidade). Sem MSAA a cena
    // já foi renderizada direto no s_HDRFBO, não precisa de blit.
    if (s_CurrentMSAA > 1) {
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_MSAAHDRFBO);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, s_HDRFBO);
        glBlitFramebuffer(0, 0, (GLint)internalW, (GLint)internalH,
                          0, 0, (GLint)internalW, (GLint)internalH,
                          GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT, GL_NEAREST);
    }

    // A partir daqui são passes de tela cheia sobre texturas — o teste de
    // profundidade não faz sentido e o quad de tela inteira se perderia na
    // geometria da cena se ele continuasse ligado.
    RenderCommand::SetDepthTest(false);

    // --- Passe 1.5: SSAO (do depth resolvido), meia resolução + blur ---
    bool hasAO = false;
    if (s_Settings.SSAOEnabled) {
        EnsureSSAOBuffers(internalW, internalH);

        glBindFramebuffer(GL_FRAMEBUFFER, s_SSAOFBO);
        glViewport(0, 0, (GLsizei)s_SSAOWidth, (GLsizei)s_SSAOHeight);
        s_SSAOShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_HDRDepthTexture);
        s_SSAOShader->SetInt("u_Depth", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_NoiseTexture);
        s_SSAOShader->SetInt("u_Noise", 1);
        s_SSAOShader->SetMat4("u_Projection", s_Projection);
        s_SSAOShader->SetMat4("u_InverseProjection", glm::inverse(s_Projection));
        s_SSAOShader->SetFloat("u_Radius", s_Settings.SSAORadius);
        s_SSAOShader->SetFloat("u_Power", 1.2f); // 2.0 escurecia demais (relato de cena 'meio escura' no 4.0)
        s_SSAOShader->SetInt("u_SampleCount", s_Settings.SSAOSamples);
        for (size_t i = 0; i < s_SSAOKernel.size(); ++i)
            s_SSAOShader->SetFloat3("u_Samples[" + std::to_string(i) + "]", s_SSAOKernel[i]);
        s_SSAOShader->SetFloat2("u_NoiseScale", glm::vec2((float)s_SSAOWidth / 4.0f, (float)s_SSAOHeight / 4.0f));
        RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

        // Blur separável (reaproveita o gaussiano do bloom) pra esconder o ruído.
        int aoRead = 0, aoWrite = 1;
        s_BlurShader->Bind();
        for (int i = 0; i < 2; ++i) {
            uint32_t writeFBO = (aoWrite == 0) ? s_SSAOFBO : s_SSAOBlurFBO;
            uint32_t readTex  = (aoRead == 0) ? s_SSAOColorBuffer : s_SSAOBlurBuffer;
            glBindFramebuffer(GL_FRAMEBUFFER, writeFBO);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, readTex);
            s_BlurShader->SetInt("u_Image", 0);
            s_BlurShader->SetInt("u_Horizontal", (i == 0) ? 1 : 0);
            RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
            std::swap(aoRead, aoWrite);
        }
        hasAO = true;
    }

    // --- Passe 1.75: SSR (reflexos em espaço de tela) — cor + depth resolvidos,
    // marcha do raio refletido com loop de PASSOS FIXOS (seguro em 3.3) ---
    bool hasSSR = false;
    if (s_Settings.SSREnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_SSRFBO);
        glViewport(0, 0, (GLsizei)internalW, (GLsizei)internalH);
        s_SSRShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
        s_SSRShader->SetInt("u_SceneColor", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_HDRDepthTexture);
        s_SSRShader->SetInt("u_Depth", 1);
        s_SSRShader->SetMat4("u_Projection", s_Projection);
        s_SSRShader->SetMat4("u_InverseProjection", glm::inverse(s_Projection));
        s_SSRShader->SetInt("u_MaxSteps", s_Settings.SSRMaxSteps);
        s_SSRShader->SetFloat("u_Thickness", s_Settings.SSRThickness);
        s_SSRShader->SetFloat("u_Intensity", s_Settings.SSRIntensity);
        s_SSRShader->SetFloat("u_MarchDistance", s_Settings.SSRMarchDistance);
        s_SSRShader->SetFloat2("u_ViewportSize", glm::vec2((float)internalW, (float)internalH));
        RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
        hasSSR = true;
    }

    // --- Passe 1.8: God rays / luz volumétrica (meia resolução) — marcha
    // radial do pixel até o sol na tela acumulando o brilho da cena ---
    bool hasGodRays = false;
    if (s_Settings.GodRaysEnabled && s_Settings.GodRaysIntensity > 0.001f) {
        glm::vec3 sunDir = s_HasShadowCaster
            ? glm::normalize(-s_ShadowCaster.Direction)
            : glm::normalize(glm::vec3(0.3f, 1.0f, 0.2f));
        glm::vec4 sunClip = s_Projection * (s_View * glm::vec4(sunDir, 0.0f));
        if (sunClip.w > 0.0f) {
            glm::vec2 sunNDC = glm::vec2(sunClip) / sunClip.w;
            glm::vec2 sunUV = sunNDC * 0.5f + 0.5f;
            // Só renderiza se o sol está (quase) dentro da tela.
            if (sunUV.x >= -0.6f && sunUV.x <= 1.6f && sunUV.y >= -0.6f && sunUV.y <= 1.6f) {
                uint32_t grW = glm::max(1u, internalW / 2), grH = glm::max(1u, internalH / 2);
                glBindFramebuffer(GL_FRAMEBUFFER, s_GodRaysFBO);
                glViewport(0, 0, (GLsizei)grW, (GLsizei)grH);
                s_GodRaysShader->Bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
                s_GodRaysShader->SetInt("u_SceneColor", 0);
                s_GodRaysShader->SetFloat2("u_SunUV", sunUV);
                s_GodRaysShader->SetFloat("u_Density", 0.15f);
                s_GodRaysShader->SetFloat("u_Decay", 0.92f);
                s_GodRaysShader->SetFloat("u_Weight", 0.045f);
                s_GodRaysShader->SetFloat("u_Intensity", s_Settings.GodRaysIntensity);
                RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

                // Um passe de blur horizontal pra suavizar os raios.
                s_BlurShader->Bind();
                glActiveTexture(GL_TEXTURE0);
                glBindTexture(GL_TEXTURE_2D, s_GodRaysColorBuffer);
                s_BlurShader->SetInt("u_Image", 0);
                s_BlurShader->SetInt("u_Horizontal", 1);
                RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
                hasGodRays = true;
            }
        }
    }

    // --- Passe 2: bright-pass (meia resolução) — extrai o glow, se bloom ligado ---
    uint32_t bloomW = glm::max(1u, internalW / 2), bloomH = glm::max(1u, internalH / 2);
    int bloomReadIdx = 0;
    if (s_Settings.BloomEnabled) {
        glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[0]);
        glViewport(0, 0, (GLsizei)bloomW, (GLsizei)bloomH);
        s_BrightPassShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
        s_BrightPassShader->SetInt("u_SceneColor", 0);
        s_BrightPassShader->SetFloat("u_Threshold", s_Settings.BloomThreshold);
        RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

        // --- Passe 3: blur gaussiano separável, ping-pong entre os dois FBOs de bloom ---
        bool horizontal = true;
        uint32_t blurIterations = glm::max(1u, (uint32_t)s_Settings.BloomIterations);
        s_BlurShader->Bind();
        for (uint32_t i = 0; i < blurIterations * 2; ++i) {
            int writeIdx = 1 - bloomReadIdx;
            glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[writeIdx]);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, s_BloomColorBuffer[bloomReadIdx]);
            s_BlurShader->SetInt("u_Image", 0);
            s_BlurShader->SetInt("u_Horizontal", horizontal ? 1 : 0);
            RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
            bloomReadIdx = writeIdx;
            horizontal = !horizontal;
        }
    }

    // --- Passe 4: composição final (HDR + bloom + AO + exposição, tonemap ACES) ---
    // Com TAA ligado o composite escreve num alvo intermediário (resolução
    // interna); o passe seguinte mistura com o histórico e blita pro destino.
    GLuint compositeTarget = (GLuint)prevFBO;
    int compositeW = prevViewport[2], compositeH = prevViewport[3];
    if (taaEnabled) {
        compositeTarget = s_TAACompositeFBO;
        compositeW = (int)internalW;
        compositeH = (int)internalH;
    }
    glBindFramebuffer(GL_FRAMEBUFFER, compositeTarget);
    glViewport(0, 0, (GLsizei)compositeW, (GLsizei)compositeH);
    s_CompositeShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
    s_CompositeShader->SetInt("u_SceneColor", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_Settings.BloomEnabled ? s_BloomColorBuffer[bloomReadIdx] : s_HDRColorBuffer);
    s_CompositeShader->SetInt("u_BloomBlur", 1);
    s_CompositeShader->SetFloat("u_BloomIntensity", s_Settings.BloomEnabled ? s_Settings.BloomIntensity : 0.0f);
    s_CompositeShader->SetFloat("u_Exposure", s_Settings.Exposure);
    s_CompositeShader->SetFloat("u_Vignette", s_Settings.Vignette);
    s_CompositeShader->SetFloat("u_ChromaticAberration", s_Settings.ChromaticAberration);
    s_CompositeShader->SetFloat("u_FilmGrain", s_Settings.FilmGrain);
    s_CompositeShader->SetInt("u_ToneMapping", s_Settings.ToneMapping);
    s_PostTime += 0.016f; // grão de filme animado (relógio de pós-processamento)
    s_CompositeShader->SetFloat("u_Time", s_PostTime);
    s_CompositeShader->SetInt("u_HasAO", hasAO ? 1 : 0);
    if (hasAO) {
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, s_SSAOColorBuffer); // borrada (passe 2 do blur volta pro s_SSAOFBO)
        s_CompositeShader->SetInt("u_AOTexture", 2);
    }
    s_CompositeShader->SetInt("u_HasSSR", hasSSR ? 1 : 0);
    if (hasSSR) {
        glActiveTexture(GL_TEXTURE3);
        glBindTexture(GL_TEXTURE_2D, s_SSRColorBuffer);
        s_CompositeShader->SetInt("u_SSRTexture", 3);
    }
    s_CompositeShader->SetInt("u_HasGodRays", hasGodRays ? 1 : 0);
    if (hasGodRays) {
        glActiveTexture(GL_TEXTURE4);
        glBindTexture(GL_TEXTURE_2D, s_GodRaysColorBuffer);
        s_CompositeShader->SetInt("u_GodRaysTexture", 4);
    }
    RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

    // --- Passe 4.5: TAA — mistura com o histórico (clamp de vizinhança) e
    // blita pro destino. O histórico antigo (read) vira o novo (write) ---
    if (taaEnabled) {
        int histRead = (int)(s_TAACounter % 2u);
        int histWrite = 1 - histRead;
        glBindFramebuffer(GL_FRAMEBUFFER, s_TAAHistoryFBO[histWrite]);
        glViewport(0, 0, (GLsizei)internalW, (GLsizei)internalH);
        s_TAAShader->Bind();
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_TAACompositeTex);
        s_TAAShader->SetInt("u_Current", 0);
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, s_TAAHistoryTex[histRead]);
        s_TAAShader->SetInt("u_History", 1);
        s_TAAShader->SetFloat2("u_InvResolution", glm::vec2(1.0f / (float)internalW, 1.0f / (float)internalH));
        s_TAAShader->SetFloat("u_HistoryWeight", 0.1f);
        s_TAAShader->SetInt("u_UseHistory", s_TAAHistoryValid ? 1 : 0);
        RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

        // Blit pro destino real (up/downscale pro tamanho do alvo).
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_TAAHistoryFBO[histWrite]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, (GLuint)prevFBO);
        glBlitFramebuffer(0, 0, (GLint)internalW, (GLint)internalH,
                          prevViewport[0], prevViewport[1], (GLsizei)prevViewport[2], (GLsizei)prevViewport[3],
                          GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
        s_TAAHistoryValid = true;
    }

    RenderCommand::SetDepthTest(true);

    // Diagnóstico: qualquer erro de driver neste frame aparece no log (ex.:
    // GL_INVALID_FRAMEBUFFER_OPERATION = FBO incompleto, que é tela preta).
    if (GLenum err = glGetError(); err != GL_NO_ERROR) {
        KZ_CORE_ERROR("OpenGL erro 0x{0:x} no frame (pós).", (unsigned)err);
        char buf[64];
        snprintf(buf, sizeof(buf), "OpenGL erro 0x%04x", (unsigned)err);
        SetShaderDiagnostic(buf);
    }

    // Detecção de VIEWPORT PRETO no 1º frame: a cena padrão nunca é preta de
    // verdade — se o readback do centro do HDR buffer vier ~0, algo do pipeline
    // falhou silenciosamente. Loga nos 3 primeiros frames (a progressão mostra
    // se a degradação automática resolveu), degrada no 1º e grava render_info.
    static int s_BlackFrames = 0;
    if (s_BlackFrames < 3) {
        ++s_BlackFrames;
        glBindFramebuffer(GL_READ_FRAMEBUFFER, s_HDRFBO);
        float px[3] = { 1.0f, 1.0f, 1.0f };
        glReadPixels((GLint)(internalW / 2), (GLint)(internalH / 2),
                     1, 1, GL_RGB, GL_FLOAT, px); // RGB combina com o RGB16F
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        float lum = px[0] + px[1] + px[2];
        KZ_CORE_INFO("Verificação de frame {0}: centro do HDR = ({1:.3f}, {2:.3f}, {3:.3f}).",
                     s_BlackFrames, px[0], px[1], px[2]);

        if (s_BlackFrames == 1) {
            // Sempre grava um dossiê de render (render_info.txt ao lado do exe).
            std::ofstream dump("render_info.txt");
            dump << "Kizuri Engine — diagnostico de render\n";
            dump << "OpenGL: " << GetOpenGLVersionString() << "\n";
            dump << "GLSL core: " << GetGLSLVersion() << "\n";
            GLint maxS = 0; glGetIntegerv(GL_MAX_SAMPLES, &maxS);
            dump << "MSAA: config " << s_Settings.MSAA << " / ativo " << s_CurrentMSAA << " (max samples " << maxS << ")\n";
            dump << "SSAO: " << (s_Settings.SSAOEnabled ? "on" : "off") << "\n";
            dump << "SSR: " << (s_Settings.SSREnabled ? "on" : "off") << "\n";
            dump << "TAA: " << (s_Settings.TAAEnabled ? "on" : "off") << "\n";
            dump << "PCSS (penumbra): " << s_Settings.ShadowSoftness << "\n";
            dump << "God rays: " << (s_Settings.GodRaysEnabled ? "on" : "off") << "\n";
            dump << "Bloom: " << (s_Settings.BloomEnabled ? "on" : "off") << "\n";
            auto fbStatus = [](GLuint fbo) {
                if (fbo == 0) return 0;
                glBindFramebuffer(GL_FRAMEBUFFER, fbo);
                GLenum s = glCheckFramebufferStatus(GL_FRAMEBUFFER);
                glBindFramebuffer(GL_FRAMEBUFFER, 0);
                return (int)s;
            };
            auto ShaderOk = [](const Ref<Shader>& sh) { return sh ? (sh->IsValid() ? "ok" : "INVALIDO") : "nao criado"; };
            dump << "FBO HDR: 0x" << std::hex << fbStatus(s_HDRFBO) << std::dec << "\n";
            dump << "FBO MSAA: 0x" << std::hex << fbStatus(s_MSAAHDRFBO) << std::dec << "\n";
            dump << "FBO SSAO: 0x" << std::hex << fbStatus(s_SSAOFBO) << std::dec << "\n";
            dump << "Shader mesh: " << ShaderOk(s_MeshShader) << "\n";
            dump << "Shader skybox: " << ShaderOk(s_SkyboxShader) << "\n";
            dump << "Shader composite: " << ShaderOk(s_CompositeShader) << "\n";
            dump << "Shader shadow: " << ShaderOk(s_ShadowShader) << "\n";
            dump.close();
        }

        if (lum < 0.001f) {
            char msg[256];
            snprintf(msg, sizeof(msg),
                     "VIEWPORT PRETO — HDR centro (%.3f, %.3f, %.3f) — compatibilidade aplicada",
                     px[0], px[1], px[2]);
            KZ_CORE_ERROR("{0}", msg);
            SetShaderDiagnostic(msg);
            GraphicsSettings basic = s_Settings;
            basic.MSAA = 1;
            basic.SSAOEnabled = false;
            basic.BloomEnabled = false;
            basic.Preset = QualityPreset::Custom;
            s_Settings = basic;
        }
    }

    s_DrawList.clear();
}

void Renderer3D::DrawGrid() {
    s_DrawGridFlag = true;
}

void Renderer3D::Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform) {
    s_DrawList.push_back({ mesh, material, transform, {} });
}

void Renderer3D::SubmitSkinned(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform,
                               const glm::mat4* jointMatrices, uint32_t jointCount) {
    DrawCommand cmd;
    cmd.MeshAsset = mesh;
    cmd.Mat = material;
    cmd.Transform = transform;
    if (jointMatrices && jointCount > 0) {
        uint32_t count = jointCount < (uint32_t)kMaxSkinJoints ? jointCount : (uint32_t)kMaxSkinJoints;
        cmd.Joints.reserve(count);
        for (uint32_t i = 0; i < count; ++i) cmd.Joints.push_back(jointMatrices[i]);
    }
    s_DrawList.push_back(std::move(cmd));
}

} // namespace kizuri
