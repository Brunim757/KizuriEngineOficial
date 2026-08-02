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
    int EntityID;
};

struct Renderer2DData {
    static const uint32_t MaxQuads = 20000;
    static const uint32_t MaxVertices = MaxQuads * 4;
    static const uint32_t MaxIndices = MaxQuads * 6;
    static const uint32_t MaxTextureSlots = 32;

    Ref<VertexArray> QuadVertexArray;
    Ref<VertexBuffer> QuadVertexBuffer;
    Ref<Shader> QuadShader;
    Ref<Texture2D> WhiteTexture;

    uint32_t QuadIndexCount = 0;
    std::vector<QuadVertex> QuadVertexBufferBase;
    uint32_t QuadVertexBufferPtr = 0;

    std::array<Ref<Texture2D>, MaxTextureSlots> TextureSlots;
    uint32_t TextureSlotIndex = 1; // 0 = white texture

    glm::vec4 QuadVertexPositions[4];

    glm::mat4 ViewProjection{ 1.0f };

    Ref<VertexArray> GridVertexArray;
    Ref<Shader> GridShader;
    uint32_t GridVertexCount = 0;

    Renderer2DStats Stats;
};

static Renderer2DData s_Data;

// Únicos shaders da engine que realmente PRECISAM de GLSL 450 (todos os
// outros — Renderer2D_Line, Renderer3D_Mesh/Line/Shadow — rodam em 330
// core, o mínimo que a Window garante, ver Window::Init). O motivo é
// s_QuadFragmentSrc logo abaixo: `u_Textures[index]` indexa o array de
// sampler2D com um índice que só é conhecido em tempo de execução (vem de
// v_TexIndex, por-vértice). GLSL 1.30–3.30 exige que índice de array de
// sampler seja uma expressão CONSTANTE em tempo de compilação — só a
// partir do GLSL 4.00 (dynamically uniform expressions) isso é permitido.
// É o preço do batching multi-textura num draw call só; a alternativa
// portável (cadeia de if/else com índice constante por textura) sacrifica
// a elegância do código pra rodar em 3.3 — não vale a troca ainda porque
// nenhuma GPU/driver testado até agora rejeitou isso (inclusive o Zink/
// Vortek do log que motivou essa revisão: compilou os cinco shaders sem
// erro mesmo com o contexto tendo caído pra 3.3). Se isso virar dor real
// num driver mais rígido, a solução é reescrever só este par de shaders.
static const char* s_QuadVertexSrc = R"(
#version 450 core
layout(location = 0) in vec3 a_Position;
layout(location = 1) in vec4 a_Color;
layout(location = 2) in vec2 a_TexCoord;
layout(location = 3) in float a_TexIndex;
layout(location = 4) in float a_TilingFactor;

uniform mat4 u_ViewProjection;

out vec4 v_Color;
out vec2 v_TexCoord;
out float v_TexIndex;
out float v_TilingFactor;

void main() {
    v_Color = a_Color;
    v_TexCoord = a_TexCoord;
    v_TexIndex = a_TexIndex;
    v_TilingFactor = a_TilingFactor;
    gl_Position = u_ViewProjection * vec4(a_Position, 1.0);
}
)";

static const char* s_QuadFragmentSrc = R"(
#version 450 core
layout(location = 0) out vec4 o_Color;

in vec4 v_Color;
in vec2 v_TexCoord;
in float v_TexIndex;
in float v_TilingFactor;

uniform sampler2D u_Textures[32];

void main() {
    vec4 texColor = v_Color;
    int index = int(v_TexIndex);
    texColor *= texture(u_Textures[index], v_TexCoord * v_TilingFactor);
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

    int samplers[Renderer2DData::MaxTextureSlots];
    for (uint32_t i = 0; i < Renderer2DData::MaxTextureSlots; ++i) samplers[i] = (int)i;

    s_Data.QuadShader = CreateRef<Shader>("Renderer2D_Quad", s_QuadVertexSrc, s_QuadFragmentSrc);
    s_Data.QuadShader->Bind();
    s_Data.QuadShader->SetIntArray("u_Textures", samplers, Renderer2DData::MaxTextureSlots);

    s_Data.TextureSlots[0] = s_Data.WhiteTexture;

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
    s_Data.TextureSlotIndex = 1;
}

void Renderer2D::NextBatch() { Flush(); StartBatch(); }

void Renderer2D::EndScene() { KZ_TRACE_SCOPE("Renderer2D::EndScene"); Flush(); }

void Renderer2D::Flush() {
    KZ_TRACE_SCOPE("Renderer2D::Flush");
    if (s_Data.QuadIndexCount == 0) return;

    uint32_t dataSize = s_Data.QuadVertexBufferPtr * sizeof(QuadVertex);
    s_Data.QuadVertexBuffer->SetData(s_Data.QuadVertexBufferBase.data(), dataSize);

    for (uint32_t i = 0; i < s_Data.TextureSlotIndex; ++i)
        s_Data.TextureSlots[i]->Bind(i);

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
        v.EntityID = -1;
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
    static const glm::vec2 texCoords[4] = { {0,0}, {1,0}, {1,1}, {0,1} };

    float textureIndex = 0.0f;
    for (uint32_t i = 1; i < s_Data.TextureSlotIndex; ++i) {
        if (*s_Data.TextureSlots[i] == *texture) { textureIndex = (float)i; break; }
    }
    if (textureIndex == 0.0f) {
        if (s_Data.TextureSlotIndex >= Renderer2DData::MaxTextureSlots) NextBatch();
        textureIndex = (float)s_Data.TextureSlotIndex;
        s_Data.TextureSlots[s_Data.TextureSlotIndex] = texture;
        ++s_Data.TextureSlotIndex;
    }
    PushQuadVertices(transform, tint, texCoords, textureIndex, tiling);
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

} // namespace kizuri
