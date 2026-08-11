#include "kizuri/renderer/Renderer2D.hpp"
#include "kizuri/renderer/Buffer.hpp"
#include "kizuri/renderer/RenderCommand.hpp"
#include "kizuri/renderer/Shader.hpp"
#include "kizuri/core/Log.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <array>

namespace kizuri {

struct QuadVertex {
    glm::vec3 Position;
    glm::vec4 Color;
    glm::vec2 TexCoord;
    float TexIndex;
    float TilingFactor;
};

// Vértice do pipeline de círculos — posição local (quad unitário) + atributos
// de desenho; o fragment shader calcula o SDF do disco/anel.
struct CircleVertex {
    glm::vec3 WorldPosition;
    glm::vec3 LocalPosition;
    glm::vec4 Color;
    float Thickness;
    float Fade;
};

struct Renderer2DData {
    static const uint32_t MaxQuads = 20000;
    static const uint32_t MaxVertices = MaxQuads * 4;
    static const uint32_t MaxIndices = MaxQuads * 6;

    static const uint32_t MaxCircles = 4000;
    static const uint32_t MaxCircleVertices = MaxCircles * 4;
    static const uint32_t MaxCircleIndices = MaxCircles * 6;

    Ref<VertexArray> QuadVertexArray;
    Ref<VertexBuffer> QuadVertexBuffer;
    Ref<Shader> QuadShader;
    Ref<Texture2D> WhiteTexture;

    // Textura ativa do batch atual — quads são agrupados por textura e o
    // batch é despejado (Flush) quando ela muda. É o que mantém o shader em
    // GLSL 330 (sem indexação dinâmica de sampler array).
    Ref<Texture2D> CurrentTexture;

    uint32_t QuadIndexCount = 0;
    std::vector<QuadVertex> QuadVertexBufferBase;
    uint32_t QuadVertexBufferPtr = 0;

    glm::vec4 QuadVertexPositions[4];

    glm::mat4 ViewProjection{ 1.0f };

    // Pipeline de círculos — buffer/VAO próprios, flush separado do de quads.
    Ref<VertexArray> CircleVertexArray;
    Ref<VertexBuffer> CircleVertexBuffer;
    Ref<Shader> CircleShader;
    uint32_t CircleIndexCount = 0;
    std::vector<CircleVertex> CircleVertexBufferBase;
    uint32_t CircleVertexBufferPtr = 0;

    Ref<VertexArray> GridVertexArray;
    Ref<Shader> GridShader;
    uint32_t GridVertexCount = 0;

    Renderer2DStats Stats;
};

static Renderer2DData s_Data;

// O batching de quads roda 100% em GLSL 330 core (o mínimo que a engine
// garante, ver Window::Init) — inclusive o shader de quads. O preço da
// portabilidade: GLSL 1.30–3.30 não permite indexar array de sampler com
// índice dinâmico (`u_Textures[index]` com índice por-vértice só existe a
// partir do 4.00), então em vez de um draw call multi-textura a engine
// agrupa os quads POR textura e faz um draw call por textura do lote —
// o mesmo resultado visual com zero dependência de driver novo.
static const char* s_QuadVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
out float v_TilingFactor;

void main() {
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    v_TilingFactor = a_TilingFactor;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

static const char* s_QuadFragmentSrc = R"(
#version 330 core
layout(location = 0) out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TilingFactor;

uniform sampler2D u_Texture;

void main() {
    vec4 texColor = v_Color;
    texColor *= texture(u_Texture, v_TexCoord * v_TilingFactor);
    if (texColor.a < 0.01) discard;
    o_Color = texColor;
}
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

// Círculo 2D: desenha um quad expandido e calcula o SDF no fragment shader
// (distância ao centro em espaço local) — disco suave ou anel fino, sem
// precisar de textura nem geometria curva na CPU.
static const char* s_CircleVertexSrc = R"(
#version 330 core
layout(location = 0) in vec3 a_WorldPosition;
layout(location = 1) in vec3 a_LocalPosition;
layout(location = 2) in vec4 a_Color;
layout(location = 3) in float a_Thickness;
layout(location = 4) in float a_Fade;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec3 v_LocalPosition;
out float v_Thickness;
out float v_Fade;

void main() {
    v_Color = a_Color;
    v_LocalPosition = a_LocalPosition;
    v_Thickness = a_Thickness;
    v_Fade = a_Fade;
    gl_Position = u_ViewProjection * vec4(a_WorldPosition, 1.0);
}
)";

static const char* s_CircleFragmentSrc = R"(
#version 330 core
layout(location = 0) out vec4 o_Color;

in vec4 v_Color;
in vec3 v_LocalPosition;
in float v_Thickness;
in float v_Fade;

void main() {
    float dist = 1.0 - length(v_LocalPosition);
    float circle = smoothstep(0.0, v_Fade, dist);
    circle *= smoothstep(v_Thickness + v_Fade, v_Thickness, dist);
    if (circle == 0.0) discard;
    o_Color = vec4(v_Color.rgb, v_Color.a * circle);
}
)";

