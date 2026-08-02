#include "kizuri/renderer/Renderer3D.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/core/Log.hpp"
#include <glad/gl.h>
#include <glm/gtc/matrix_transform.hpp>
#include <string>
#include <cstddef>

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

uint32_t Renderer3D::s_EnvironmentCubemap = 0;
uint32_t Renderer3D::s_IrradianceCubemap = 0;
uint32_t Renderer3D::s_PrefilterCubemap = 0;
Ref<Mesh> Renderer3D::s_CaptureCube;
Ref<Shader> Renderer3D::s_SkyboxShader;
glm::mat4 Renderer3D::s_View = glm::mat4(1.0f);
glm::mat4 Renderer3D::s_Projection = glm::mat4(1.0f);
float Renderer3D::s_CamFOV = 45.0f, Renderer3D::s_CamAspect = 16.0f / 9.0f;
float Renderer3D::s_CamNear = 0.1f, Renderer3D::s_CamFar = 1000.0f;

uint32_t Renderer3D::s_HDRFBO = 0, Renderer3D::s_HDRColorBuffer = 0, Renderer3D::s_HDRDepthRBO = 0;
uint32_t Renderer3D::s_BloomFBO[2] = {}, Renderer3D::s_BloomColorBuffer[2] = {};
uint32_t Renderer3D::s_PostWidth = 0, Renderer3D::s_PostHeight = 0;
Ref<VertexArray> Renderer3D::s_FullscreenQuad;
Ref<Shader> Renderer3D::s_BrightPassShader, Renderer3D::s_BlurShader, Renderer3D::s_CompositeShader;
bool Renderer3D::s_DrawGridFlag = false;

std::vector<Renderer3D::ParticleBatch> Renderer3D::s_ParticleBatches;
uint32_t Renderer3D::s_ParticleVAO = 0, Renderer3D::s_ParticleQuadVBO = 0;
uint32_t Renderer3D::s_ParticleEBO = 0, Renderer3D::s_ParticleInstanceVBO = 0;
Ref<Shader> Renderer3D::s_ParticleShader;

static constexpr uint32_t kEnvironmentSize = 128;
static constexpr uint32_t kIrradianceSize = 32;
static constexpr uint32_t kPrefilterBaseSize = 128;
static constexpr uint32_t kPrefilterMipLevels = 5;

static constexpr uint32_t kShadowMapSize = 2048;
static constexpr uint32_t kMaxLights = 16;
static constexpr uint32_t kBloomBlurIterations = 4; // 4x horizontal + 4x vertical, ping-pong

static const char* s_MeshVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec3 a_Normal;
layout(location = 2) in vec2 a_TexCoord;

uniform mat4 u_ViewProjection;
uniform mat4 u_Transform;
uniform mat3 u_NormalMatrix;

out vec3 v_FragPos;
out vec3 v_Normal;
out vec2 v_TexCoord;