void Renderer2D::Init() {
    KZ_TRACE_SCOPE("Renderer2D::Init");
    s_Data.QuadVertexArray = CreateRef<VertexArray>();
    s_Data.QuadVertexBufferBase.resize(Renderer2DData::MaxVertices);

    s_Data.QuadVertexBuffer = CreateRef<VertexBuffer>(Renderer2DData::MaxVertices * sizeof(QuadVertex));
    s_Data.QuadVertexBuffer->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float4, "a_Color" },
        { ShaderDataType::Float2, "a_TexCoord" },
        { ShaderDataType::Float,  "a_TexIndex" },
        { ShaderDataType::Float,  "a_TilingFactor" },
    });
    s_Data.QuadVertexArray->AddVertexBuffer(s_Data.QuadVertexBuffer);

    std::vector<uint32_t> quadIndices(Renderer2DData::MaxIndices);
    uint32_t offset = 0;
    for (uint32_t i = 0; i < Renderer2DData::MaxIndices; i += 6) {
        quadIndices[i + 0] = offset + 0;
        quadIndices[i + 1] = offset + 1;
        quadIndices[i + 2] = offset + 2;
        quadIndices[i + 3] = offset + 2;
        quadIndices[i + 4] = offset + 3;
        quadIndices[i + 5] = offset + 0;
        offset += 4;
    }
    auto quadIB = CreateRef<IndexBuffer>(quadIndices.data(), Renderer2DData::MaxIndices);
    s_Data.QuadVertexArray->SetIndexBuffer(quadIB);

    s_Data.WhiteTexture = Texture2D::Create(1, 1);
    uint32_t whitePixel = 0xffffffff;
    s_Data.WhiteTexture->SetData(&whitePixel, sizeof(uint32_t));

    s_Data.QuadShader = CreateRef<Shader>("Renderer2D_Quad", s_QuadVertexSrc, s_QuadFragmentSrc);

    s_Data.CurrentTexture = s_Data.WhiteTexture;

    s_Data.QuadVertexPositions[0] = { -0.5f, -0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[1] = {  0.5f, -0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[2] = {  0.5f,  0.5f, 0.0f, 1.0f };
    s_Data.QuadVertexPositions[3] = { -0.5f,  0.5f, 0.0f, 1.0f };

    // Grid de referência no plano XY — equivalente 2D do grid de chão do
    // Renderer3D, mesma ideia (fixo, construído uma vez, eixo destacado
    // pra dar noção de orientação além de escala).
    s_Data.GridShader = CreateRef<Shader>("Renderer2D_Line", s_LineVertexSrc, s_LineFragmentSrc);

    const float halfSize = 50.0f;
    const glm::vec3 gridColor(0.32f, 0.32f, 0.35f);
    const glm::vec3 xAxisColor(0.75f, 0.25f, 0.25f);
    const glm::vec3 yAxisColor(0.25f, 0.6f, 0.35f);

    struct LineVertex { glm::vec3 Position; glm::vec3 Color; };
    std::vector<LineVertex> gridVerts;
    gridVerts.reserve((size_t)(halfSize * 2 + 1) * 4);

    for (int i = -(int)halfSize; i <= (int)halfSize; ++i) {
        float x = (float)i;
        glm::vec3 c = (i == 0) ? yAxisColor : gridColor; // x=0 é o eixo Y
        gridVerts.push_back({ { x, -halfSize, 0.0f }, c });
        gridVerts.push_back({ { x,  halfSize, 0.0f }, c });
    }
    for (int i = -(int)halfSize; i <= (int)halfSize; ++i) {
        float y = (float)i;
        glm::vec3 c = (i == 0) ? xAxisColor : gridColor; // y=0 é o eixo X
        gridVerts.push_back({ { -halfSize, y, 0.0f }, c });
        gridVerts.push_back({ {  halfSize, y, 0.0f }, c });
    }

    s_Data.GridVertexCount = (uint32_t)gridVerts.size();
    s_Data.GridVertexArray = CreateRef<VertexArray>();
    auto gridVB = CreateRef<VertexBuffer>((float*)gridVerts.data(), (uint32_t)(gridVerts.size() * sizeof(LineVertex)));
    gridVB->SetLayout({
        { ShaderDataType::Float3, "a_Position" },
        { ShaderDataType::Float3, "a_Color" },
    });
    s_Data.GridVertexArray->AddVertexBuffer(gridVB);

    // ---- Pipeline de círculos ----
    s_Data.CircleVertexArray = CreateRef<VertexArray>();
    s_Data.CircleVertexBufferBase.resize(Renderer2DData::MaxCircleVertices);

    s_Data.CircleVertexBuffer = CreateRef<VertexBuffer>(Renderer2DData::MaxCircleVertices * sizeof(CircleVertex));
    s_Data.CircleVertexBuffer->SetLayout({
        { ShaderDataType::Float3, "a_WorldPosition" },
        { ShaderDataType::Float3, "a_LocalPosition" },
        { ShaderDataType::Float4, "a_Color" },
        { ShaderDataType::Float,  "a_Thickness" },
        { ShaderDataType::Float,  "a_Fade" },
    });
    s_Data.CircleVertexArray->AddVertexBuffer(s_Data.CircleVertexBuffer);

    std::vector<uint32_t> circleIndices(Renderer2DData::MaxCircleIndices);
    uint32_t circleOffset = 0;
    for (uint32_t i = 0; i < Renderer2DData::MaxCircleIndices; i += 6) {
        circleIndices[i + 0] = circleOffset + 0;
        circleIndices[i + 1] = circleOffset + 1;
        circleIndices[i + 2] = circleOffset + 2;
        circleIndices[i + 3] = circleOffset + 2;
        circleIndices[i + 4] = circleOffset + 3;
        circleIndices[i + 5] = circleOffset + 0;
        circleOffset += 4;
    }
    auto circleIB = CreateRef<IndexBuffer>(circleIndices.data(), Renderer2DData::MaxCircleIndices);
    s_Data.CircleVertexArray->SetIndexBuffer(circleIB);

    s_Data.CircleShader = CreateRef<Shader>("Renderer2D_Circle", s_CircleVertexSrc, s_CircleFragmentSrc);
}

void Renderer2D::Shutdown() {}

void Renderer2D::BeginScene(const OrthographicCamera& camera) { BeginScene(camera.GetViewProjectionMatrix()); }

void Renderer2D::BeginScene(const glm::mat4& viewProjection) {
    KZ_TRACE_SCOPE("Renderer2D::BeginScene");
    s_Data.ViewProjection = viewProjection;
    s_Data.QuadShader->Bind();
    s_Data.QuadShader->SetMat4("u_ViewProjection", viewProjection);
    StartBatch();
}

void Renderer2D::DrawGrid() {
    KZ_TRACE_SCOPE("Renderer2D::DrawGrid");
    // Desenha fora do batch de quads (shader e VAO diferentes) — não
    // interfere no StartBatch/Flush do resto da cena.
    s_Data.GridShader->Bind();
    s_Data.GridShader->SetMat4("u_ViewProjection", s_Data.ViewProjection);
    RenderCommand::DrawLines(s_Data.GridVertexArray, s_Data.GridVertexCount);
}

void Renderer2D::StartBatch() {
    s_Data.QuadIndexCount = 0;
    s_Data.QuadVertexBufferPtr = 0;
    s_Data.CurrentTexture = s_Data.WhiteTexture;
    s_Data.CircleIndexCount = 0;
    s_Data.CircleVertexBufferPtr = 0;
}

void Renderer2D::NextBatch() { Flush(); StartBatch(); }

void Renderer2D::EndScene() {
    KZ_TRACE_SCOPE("Renderer2D::EndScene");
    Flush();
    if (s_Data.CircleIndexCount == 0) return;

    uint32_t dataSize = s_Data.CircleVertexBufferPtr * sizeof(CircleVertex);
    s_Data.CircleVertexBuffer->SetData(s_Data.CircleVertexBufferBase.data(), dataSize);

    s_Data.CircleShader->Bind();
    s_Data.CircleShader->SetMat4("u_ViewProjection", s_Data.ViewProjection);
    RenderCommand::DrawIndexed(s_Data.CircleVertexArray, s_Data.CircleIndexCount);
    ++s_Data.Stats.DrawCalls;
}

void Renderer2D::Flush() {
    KZ_TRACE_SCOPE("Renderer2D::Flush");
    if (s_Data.QuadIndexCount == 0) return;

    uint32_t dataSize = s_Data.QuadVertexBufferPtr * sizeof(QuadVertex);
    s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase.data(), dataSize);

    // Textura única do batch — GLSL 330 não permite sampler array dinâmico,
    // então cada batch carrega só a textura atual na unidade 0.
    if (s_Data.CurrentTexture) {
        s_Data.CurrentTexture->Bind(0);
        s_Data.QuadShader->SetInt("u_Texture", 0);
    }

    RenderCommand::DrawIndexed(s_Data.QuadVertexArray, s_Data.QuadIndexCount);
    ++s_Data.Stats.DrawCalls;
}