void main() {
    v_FragPos = vec3(u_Transform * vec4(a_Position, 1.0));
    v_Normal = u_NormalMatrix * a_Normal;
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
uniform sampler2D u_AlbedoMap;
uniform bool u_HasAlbedoMap;
uniform sampler2D u_NormalMap;
uniform bool u_HasNormalMap;

uniform vec3 u_ViewPos;
uniform mat4 u_View;
uniform bool u_HasShadow; // se a luz índice 0 é a que projeta sombra

#define CASCADE_COUNT 3
uniform sampler2D u_ShadowMap0;
uniform sampler2D u_ShadowMap1;
uniform sampler2D u_ShadowMap2;
uniform mat4 u_LightSpaceMatrix[CASCADE_COUNT];
uniform float u_CascadeSplits[CASCADE_COUNT]; // distância (view-space) onde cada cascata termina

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

const float PI = 3.14159265359;

// PCF 3x3 num sampler já escolhido — média numa vizinhança pequena em vez de um sample só.
float SampleShadowPCF(sampler2D map, vec3 projCoords, float bias) {
    if (projCoords.z > 1.0) return 0.0;
    float shadow = 0.0;
    vec2 texelSize = 1.0 / textureSize(map, 0);
    for (int x = -1; x <= 1; ++x) {
        for (int y = -1; y <= 1; ++y) {
            float pcfDepth = texture(map, projCoords.xy + vec2(x, y) * texelSize).r;
            shadow += (projCoords.z - bias > pcfDepth) ? 1.0 : 0.0;
        }
    }
    return shadow / 9.0;
}

// Escolhe a cascata pela profundidade view-space e amostra o shadow map certo — GLSL 330
// core não permite indexar array de sampler com índice dinâmico, daí o if/else explícito.
float CalculateShadow(vec3 N, vec3 L) {
    float viewDepth = -(u_View * vec4(v_FragPos, 1.0)).z;
    int idx = (viewDepth < u_CascadeSplits[0]) ? 0 : (viewDepth < u_CascadeSplits[1]) ? 1 : 2;
    float bias = max(0.0025 * (1.0 - dot(N, L)), 0.0006);

    if (idx == 0) {
        vec4 p = u_LightSpaceMatrix[0] * vec4(v_FragPos, 1.0);
        return SampleShadowPCF(u_ShadowMap0, p.xyz / p.w * 0.5 + 0.5, bias);
    } else if (idx == 1) {
        vec4 p = u_LightSpaceMatrix[1] * vec4(v_FragPos, 1.0);
        return SampleShadowPCF(u_ShadowMap1, p.xyz / p.w * 0.5 + 0.5, bias);
    } else {
        vec4 p = u_LightSpaceMatrix[2] * vec4(v_FragPos, 1.0);
        return SampleShadowPCF(u_ShadowMap2, p.xyz / p.w * 0.5 + 0.5, bias);
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
vec3 ApplyNormalMap(vec3 N) {
    vec3 tangentNormal = texture(u_NormalMap, v_TexCoord).xyz * 2.0 - 1.0;
    vec3 Q1 = dFdx(v_FragPos), Q2 = dFdy(v_FragPos);
    vec2 st1 = dFdx(v_TexCoord), st2 = dFdy(v_TexCoord);
    vec3 T = normalize(Q1 * st2.t - Q2 * st1.t);
    vec3 B = -normalize(cross(N, T));
    return normalize(mat3(T, B, N) * tangentNormal);
}

void main() {
    vec3 albedo = u_HasAlbedoMap ? texture(u_AlbedoMap, v_TexCoord).rgb * u_Albedo : u_Albedo;

    vec3 N = normalize(v_Normal);
    if (u_HasNormalMap) N = ApplyNormalMap(N);

    vec3 V = normalize(u_ViewPos - v_FragPos);
    vec3 R = reflect(-V, N);
    float NdotV = max(dot(N, V), 0.0);

    // Dielétrico ~4% de F0 fixo; metal usa o próprio albedo como F0 (colore o especular).
    vec3 F0 = mix(vec3(0.04), albedo, u_Metallic);
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
        float NDF = DistributionGGX(N, H, u_Roughness);
        float G = GeometrySmith(N, V, L, u_Roughness);
        vec3 F = FresnelSchlick(max(dot(H, V), 0.0), F0);
        vec3 specularBRDF = (NDF * G * F) / max(4.0 * NdotV * NdotL, 0.0001);

        vec3 kS = F;
        vec3 kD = (vec3(1.0) - kS) * (1.0 - u_Metallic);
        vec3 radiance = u_LightColors[i] * u_LightIntensities[i] * attenuation;
        vec3 contribution = (kD * albedo / PI + specularBRDF) * radiance * NdotL;

        // Sombra só se aplica à luz 0 quando ela é a shadow caster (sempre a Directional, ver C++).
        if (i == 0 && u_HasShadow) contribution *= (1.0 - shadow);
        directLighting += contribution;
    }

    // --- Luz ambiente (IBL) — nunca é sombreada, luz indireta ainda chega em área de sombra ---
    vec3 F_ibl = FresnelSchlickRoughness(NdotV, F0, u_Roughness);
    vec3 kD_ibl = (vec3(1.0) - F_ibl) * (1.0 - u_Metallic);
    vec3 diffuseIBL = texture(u_IrradianceMap, N).rgb * albedo;
    vec3 prefilteredColor = textureLod(u_PrefilterMap, R, u_Roughness * u_MaxPrefilterLod).rgb;
    vec2 envBRDF = EnvBRDFApprox(NdotV, u_Roughness);
    vec3 specularIBL = prefilteredColor * (F_ibl * envBRDF.x + envBRDF.y);
    vec3 ambient = (kD_ibl * diffuseIBL + specularIBL) * u_AO;

    // Sem tonemap aqui — sai HDR linear cru de propósito. O passe de composição
    // (bright-pass + bloom + ACES) é quem faz o tonemap, uma vez só, no final.
    o_Color = vec4(ambient + directLighting, 1.0);
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

// Céu procedural: gradiente horizonte->zênite acima do horizonte, um chão
// escuro simples abaixo (evita "buraco preto" olhando pra baixo), e um sol
// desenhado como disco + halo na direção da luz direcional ativa. Não
// depende de nenhuma textura/HDRI externa — consistente com o resto da
// engine (fontes embutidas, sem pasta assets/ obrigatória).
static const char* s_SkyFragmentSrc = R"(
#version 330 core
in vec3 v_LocalPos;
out vec4 o_Color;

uniform vec3 u_SunDir;   // aponta DA superfície PRA o sol (oposto de DirectionalLight::Direction)
uniform vec3 u_SunColor;

void main() {
    vec3 dir = normalize(v_LocalPos);

    // Cores em HDR linear. Cuidado: passam por tonemap ACES + gamma no fim do
    // pipeline — valores claros demais (ex: horizonte 0.45) explodem pra ~0.8
    // depois do ACES e viram um branco "névoa" em vez de céu azul (era esse o
    // sintoma reportado: céu parecendo neblina e uma faixa clara sobre o grid,
    // que fica exatamente na linha do horizonte). Por isso o horizonte e o
    // zenite ficam propositalmente escuros/saturados aqui.
    vec3 horizonColor = vec3(0.12, 0.22, 0.42);
    vec3 zenithColor   = vec3(0.04, 0.13, 0.40);
    vec3 groundColor   = vec3(0.07, 0.08, 0.10);

    vec3 sky = (dir.y >= 0.0)
        ? mix(horizonColor, zenithColor, smoothstep(-0.05, 0.35, dir.y))
        : mix(horizonColor, groundColor, smoothstep(0.0, 0.6, -dir.y));

    float sunDot = max(dot(dir, normalize(u_SunDir)), 0.0);
    sky += u_SunColor * pow(sunDot, 900.0) * 12.0; // disco do sol, pequeno e intenso
    sky += u_SunColor * pow(sunDot, 16.0) * 0.10;  // halo suave ao redor

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

void main() {
    // Sem tonemap aqui de propósito — o disco do sol sai bem acima de 1.0 e
    // vira "combustível" pro bright-pass do bloom no passe de composição.
    o_Color = vec4(texture(u_EnvironmentMap, normalize(v_LocalPos)).rgb, 1.0);
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

// Soma cena HDR + bloom, aplica tonemap ACES (aproximação de Narkowicz) e gamma —
// é o único lugar do pipeline inteiro que converte de HDR linear pra LDR de tela.
static const char* s_CompositeFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
out vec4 o_Color;
uniform sampler2D u_SceneColor;
uniform sampler2D u_BloomBlur;
uniform float u_BloomIntensity;

vec3 ACESFilm(vec3 x) {
    float a = 2.51, b = 0.03, c = 2.43, d = 0.59, e = 0.14;
    return clamp((x * (a * x + b)) / (x * (c * x + d) + e), 0.0, 1.0);
}

void main() {
    vec3 hdr = texture(u_SceneColor, v_TexCoord).rgb;
    vec3 bloom = texture(u_BloomBlur, v_TexCoord).rgb;
    vec3 color = hdr + bloom * u_BloomIntensity;
    color = ACESFilm(color);
    color = pow(color, vec3(1.0 / 2.2));
    o_Color = vec4(color, 1.0);
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
static const char* s_ParticleFragmentSrc = R"(
#version 330 core
in vec2 v_TexCoord;
in vec4 v_Color;
out vec4 o_Color;

void main() {
    float dist = length(v_TexCoord - vec2(0.5)) * 2.0;
    float falloff = 1.0 - smoothstep(0.0, 1.0, dist);
    falloff *= falloff;
    o_Color = vec4(v_Color.rgb, v_Color.a * falloff);
}
)";

// Shader do passe de sombra: só precisa escrever profundidade, não tem
// saída de cor nenhuma — gl_Position já é suficiente, o resto o hardware
// resolve sozinho gravando no depth buffer do framebuffer de sombra.
static const char* s_ShadowVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;

uniform mat4 u_LightSpaceMatrix;
uniform mat4 u_Model;

void main() {
    gl_Position = u_LightSpaceMatrix * u_Model * vec4(a_Position, 1.0);
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
    s_ShadowShader = CreateRef<Shader>("Renderer3D_Shadow", s_ShadowVertexSrc, s_ShadowFragmentSrc);

    float borderColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f }; // fora do shadow map = nunca em sombra
    for (int i = 0; i < kCascadeCount; ++i) {
        glGenFramebuffers(1, &s_ShadowFBO[i]);
        glGenTextures(1, &s_ShadowMap[i]);
        glBindTexture(GL_TEXTURE_2D, s_ShadowMap[i]);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT24, (GLsizei)kShadowMapSize, (GLsizei)kShadowMapSize,
                     0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
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

// (Re)cria o framebuffer HDR e os dois de bloom (meia resolução) se o tamanho do
// destino mudou desde a última chamada — evita realocar textura todo frame à toa.
void Renderer3D::EnsurePostBuffers(uint32_t width, uint32_t height) {
    if (width == s_PostWidth && height == s_PostHeight && s_HDRFBO != 0) return;
    s_PostWidth = width;
    s_PostHeight = height;
    uint32_t bloomW = glm::max(1u, width / 2), bloomH = glm::max(1u, height / 2);

    if (s_HDRFBO != 0) {
        glDeleteFramebuffers(1, &s_HDRFBO);
        glDeleteTextures(1, &s_HDRColorBuffer);
        glDeleteRenderbuffers(1, &s_HDRDepthRBO);
        glDeleteFramebuffers(2, s_BloomFBO);
        glDeleteTextures(2, s_BloomColorBuffer);
    }

    glGenFramebuffers(1, &s_HDRFBO);
    glGenTextures(1, &s_HDRColorBuffer);
    glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB16F, (GLsizei)width, (GLsizei)height, 0, GL_RGB, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glGenRenderbuffers(1, &s_HDRDepthRBO);
    glBindRenderbuffer(GL_RENDERBUFFER, s_HDRDepthRBO);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, (GLsizei)width, (GLsizei)height);

    glBindFramebuffer(GL_FRAMEBUFFER, s_HDRFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, s_HDRColorBuffer, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, s_HDRDepthRBO);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        KZ_CORE_ERROR("Framebuffer HDR incompleto.");

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
        if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
            KZ_CORE_ERROR("Framebuffer de bloom {0} incompleto.", i);
    }
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

// Gera os três cubemaps de IBL (ver comentário em Renderer3D.hpp) num bake
// único, síncrono, aqui em Init(). É uma limitação honesta da v1: o
// ambiente reflete a direção do sol no momento em que a engine iniciou,
// não a direção atual (que pode mudar se o usuário editar a
// DirectionalLight pela cena/Inspetor) — recalcular isso em tempo real a
// cada frame seria caro demais (a convolução de irradiância sozinha é
// ~2500 amostras por texel). Evolução natural: expor um
// Renderer3D::RegenerateEnvironment() e chamar sob demanda (mudança
// significativa de sol, ou botão manual no editor) em vez de todo frame —
// registrado como próximo passo, não esquecimento.
void Renderer3D::GenerateEnvironment() {
    KZ_TRACE_SCOPE("Renderer3D::GenerateEnvironment");

    s_CaptureCube = Mesh::CreateCube();
    s_SkyboxShader = CreateRef<Shader>("Renderer3D_Skybox", s_SkyboxVertexSrc, s_SkyboxFragmentSrc);
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

    // Sol padrão pro bake: mesma direção default de DirectionalLight (ver
    // Renderer3D.hpp) — se a cena não tiver luz nenhuma ainda quando o
    // ambiente é gerado (é sempre esse o caso, Init() roda antes de
    // qualquer Scene existir), o céu já nasce coerente com o sol default.
    DirectionalLight defaultSun;
    glm::vec3 sunDirToLight = glm::normalize(-defaultSun.Direction);

    // --- 1. Cubemap de ambiente (céu procedural) ------------------------
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

    KZ_CORE_INFO("Renderer3D::GenerateEnvironment: bake concluído (ambiente {0}x{1}, irradiância {2}x{3}, pré-filtro {4}x{4} com {5} mips)",
                 kEnvironmentSize, kEnvironmentSize, kIrradianceSize, kIrradianceSize, kPrefilterBaseSize, kPrefilterMipLevels);
}

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

void Renderer3D::SubmitParticles(const std::vector<ParticleInstance>& instances, bool additive) {
    if (instances.empty()) return;
    if (instances.size() <= kMaxParticlesPerBatch) {
        s_ParticleBatches.push_back({ instances, additive });
    } else {
        s_ParticleBatches.push_back({ std::vector<ParticleInstance>(instances.begin(), instances.begin() + kMaxParticlesPerBatch), additive });
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
    EnsurePostBuffers(targetW, targetH);

    // --- Passe 0: profundidade vista da luz, uma vez por cascata (só se há shadow caster) ---
    if (!s_DrawList.empty() && s_HasShadowCaster) {
        ComputeCascades(s_ShadowCaster.Direction);
        glCullFace(GL_FRONT); // reduz acne de sombra sem precisar de bias grande
        s_ShadowShader->Bind();
        for (int c = 0; c < kCascadeCount; ++c) {
            glBindFramebuffer(GL_FRAMEBUFFER, s_ShadowFBO[c]);
            glViewport(0, 0, (GLsizei)kShadowMapSize, (GLsizei)kShadowMapSize);
            glClear(GL_DEPTH_BUFFER_BIT);
            s_ShadowShader->SetMat4("u_LightSpaceMatrix", s_LightSpaceMatrix[c]);
            for (auto& cmd : s_DrawList) {
                s_ShadowShader->SetMat4("u_Model", cmd.Transform);
                RenderCommand::DrawIndexed(cmd.MeshAsset->GetVertexArray(), cmd.MeshAsset->GetIndexCount());
            }
        }
        glCullFace(GL_BACK);
    }

    // --- Passe 1: cena inteira (mesh + skybox), pro framebuffer HDR interno, não pro destino final ---
    glBindFramebuffer(GL_FRAMEBUFFER, s_HDRFBO);
    glViewport(0, 0, (GLsizei)targetW, (GLsizei)targetH);
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
            glBindBuffer(GL_ARRAY_BUFFER, s_ParticleInstanceVBO);
            glBufferSubData(GL_ARRAY_BUFFER, 0, (GLsizeiptr)(batch.Instances.size() * sizeof(ParticleInstance)), batch.Instances.data());
            glDrawElementsInstanced(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr, (GLsizei)batch.Instances.size());
        }
        glBindVertexArray(0);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA); // volta pro padrão
        RenderCommand::SetBlending(false);
        glDepthMask(GL_TRUE);
    }

    // --- Passe 2: bright-pass (meia resolução) — extrai só o que vai virar glow ---
    uint32_t bloomW = glm::max(1u, targetW / 2), bloomH = glm::max(1u, targetH / 2);
    RenderCommand::SetDepthTest(false);
    glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[0]);
    glViewport(0, 0, (GLsizei)bloomW, (GLsizei)bloomH);
    s_BrightPassShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
    s_BrightPassShader->SetInt("u_SceneColor", 0);
    s_BrightPassShader->SetFloat("u_Threshold", 1.2f);
    RenderCommand::DrawIndexed(s_FullscreenQuad, 6);

    // --- Passe 3: blur gaussiano separável, ping-pong entre os dois FBOs de bloom ---
    int readIdx = 0;
    bool horizontal = true;
    s_BlurShader->Bind();
    for (uint32_t i = 0; i < kBloomBlurIterations * 2; ++i) {
        int writeIdx = 1 - readIdx;
        glBindFramebuffer(GL_FRAMEBUFFER, s_BloomFBO[writeIdx]);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, s_BloomColorBuffer[readIdx]);
        s_BlurShader->SetInt("u_Image", 0);
        s_BlurShader->SetInt("u_Horizontal", horizontal ? 1 : 0);
        RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
        readIdx = writeIdx;
        horizontal = !horizontal;
    }

    // --- Passe 4: composição final (HDR + bloom, tonemap ACES) direto no destino do chamador ---
    glBindFramebuffer(GL_FRAMEBUFFER, (GLuint)prevFBO);
    glViewport(prevViewport[0], prevViewport[1], (GLsizei)prevViewport[2], (GLsizei)prevViewport[3]);
    s_CompositeShader->Bind();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, s_HDRColorBuffer);
    s_CompositeShader->SetInt("u_SceneColor", 0);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, s_BloomColorBuffer[readIdx]);
    s_CompositeShader->SetInt("u_BloomBlur", 1);
    s_CompositeShader->SetFloat("u_BloomIntensity", 0.45f);
    RenderCommand::DrawIndexed(s_FullscreenQuad, 6);
    RenderCommand::SetDepthTest(true);

    s_DrawList.clear();
}

void Renderer3D::DrawGrid() {
    s_DrawGridFlag = true;
}

void Renderer3D::Submit(const Ref<Mesh>& mesh, const Material& material, const glm::mat4& transform) {
    s_DrawList.push_back({ mesh, material, transform });
}

} // namespace kizuri