static void PushQuadVertices(const glm::mat4& transform, const glm::vec4& color, const glm::vec2 texCoords[4], float texIndex, float tiling) {
    if (s_Data.QuadIndexCount >= Renderer2DData::MaxIndices) Renderer2D::Flush();

    for (int i = 0; i < 4; ++i) {
        QuadVertex& v = s_Data.QuadVertexBufferBase[s_Data.QuadVertexBufferPtr];
        v.Position = glm::vec3(transform * s_Data.QuadVertexPositions[i]);
        v.Color = color;
        v.TexCoord = texCoords[i];
        v.TexIndex = texIndex;
        v.TilingFactor = tiling;
        ++s_Data.QuadVertexBufferPtr;
    }
    s_Data.QuadIndexCount += 6;
    ++s_Data.Stats.QuadCount;
}

void Renderer2D::DrawTransformedQuad(const glm::mat4& transform, const glm::vec4& color, int) {
    static const glm::vec2 texCoords[4] = { {0,0}, {1,0}, {1,1}, {0,1} };
    PushQuadVertices(transform, color, texCoords, 0.0f, 1.0f);
}

void Renderer2D::DrawTransformedQuad(const glm::mat4& transform, const Ref<Texture2D>& texture, float tiling, const glm::vec4& tint, int) {
    DrawTransformedQuadUV(transform, texture, { 0.0f, 0.0f }, { 1.0f, 1.0f }, tint, tiling);
}

void Renderer2D::DrawTransformedQuadUV(const glm::mat4& transform, const Ref<Texture2D>& texture,
                                        const glm::vec2& uvMin, const glm::vec2& uvMax,
                                        const glm::vec4& tint, float tilingFactor, int) {
    const glm::vec2 texCoords[4] = {
        { uvMin.x, uvMin.y }, { uvMax.x, uvMin.y }, { uvMax.x, uvMax.y }, { uvMin.x, uvMax.y },
    };

    // Troca de textura = fim do batch atual; o próximo quad começa um lote
    // novo com a nova textura (flush no meio do frame é OK — o buffer de
    // vértice continua acumulando, só despeja o que já estava lá).
    if (*s_Data.CurrentTexture != *texture) {
        Renderer2D::Flush();
        s_Data.CurrentTexture = texture;
    }
    PushQuadVertices(transform, tint, texCoords, 0.0f, tilingFactor);
}

void Renderer2D::DrawCircle(const glm::mat4& transform, const glm::vec4& color, float thickness, float fade, int) {
    if (s_Data.CircleIndexCount >= Renderer2DData::MaxCircleIndices) {
        // Lote cheio: despeja o que tem e recomeça — mesmo padrão dos quads.
        EndScene();
        StartBatch();
    }
    for (int i = 0; i < 4; ++i) {
        CircleVertex& v = s_Data.CircleVertexBufferBase[s_Data.CircleVertexBufferPtr];
        v.WorldPosition = glm::vec3(transform * s_Data.QuadVertexPositions[i]);
        v.LocalPosition = glm::vec3(s_Data.QuadVertexPositions[i]) * 2.0f;
        v.Color = color;
        v.Thickness = thickness;
        v.Fade = fade;
        ++s_Data.CircleVertexBufferPtr;
    }
    s_Data.CircleIndexCount += 6;
    ++s_Data.Stats.QuadCount;
}

void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const glm::vec4& color) { DrawQuad({ pos.x, pos.y, 0.0f }, size, color); }

void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size, const glm::vec4& color) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
    DrawTransformedQuad(transform, color);
}

void Renderer2D::DrawQuad(const glm::vec2& pos, const glm::vec2& size, const Ref<Texture2D>& texture, float tiling, const glm::vec4& tint) {
    DrawQuad({ pos.x, pos.y, 0.0f }, size, texture, tiling, tint);
}

void Renderer2D::DrawQuad(const glm::vec3& pos, const glm::vec2& size, const Ref<Texture2D>& texture, float tiling, const glm::vec4& tint) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) * glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
    DrawTransformedQuad(transform, texture, tiling, tint);
}

void Renderer2D::DrawRotatedQuad(const glm::vec3& pos, const glm::vec2& size, float rotationDeg, const glm::vec4& color) {
    glm::mat4 transform = glm::translate(glm::mat4(1.0f), pos) *
                           glm::rotate(glm::mat4(1.0f), glm::radians(rotationDeg), { 0, 0, 1 }) *
                           glm::scale(glm::mat4(1.0f), { size.x, size.y, 1.0f });
    DrawTransformedQuad(transform, color);
}

void Renderer2D::ResetStats() { s_Data.Stats = {}; }
Renderer2DStats Renderer2D::GetStats() { return s_Data.Stats; }
glm::mat4 Renderer2D::GetViewProjection() { return s_Data.ViewProjection; }

} // namespace kizuri
