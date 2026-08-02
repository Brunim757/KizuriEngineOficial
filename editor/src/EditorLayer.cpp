// IMGUI_DEFINE_MATH_OPERATORS precisa estar definido ANTES da primeira vez
// que imgui.h é incluído neste arquivo (o header guard do imgui.h impede
// que a macro tenha efeito numa inclusão posterior) — e EditorLayer.hpp já
// inclui imgui.h de forma transitiva via Kizuri.hpp -> ImGuiLayer.hpp, por
// isso o #define precisa vir antes até desse include.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "EditorLayer.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "UI/Icons.hpp"
#include <fstream>
#include <cfloat>
#include <cctype>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cstring>

using namespace kizuri;

// Decompõe uma matriz de transformação em translação/rotação(euler,
// radianos)/escala. Usado pra converter o resultado do ImGuizmo::Manipulate
// (uma mat4 só) de volta pros três campos separados de TransformComponent.
static bool DecomposeTransform(const glm::mat4& transform, glm::vec3& outTranslation, glm::vec3& outRotation, glm::vec3& outScale) {
    glm::vec3 skew;
    glm::vec4 perspective;
    glm::quat rotation;
    if (!glm::decompose(transform, outScale, rotation, outTranslation, skew, perspective))
        return false;
    outRotation = glm::eulerAngles(rotation);
    return true;
}

// Garante que nenhum painel mostre o botão de menu (triângulo) que o
// próprio ImGui desenha no canto de um nó de dock — configurar a flag só
// no DockSpace()/DockBuilderAddNode() não bastou pra sumir de fato em
// todo painel, então força aqui, por janela, via ImGuiWindowClass. Chamar
// antes de CADA ImGui::Begin() de painel (Hierarquia, Inspetor, Viewport,
// Console, Content Browser) — não afeta a janela-hospedeira do dockspace.
static void BeginPanelNoMenuButton() {
    ImGuiWindowClass windowClass;
    windowClass.DockNodeFlagsOverrideSet |= ImGuiDockNodeFlags_NoWindowMenuButton;
    ImGui::SetNextWindowClass(&windowClass);
}

EditorLayer::EditorLayer() : Layer("EditorLayer") {}

void EditorLayer::OnAttach() {
    KZ_TRACE_SCOPE("EditorLayer::OnAttach");

    // PRECISA ser a primeira coisa aqui, antes de qualquer ImGui::.., em
    // TODO o resto do EditorLayer/Icons.cpp. Ver ImGuiLayer::GetContext()
    // pra explicação completa: em build SHARED o KizuriEditor.exe compila
    // sua própria cópia do ImGui (via ImGuizmo), com seu próprio GImGui
    // separado do que a KizuriEngine.dll usa — sem isso, a primeira
    // chamada ImGui:: do editor crasha na hora (contexto nulo).
    ImGui::SetCurrentContext(kizuri::ImGuiLayer::GetContext());

    FramebufferSpec fbSpec;
    fbSpec.Width = 1280;
    fbSpec.Height = 720;
    m_Framebuffer = Framebuffer::Create(fbSpec);

    m_ActiveScene = CreateRef<Scene>("Nova Cena");
    CreateDefaultSceneContent();
}

void EditorLayer::SyncEditorCameraToRuntimeScene() {
    KZ_TRACE_SCOPE("EditorLayer::SyncEditorCameraToRuntimeScene");
    // RenderScene3D/RenderScene2D do Play usam a CameraComponent da cena, não
    // a câmera livre do editor. Enquanto a câmera da cena não foi "autorada"
    // (cena nova recém-criada), copia a pose da câmera de edição pra entidade
    // primária da cópia runtime — o Play começa de onde a câmera do editor
    // estava. Se o usuário editou a câmera da cena (gizmo/inspetor) ou abriu
    // uma cena salva, respeita a pose dela como está e não sobrescreve nada.
    if (!m_UseEditorCameraOnPlay) return;
    auto view = m_ActiveScene->GetRegistry().view<TransformComponent, CameraComponent>();
    for (auto e : view) {
        auto& cc = view.get<CameraComponent>(e);
        if (!cc.Primary) continue;
        auto& tc = view.get<TransformComponent>(e);

        if (m_ViewportMode == ViewportMode::Mode3D && cc.Type == CameraComponent::ProjectionType::Perspective3D) {
            tc.Translation = m_EditorCamPos;
            tc.Rotation = { glm::radians(m_EditorCamPitch), glm::radians(m_EditorCamYaw), 0.0f };
            break;
        } else if (m_ViewportMode == ViewportMode::Mode2D && cc.Type == CameraComponent::ProjectionType::Orthographic2D) {
            tc.Translation = { m_Editor2DCamPos.x, m_Editor2DCamPos.y, 0.0f };
            cc.OrthoSize = m_Editor2DZoom;
            break;
        }
        // Câmera primária com tipo diferente do modo do viewport: ignora e
        // procura a próxima (ex: câmera 2D primária enquanto o viewport está em 3D).
    }
}

void EditorLayer::AutoSwitchViewportMode() {
    // Seleção mudou (essa função é chamada em toda troca de seleção) — aborta
    // o gesto de pintura em andamento, senão o snapshot "antes" ficaria
    // preso da entidade anterior e o próximo undo seria de outro objeto.
    m_TilePainting = false;
    if (m_SceneState != SceneState::Edit || !m_SelectedEntity) return;

    if (m_SelectedEntity.HasComponent<MeshRendererComponent>() || m_SelectedEntity.HasComponent<LightComponent>()) {
        m_ViewportMode = ViewportMode::Mode3D;
        return;
    }
    if (m_SelectedEntity.HasComponent<CameraComponent>()) {
        auto& cc = m_SelectedEntity.GetComponent<CameraComponent>();
        m_ViewportMode = (cc.Type == CameraComponent::ProjectionType::Perspective3D)
            ? ViewportMode::Mode3D : ViewportMode::Mode2D;
        return;
    }
    if (m_SelectedEntity.HasComponent<SpriteRendererComponent>() ||
        m_SelectedEntity.HasComponent<CircleRendererComponent>() ||
        m_SelectedEntity.HasComponent<TextComponent>() ||
        m_SelectedEntity.HasComponent<TilemapComponent>() ||
        m_SelectedEntity.HasComponent<SpriteAnimationComponent>()) {
        m_ViewportMode = ViewportMode::Mode2D;
        return;
    }
}

void EditorLayer::CreateDefaultSceneContent() {
    KZ_TRACE_SCOPE("EditorLayer::CreateDefaultSceneContent");
    Entity camera = m_ActiveScene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Primary = true;
    // Perspective3D explícito: o default do componente é Orthographic2D (pensado pra HUD/UI),
    // e Play usa a Camera da PRÓPRIA cena — sem isso, o cubo 3D abaixo nunca aparecia no Play,
    // só o quad 2D (ou nem isso, se ele também não estivesse enquadrado). Posição/rotação
    // também setadas: o default de TransformComponent é a origem sem rotação, mesmo ponto do
    // cubo — a câmera nascia "dentro" dele, olhando pra lugar nenhum.
    cc.Type = CameraComponent::ProjectionType::Perspective3D;
    auto& camTransform = camera.GetComponent<TransformComponent>();
    camTransform.Translation = { 0.0f, 2.0f, 6.0f };
    camTransform.Rotation = { glm::radians(-15.0f), glm::radians(-90.0f), 0.0f }; // pitch, yaw

    Entity quad = m_ActiveScene->CreateEntity("Quad de Exemplo");
    auto& sprite = quad.AddComponent<SpriteRendererComponent>();
    sprite.Color = { 0.85f, 0.25f, 0.3f, 1.0f };

    // A engine é híbrida 2D/3D por design — o quad acima mostra o lado 2D
    // (visível pela câmera ortográfica da própria cena), esse cubo mostra
    // o lado 3D (visível pela câmera livre do editor). Sem ele, olhar pro
    // viewport em modo 3D era só o grid e o vazio.
    Entity cube = m_ActiveScene->CreateEntity("Cubo de Exemplo");
    auto& mr = cube.AddComponent<MeshRendererComponent>();
    mr.MeshAsset = Mesh::CreateCube();
    mr.MeshMaterial.Albedo = { 0.3f, 0.55f, 0.85f };

    Entity sun = m_ActiveScene->CreateEntity("Sol");
    sun.AddComponent<LightComponent>().Type = LightType::Directional;

    m_SelectedEntity = quad;
}

void EditorLayer::OnDetach() {}

void EditorLayer::OnUpdate(Timestep ts) {
    KZ_CORE_TRACE("EditorLayer::OnUpdate — início (viewport {0}x{1})", m_ViewportSize.x, m_ViewportSize.y);
    if (m_ViewportSize.x > 0.0f && m_ViewportSize.y > 0.0f) {
        m_Framebuffer->Resize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
        m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);

        float aspect = m_ViewportSize.y > 0.0f ? m_ViewportSize.x / m_ViewportSize.y : 16.0f / 9.0f;
        m_EditorCamera.SetPerspective(45.0f, aspect, 0.01f, 1000.0f);
        m_Editor2DCamera.SetProjection(-m_Editor2DZoom * aspect, m_Editor2DZoom * aspect, -m_Editor2DZoom, m_Editor2DZoom);
    }

    m_Framebuffer->Bind();
    KZ_CORE_TRACE("EditorLayer::OnUpdate — framebuffer vinculado");
    RenderCommand::SetClearColor({ 0.06f, 0.06f, 0.07f, 1.0f });
    RenderCommand::Clear();

    if (m_SceneState == SceneState::Play) {
        m_ActiveScene->OnUpdateRuntime(ts);
    } else if (m_ViewportMode == ViewportMode::Mode3D) {
        UpdateEditorCamera(ts);
        KZ_CORE_TRACE("EditorLayer::OnUpdate — chamando OnUpdateEditor3D");
        m_ActiveScene->OnUpdateEditor3D(ts, m_EditorCamera);
        KZ_CORE_TRACE("EditorLayer::OnUpdate — OnUpdateEditor3D retornou");
    } else {
        UpdateEditor2DCamera(ts);
        m_ActiveScene->OnUpdateEditor2D(ts, m_Editor2DCamera);
    }

    m_Framebuffer->Unbind();
    KZ_CORE_TRACE("EditorLayer::OnUpdate — framebuffer desvinculado, fim");

    // Framebuffer::Bind() troca o glViewport para o tamanho do painel
    // Viewport; sem restaurar aqui, o ImGui (que roda depois, em espaço da
    // janela inteira) herdaria esse viewport errado e a UI apareceria
    // cortada/deslocada.
    KZ_CORE_TRACE("EditorLayer::OnUpdate — buscando Application::Get()");
    Application& app = Application::Get();
    KZ_CORE_TRACE("EditorLayer::OnUpdate — Application::Get() ok, buscando janela");
    auto& window = app.GetWindow();
    KZ_CORE_TRACE("EditorLayer::OnUpdate — janela obtida, lendo largura/altura");
    uint32_t w = window.GetWidth();
    uint32_t h = window.GetHeight();
    KZ_CORE_TRACE("EditorLayer::OnUpdate — {0}x{1}, chamando SetViewport", w, h);
    RenderCommand::SetViewport(0, 0, w, h);
    KZ_CORE_TRACE("EditorLayer::OnUpdate — SetViewport ok, retornando");
}

void EditorLayer::OnEvent(Event& e) {
    (void)e;
}

void EditorLayer::UpdateEditorCamera(Timestep ts) {
    KZ_TRACE_SCOPE("EditorLayer::UpdateEditorCamera");
    bool flying = m_ViewportHovered && Input::IsMouseButtonPressed(Mouse::Right);

    auto [mx, my] = Input::GetMousePosition();
    glm::vec2 mousePos{ mx, my };

    if (flying) {
        if (m_FirstMouseLook) {
            m_LastMousePos = mousePos;
            m_FirstMouseLook = false;
        }
        glm::vec2 delta = mousePos - m_LastMousePos;

        constexpr float kLookSensitivity = 0.12f;
        m_EditorCamYaw += delta.x * kLookSensitivity;
        m_EditorCamPitch -= delta.y * kLookSensitivity;
        m_EditorCamPitch = std::clamp(m_EditorCamPitch, -89.0f, 89.0f);

        // Mesma convenção de PerspectiveCamera::RecalculateViewMatrix
        // (Camera.cpp) — precisa bater pra WASD mover na direção que a
        // câmera está de fato olhando.
        glm::vec3 forward{
            cos(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch)),
            sin(glm::radians(m_EditorCamPitch)),
            sin(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch))
        };
        forward = glm::normalize(forward);
        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::cross(right, forward);

        float speed = (Input::IsKeyPressed(Key::LeftShift) ? 12.0f : 4.0f) * (float)ts;
        if (Input::IsKeyPressed(Key::W)) m_EditorCamPos += forward * speed;
        if (Input::IsKeyPressed(Key::S)) m_EditorCamPos -= forward * speed;
        if (Input::IsKeyPressed(Key::A)) m_EditorCamPos -= right * speed;
        if (Input::IsKeyPressed(Key::D)) m_EditorCamPos += right * speed;
        if (Input::IsKeyPressed(Key::E)) m_EditorCamPos += up * speed;
        if (Input::IsKeyPressed(Key::Q)) m_EditorCamPos -= up * speed;
    } else {
        // Solta o botão direito -> próxima vez que apertar não deve "pular"
        // usando o delta acumulado enquanto o mouse não estava sendo lido.
        m_FirstMouseLook = true;
    }

    m_LastMousePos = mousePos;

    m_EditorCamera.SetPosition(m_EditorCamPos);
    m_EditorCamera.SetRotation(m_EditorCamYaw, m_EditorCamPitch);
}

void EditorLayer::UpdateEditor2DCamera(Timestep ts) {
    KZ_TRACE_SCOPE("EditorLayer::UpdateEditor2DCamera");
    // Pan: segurar botão direito e arrastar. Zoom: scroll do mouse — só
    // quando o viewport está sob o cursor, senão rolar a página inteira
    // (ex: painel de Inspetor) zoomaria a câmera por engano.
    // Com um Tilemap selecionado, o botão direito vira a borracha do pintor
    // (ver OnImGuiRender) — então o pan é suprimido nesse caso, senão os
    // dois lutariam pelo mesmo gesto.
    bool erasingTilemap = m_SceneState == SceneState::Edit && m_ViewportMode == ViewportMode::Mode2D &&
        m_SelectedEntity && m_SelectedEntity.HasComponent<TilemapComponent>() &&
        Input::IsMouseButtonPressed(Mouse::Right);
    bool panning = m_ViewportHovered && Input::IsMouseButtonPressed(Mouse::Right) && !erasingTilemap;

    auto [mx, my] = Input::GetMousePosition();
    glm::vec2 mousePos{ mx, my };

    if (panning) {
        if (m_Editor2DFirstMouseLook) {
            m_Editor2DLastMousePos = mousePos;
            m_Editor2DFirstMouseLook = false;
        }
        glm::vec2 delta = mousePos - m_Editor2DLastMousePos;

        // Converte delta de pixels de tela pra unidades de mundo usando o
        // zoom atual, senão arrastar teria uma "velocidade" diferente
        // dependendo de quão perto/longe a câmera está.
        float worldPerPixel = (m_Editor2DZoom * 2.0f) / std::max(m_ViewportSize.y, 1.0f);
        m_Editor2DCamPos.x -= delta.x * worldPerPixel;
        m_Editor2DCamPos.y += delta.y * worldPerPixel; // Y de tela cresce pra baixo, Y de mundo cresce pra cima
    } else {
        m_Editor2DFirstMouseLook = true;
    }
    m_Editor2DLastMousePos = mousePos;

    if (m_ViewportHovered) {
        float scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            m_Editor2DZoom *= (1.0f - scroll * 0.1f);
            m_Editor2DZoom = std::clamp(m_Editor2DZoom, 0.5f, 500.0f);
        }
    }
    (void)ts;

    float aspect = m_ViewportSize.y > 0.0f ? m_ViewportSize.x / m_ViewportSize.y : 16.0f / 9.0f;
    m_Editor2DCamera.SetProjection(-m_Editor2DZoom * aspect, m_Editor2DZoom * aspect, -m_Editor2DZoom, m_Editor2DZoom);
    m_Editor2DCamera.SetPosition({ m_Editor2DCamPos.x, m_Editor2DCamPos.y, 0.0f });
}

void EditorLayer::DrawViewportToolbar() {
    KZ_TRACE_SCOPE("EditorLayer::DrawViewportToolbar");
    ImVec4 accent(0.82f, 0.24f, 0.27f, 1.0f);
    ImVec4 inactive(0.18f, 0.18f, 0.20f, 1.0f);

    bool is2D = m_ViewportMode == ViewportMode::Mode2D;
    bool is3D = m_ViewportMode == ViewportMode::Mode3D;

    // Alternância 2D/3D: só troca qual câmera/grid o EDITOR usa pra
    // navegar (ver comentário no header, junto de ViewportMode) — nunca
    // trava a cena. Uma entidade 2D e uma 3D convivem na mesma cena
    // independente de qual botão está ativo aqui.
    ImGui::PushStyleColor(ImGuiCol_Button, is2D ? accent : inactive);
    if (ImGui::Button("2D", ImVec2(36.0f, 0.0f))) m_ViewportMode = ViewportMode::Mode2D;
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 4.0f);

    ImGui::PushStyleColor(ImGuiCol_Button, is3D ? accent : inactive);
    if (ImGui::Button("3D", ImVec2(36.0f, 0.0f))) m_ViewportMode = ViewportMode::Mode3D;
    ImGui::PopStyleColor();

    ImGui::SameLine(0.0f, 16.0f);
    if (is2D)
        ImGui::TextDisabled("Arraste com o botão direito para navegar; role para aplicar zoom");
    else
        ImGui::TextDisabled("Navegue com botão direito + WASD; Q/E para subir e descer");

    // Play/Stop: física, scripts, partículas e áudio só rodam de verdade numa cópia da cena
    // (ver Scene::Copy) — a cena que você edita nunca é tocada, então Stop nunca "perde" nada.
    bool isPlaying = m_SceneState == SceneState::Play;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 72.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? accent : inactive);
    if (ImGui::Button(isPlaying ? "Stop" : "Play", ImVec2(60.0f, 0.0f))) {
        if (isPlaying) OnSceneStop(); else OnScenePlay();
    }
    ImGui::PopStyleColor();
}

// Projeta um ponto do mundo pra coordenada de tela dentro do retângulo do viewport, usando a
// view-projection já calculada. Devolve false se o ponto está atrás da câmera (clip.w <= 0) —
// nesse caso a linha desenhada ligado a ele ficaria "invertida", pior que não desenhar nada.
static bool ProjectToViewport(const glm::mat4& viewProj, const glm::vec3& worldPos,
                               const glm::vec2& viewportPos, const glm::vec2& viewportSize, ImVec2& outScreen) {
    glm::vec4 clip = viewProj * glm::vec4(worldPos, 1.0f);
    if (clip.w <= 0.001f) return false;
    glm::vec3 ndc = glm::vec3(clip) / clip.w;
    outScreen = ImVec2(viewportPos.x + (ndc.x * 0.5f + 0.5f) * viewportSize.x,
                        viewportPos.y + (1.0f - (ndc.y * 0.5f + 0.5f)) * viewportSize.y);
    return true;
}

// Sem isso, uma Camera na cena era um ponto totalmente invisível — nenhuma pista de onde ela
// tá nem pra onde aponta. Desenha uma pirâmide de frustum (tamanho fixo, só visualização —
// não é o far clip real) + uma seta curta de "frente", projetadas manualmente pra tela via
// ImDrawList (não passa pelo Renderer3D — mais simples que criar geometria de linha na GPU
// só pra isso, e já ganha profundidade/oclusão de graça por não ter nenhuma, sempre por cima).
void EditorLayer::DrawCameraGizmo() {
    if (!m_SelectedEntity || !m_SelectedEntity.HasComponent<CameraComponent>()) return;
    if (m_ViewportMode != ViewportMode::Mode3D) return; // frustum só faz sentido olhando em 3D

    auto& cc = m_SelectedEntity.GetComponent<CameraComponent>();
    glm::mat4 world = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
    glm::vec3 pos = glm::vec3(world[3]);
    glm::mat3 rot = glm::mat3(world);
    glm::vec3 forward = glm::normalize(rot * glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 up = glm::normalize(rot * glm::vec3(0.0f, 1.0f, 0.0f));
    glm::vec3 right = glm::normalize(rot * glm::vec3(1.0f, 0.0f, 0.0f));

    float gizmoDist = 1.5f;
    float fovRad = glm::radians(cc.Type == CameraComponent::ProjectionType::Perspective3D ? cc.PerspectiveFOV : 40.0f);
    float halfHeight = gizmoDist * tanf(fovRad * 0.5f);
    glm::vec2 vpPos = m_ViewportBounds[0], vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    float aspect = vpSize.y > 0.0f ? vpSize.x / vpSize.y : 16.0f / 9.0f;
    float halfWidth = halfHeight * aspect;

    glm::vec3 center = pos + forward * gizmoDist;
    glm::vec3 corners[4] = {
        center + up * halfHeight - right * halfWidth,
        center + up * halfHeight + right * halfWidth,
        center - up * halfHeight + right * halfWidth,
        center - up * halfHeight - right * halfWidth,
    };

    glm::mat4 viewProj = m_EditorCamera.GetViewProjectionMatrix();
    ImVec2 screenApex, screenCorners[4], screenForwardTip;
    bool ok = ProjectToViewport(viewProj, pos, vpPos, vpSize, screenApex);
    for (int i = 0; i < 4; ++i) ok &= ProjectToViewport(viewProj, corners[i], vpPos, vpSize, screenCorners[i]);
    ok &= ProjectToViewport(viewProj, pos + forward * (gizmoDist * 0.4f), vpPos, vpSize, screenForwardTip);
    if (!ok) return; // câmera do gizmo atrás da câmera do editor agora — melhor nada do que uma linha errada

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(255, 205, 60, 255); // amarelo — não colide com os eixos RGB do transform gizmo
    for (int i = 0; i < 4; ++i) dl->AddLine(screenApex, screenCorners[i], color, 1.5f);
    for (int i = 0; i < 4; ++i) dl->AddLine(screenCorners[i], screenCorners[(i + 1) % 4], color, 1.5f);
    dl->AddLine(screenApex, screenForwardTip, IM_COL32(255, 255, 255, 220), 2.5f);
}

void EditorLayer::DrawGizmo() {
    KZ_TRACE_SCOPE("EditorLayer::DrawGizmo");
    if (!m_SelectedEntity || !m_SelectedEntity.HasComponent<TransformComponent>()) return;

    // Atalhos de operação só valem com o viewport focado e a câmera livre
    // desativada (botão direito solto) — senão W entraria em conflito com
    // o "andar pra frente" da fly camera.
    bool flying = Input::IsMouseButtonPressed(Mouse::Right);
    if (m_ViewportHovered && !flying) {
        if (Input::IsKeyPressed(Key::W)) m_GizmoOperation = ImGuizmo::TRANSLATE;
        if (Input::IsKeyPressed(Key::E)) m_GizmoOperation = ImGuizmo::ROTATE;
        if (Input::IsKeyPressed(Key::R)) m_GizmoOperation = ImGuizmo::SCALE;
    }

    ImGuizmo::SetOrthographic(m_ViewportMode == ViewportMode::Mode2D);
    ImGuizmo::SetDrawlist();
    ImGuizmo::SetRect(m_ViewportBounds[0].x, m_ViewportBounds[0].y,
                       m_ViewportBounds[1].x - m_ViewportBounds[0].x,
                       m_ViewportBounds[1].y - m_ViewportBounds[0].y);

    glm::mat4 view = (m_ViewportMode == ViewportMode::Mode2D) ? m_Editor2DCamera.GetViewMatrix() : m_EditorCamera.GetViewMatrix();
    glm::mat4 proj = (m_ViewportMode == ViewportMode::Mode2D) ? m_Editor2DCamera.GetProjectionMatrix() : m_EditorCamera.GetProjectionMatrix();
    glm::mat4 worldTransform = m_ActiveScene->GetWorldTransform(m_SelectedEntity);

    bool snap = Input::IsKeyPressed(Key::LeftControl);
    float snapAmount = (m_GizmoOperation == ImGuizmo::OPERATION::ROTATE) ? 15.0f : 0.5f;
    float snapValues[3] = { snapAmount, snapAmount, snapAmount };

    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                          m_GizmoOperation, ImGuizmo::LOCAL,
                          glm::value_ptr(worldTransform), nullptr,
                          snap ? snapValues : nullptr);

    bool isUsing = ImGuizmo::IsUsing();

    // Início do arrasto: guarda o estado de antes pra poder desfazer o
    // gesto inteiro de uma vez (não um comando por frame de movimento).
    if (!m_GizmoWasUsing && isUsing)
        m_GizmoEditBefore = EntitySnapshot::Capture(m_SelectedEntity);

    if (isUsing) {
        // O gizmo edita o transform mundial; se a entidade tem pai, precisa
        // voltar pro espaço local dele antes de gravar em TransformComponent
        // (que sempre guarda posição/rotação/escala relativas ao pai).
        glm::mat4 localTransform = worldTransform;
        Entity parent = m_SelectedEntity.GetParent();
        if (parent) localTransform = glm::inverse(m_ActiveScene->GetWorldTransform(parent)) * worldTransform;

        glm::vec3 translation, rotation, scale;
        if (DecomposeTransform(localTransform, translation, rotation, scale)) {
            auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
            tc.Translation = translation;
            tc.Rotation = rotation;
            tc.Scale = scale;
        }
        // Usuário "autorou" a câmera da cena com o gizmo: o Play passa a
        // respeitar a pose dela em vez de espelhar a câmera livre do editor.
        if (m_SelectedEntity.HasComponent<CameraComponent>() &&
            m_SelectedEntity.GetComponent<CameraComponent>().Primary)
            m_UseEditorCameraOnPlay = false;
    }

    // Fim do arrasto: fecha o comando com o estado final (só se algo de
    // fato mudou — um clique que soltou sem arrastar não deveria virar
    // uma entrada de undo vazia).
    if (m_GizmoWasUsing && !isUsing) {
        EntitySnapshot after = EntitySnapshot::Capture(m_SelectedEntity);
        if (after.DiffersFrom(m_GizmoEditBefore))
            m_History.Push(CreateRef<EntityEditCommand>(m_SelectedEntity.GetUUID(), m_GizmoEditBefore, after));
    }
    m_GizmoWasUsing = isUsing;
}

void EditorLayer::OnScenePlay() {
    KZ_TRACE_SCOPE("EditorLayer::OnScenePlay");
    m_EditorScene = m_ActiveScene;
    m_ActiveScene = Scene::Copy(m_EditorScene);
    m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
    SyncEditorCameraToRuntimeScene(); // Play começa de onde a câmera do editor estava
    m_SelectedEntity = {}; // handle da cena antiga não é válido na cópia
    m_SceneState = SceneState::Play;
    m_ActiveScene->OnRuntimeStart();
}

void EditorLayer::OnSceneStop() {
    KZ_TRACE_SCOPE("EditorLayer::OnSceneStop");
    m_ActiveScene->OnRuntimeStop();
    AudioEngine::StopAll(); // OnRuntimeStop só cuida de física/scripts — sem isso, som ficava tocando pra sempre
    m_SelectedEntity = {};
    m_ActiveScene = m_EditorScene; // cena original nunca foi tocada — restaurar é só isso
    m_EditorScene = nullptr;
    m_SceneState = SceneState::Edit;
}

void EditorLayer::NewScene() {
    KZ_TRACE_SCOPE("EditorLayer::NewScene");
    if (m_SceneState == SceneState::Play) {
        KZ_CORE_WARN("EditorLayer::NewScene — ignorado durante o Play (trocar a cena no meio do runtime descartaria a cópia em execução).");
        return;
    }
    m_ActiveScene = CreateRef<Scene>("Nova Cena");
    m_SelectedEntity = {};
    m_ScenePath.clear();
    m_History.Clear();
    // Cena nova: a câmera livre do editor volta a guiar o Play até a câmera
    // da cena ser editada ou uma cena salva for aberta.
    m_UseEditorCameraOnPlay = true;
}

void EditorLayer::SaveScene() {
    KZ_TRACE_SCOPE("EditorLayer::SaveScene");
    if (m_SceneState == SceneState::Play) {
        KZ_CORE_WARN("EditorLayer::SaveScene — ignorado durante o Play (salvaria a cópia efêmera, não a cena real).");
        return;
    }
    // Sem caminho ainda associado à cena (nunca foi salva) -> se comporta
    // como "Salvar Como" e pede o caminho antes de gravar.
    if (m_ScenePath.empty()) {
        SaveSceneAs();
        return;
    }
    SceneSerializer(m_ActiveScene).Serialize(m_ScenePath);
}

void EditorLayer::SaveSceneAs() {
    KZ_TRACE_SCOPE("EditorLayer::SaveSceneAs");
    if (m_SceneState == SceneState::Play) {
        KZ_CORE_WARN("EditorLayer::SaveSceneAs — ignorado durante o Play (salvaria a cópia efêmera, não a cena real).");
        return;
    }
    strncpy(m_ScenePathBuffer, m_ScenePath.empty() ? "cena.kzscene" : m_ScenePath.c_str(), sizeof(m_ScenePathBuffer));
    m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';
    m_RequestOpenSaveAsPopup = true;
}

Entity EditorLayer::CreateEntityFromAsset(const std::string& path, const glm::vec3& worldPos) {
    KZ_TRACE_SCOPE("EditorLayer::CreateEntityFromAsset");
    std::string lower = path;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) { return (char)std::tolower(c); });

    Entity created;
    if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".obj") {
        created = m_ActiveScene->CreateEntity(std::filesystem::path(path).stem().string());
        auto& mr = created.AddComponent<MeshRendererComponent>();
        mr.MeshSource = path;
        mr.MeshAsset = Mesh::FromSource(path);
    } else {
        std::string ext = lower.size() >= 4 ? lower.substr(lower.size() - 4) : "";
        bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif");
        if (!isImage) return {};
        created = m_ActiveScene->CreateEntity(std::filesystem::path(path).stem().string());
        auto& sc = created.AddComponent<SpriteRendererComponent>();
        sc.TexturePath = path;
        sc.Texture = Texture2D::Create(path);
    }

    if (created) {
        auto& tc = created.GetComponent<TransformComponent>();
        tc.Translation = worldPos;
        m_History.Push(CreateRef<CreateEntityCommand>(created));
    }
    return created;
}

void EditorLayer::OpenScene(const std::string& path) {
    KZ_TRACE_SCOPE("EditorLayer::OpenScene");
    if (m_SceneState == SceneState::Play) {
        KZ_CORE_WARN("EditorLayer::OpenScene — ignorado durante o Play (trocar a cena no meio do runtime descartaria a cópia em execução).");
        return;
    }
    auto newScene = CreateRef<Scene>("Nova Cena");
    SceneSerializer serializer(newScene);
    if (!serializer.Deserialize(path)) return;

    m_ActiveScene = newScene;
    m_SelectedEntity = {};
    m_ScenePath = path;
    m_History.Clear();
    // Cena salva: a câmera da cena é a fonte da verdade — o Play nunca
    // sobrescreve a pose dela com a câmera livre do editor.
    m_UseEditorCameraOnPlay = false;
}

void EditorLayer::DrawTitlebar() {
    KZ_TRACE_SCOPE("EditorLayer::DrawTitlebar");
    auto& window = Application::Get().GetWindow();
    ImDrawList* dl = ImGui::GetWindowDrawList();

    ImVec2 winPos = ImGui::GetWindowPos();
    float width = ImGui::GetWindowSize().x;
    ImVec2 barMin = winPos;
    ImVec2 barMax = ImVec2(winPos.x + width, winPos.y + kTitlebarHeight);

    dl->AddRectFilled(barMin, barMax, ImGui::GetColorU32(ImVec4(0.055f, 0.055f, 0.065f, 1.0f)));
    dl->AddLine(ImVec2(barMin.x, barMax.y), ImVec2(barMax.x, barMax.y),
                ImGui::GetColorU32(ImVec4(0.16f, 0.16f, 0.18f, 1.0f)), 1.0f);

    // Marca (torii) + wordmark "KIZURI"
    float markSize = 20.0f;
    ImVec2 markPos(barMin.x + 14.0f, barMin.y + (kTitlebarHeight - markSize) * 0.5f);
    kizuri::editor::icons::Torii(dl, markPos, markSize, IM_COL32(217, 64, 77, 255));

    ImFont* titleFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Titlebar);
    ImVec2 wordmarkPos(markPos.x + markSize + 10.0f, barMin.y + (kTitlebarHeight - titleFont->FontSize) * 0.5f - 1.0f);
    dl->AddText(titleFont, titleFont->FontSize, wordmarkPos, ImGui::GetColorU32(ImVec4(0.90f, 0.90f, 0.92f, 1.0f)), "KIZURI");
    ImVec2 wordmarkTextSize = titleFont->CalcTextSizeA(titleFont->FontSize, FLT_MAX, 0.0f, "KIZURI");
    float dragZoneStartX = wordmarkPos.x + wordmarkTextSize.x + 24.0f;

    // Nome do projeto ativo, discreto, logo depois do wordmark — ajuda a
    // lembrar em qual projeto você está sem precisar abrir o menu Arquivo.
    {
        auto& project = Project::GetActive();
        std::string label = project ? project->GetConfig().Name : "Nenhum projeto aberto";
        ImFont* smallFont = ImGui::GetFont();
        ImVec2 labelPos(wordmarkPos.x + wordmarkTextSize.x + 14.0f, barMin.y + (kTitlebarHeight - smallFont->FontSize) * 0.5f - 1.0f);
        dl->AddText(smallFont, smallFont->FontSize, labelPos, ImGui::GetColorU32(ImVec4(0.5f, 0.5f, 0.54f, 1.0f)), label.c_str());
        ImVec2 labelSize = smallFont->CalcTextSizeA(smallFont->FontSize, FLT_MAX, 0.0f, label.c_str());
        dragZoneStartX = labelPos.x + labelSize.x + 20.0f;
    }

    // Botões da janela (minimizar / maximizar / fechar)
    const float btnW = 46.0f;
    float rightEdge = barMax.x;
    ImVec2 closeMin(rightEdge - btnW, barMin.y), closeMax(rightEdge, barMax.y);
    ImVec2 maxMin(rightEdge - btnW * 2, barMin.y), maxMax(rightEdge - btnW, barMax.y);
    ImVec2 minMin(rightEdge - btnW * 3, barMin.y), minMax(rightEdge - btnW * 2, barMax.y);

    ImGui::SetCursorScreenPos(closeMin);
    ImGui::InvisibleButton("##titlebar_close", ImVec2(btnW, kTitlebarHeight));
    bool closeHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) Application::Get().Close();

    ImGui::SetCursorScreenPos(maxMin);
    ImGui::InvisibleButton("##titlebar_max", ImVec2(btnW, kTitlebarHeight));
    bool maxHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) window.ToggleMaximize();

    ImGui::SetCursorScreenPos(minMin);
    ImGui::InvisibleButton("##titlebar_min", ImVec2(btnW, kTitlebarHeight));
    bool minHovered = ImGui::IsItemHovered();
    if (ImGui::IsItemClicked()) window.Minimize();

    if (closeHovered) dl->AddRectFilled(closeMin, closeMax, IM_COL32(196, 43, 46, 255));
    else if (maxHovered) dl->AddRectFilled(maxMin, maxMax, IM_COL32(255, 255, 255, 18));
    else if (minHovered) dl->AddRectFilled(minMin, minMax, IM_COL32(255, 255, 255, 18));

    auto center = [](ImVec2 a, ImVec2 b) { return ImVec2((a.x + b.x) * 0.5f, (a.y + b.y) * 0.5f); };
    const ImU32 iconColor = IM_COL32(215, 215, 218, 255);
    const ImU32 closeIconColor = closeHovered ? IM_COL32(255, 255, 255, 255) : iconColor;

    ImVec2 cc = center(closeMin, closeMax);
    dl->AddLine(ImVec2(cc.x - 4.5f, cc.y - 4.5f), ImVec2(cc.x + 4.5f, cc.y + 4.5f), closeIconColor, 1.3f);
    dl->AddLine(ImVec2(cc.x - 4.5f, cc.y + 4.5f), ImVec2(cc.x + 4.5f, cc.y - 4.5f), closeIconColor, 1.3f);

    ImVec2 mc = center(maxMin, maxMax);
    dl->AddRect(ImVec2(mc.x - 4.5f, mc.y - 4.5f), ImVec2(mc.x + 4.5f, mc.y + 4.5f), iconColor, 0.0f, 0, 1.2f);

    ImVec2 nc = center(minMin, minMax);
    dl->AddLine(ImVec2(nc.x - 4.5f, nc.y), ImVec2(nc.x + 4.5f, nc.y), iconColor, 1.3f);

    // Zona de arrasto: do fim do wordmark até o início dos botões.
    // Clique simples + arrastar move a janela; duplo-clique maximiza/restaura.
    float dragZoneEndX = minMin.x - 4.0f;
    ImGui::SetCursorScreenPos(ImVec2(dragZoneStartX, barMin.y));
    ImGui::InvisibleButton("##titlebar_drag", ImVec2(std::max(0.0f, dragZoneEndX - dragZoneStartX), kTitlebarHeight));

    if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
        window.ToggleMaximize();

    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
        m_DraggingWindow = true;

    if (m_DraggingWindow) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            if (delta.x != 0.0f || delta.y != 0.0f) {
                int x, y;
                window.GetPosition(x, y);
                window.SetPosition(x + (int)delta.x, y + (int)delta.y);
            }
        } else {
            m_DraggingWindow = false;
        }
    }
}

void EditorLayer::DrawResizeBorders() {
    KZ_TRACE_SCOPE("EditorLayer::DrawResizeBorders");
    auto& window = Application::Get().GetWindow();
    if (window.IsMaximized()) { m_ResizingEdge = ResizeEdge::None; return; }

    ImVec2 winPos = ImGui::GetWindowPos();
    ImVec2 winSize = ImGui::GetWindowSize();
    float left = winPos.x, top = winPos.y, right = winPos.x + winSize.x, bottom = winPos.y + winSize.y;

    auto handle = [&](const char* id, ImVec2 pos, ImVec2 size, ResizeEdge edge, ImGuiMouseCursor cursor) {
        ImGui::SetCursorScreenPos(pos);
        ImGui::InvisibleButton(id, size);
        if (ImGui::IsItemHovered() || m_ResizingEdge == edge)
            ImGui::SetMouseCursor(cursor);
        if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left))
            m_ResizingEdge = edge;
    };

    // Só laterais e parte de baixo: o topo é ocupado pela titlebar/menu, que
    // já servem como zona de arrasto — redimensionar por cima é raro o
    // suficiente pra não valer a complexidade extra aqui.
    handle("##rs_l", ImVec2(left, top + kTitlebarHeight), ImVec2(kResizeBorder, bottom - top - kTitlebarHeight - kResizeBorder), ResizeEdge::Left, ImGuiMouseCursor_ResizeEW);
    handle("##rs_r", ImVec2(right - kResizeBorder, top + kTitlebarHeight), ImVec2(kResizeBorder, bottom - top - kTitlebarHeight - kResizeBorder), ResizeEdge::Right, ImGuiMouseCursor_ResizeEW);
    handle("##rs_b", ImVec2(left + kResizeBorder, bottom - kResizeBorder), ImVec2(std::max(0.0f, right - left - 2 * kResizeBorder), kResizeBorder), ResizeEdge::Bottom, ImGuiMouseCursor_ResizeNS);
    handle("##rs_bl", ImVec2(left, bottom - kResizeBorder), ImVec2(kResizeBorder, kResizeBorder), ResizeEdge::BottomLeft, ImGuiMouseCursor_ResizeNESW);
    handle("##rs_br", ImVec2(right - kResizeBorder, bottom - kResizeBorder), ImVec2(kResizeBorder, kResizeBorder), ResizeEdge::BottomRight, ImGuiMouseCursor_ResizeNWSE);

    if (m_ResizingEdge != ResizeEdge::None) {
        if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            ImVec2 delta = ImGui::GetIO().MouseDelta;
            if (delta.x != 0.0f || delta.y != 0.0f) {
                int wx, wy;
                window.GetPosition(wx, wy);
                int ww = (int)window.GetWidth(), wh = (int)window.GetHeight();

                switch (m_ResizingEdge) {
                    case ResizeEdge::Left:        wx += (int)delta.x; ww -= (int)delta.x; break;
                    case ResizeEdge::Right:       ww += (int)delta.x; break;
                    case ResizeEdge::Bottom:      wh += (int)delta.y; break;
                    case ResizeEdge::BottomLeft:  wx += (int)delta.x; ww -= (int)delta.x; wh += (int)delta.y; break;
                    case ResizeEdge::BottomRight: ww += (int)delta.x; wh += (int)delta.y; break;
                    default: break;
                }

                const int kMinW = 480, kMinH = 320;
                if (ww < kMinW) {
                    if (m_ResizingEdge == ResizeEdge::Left || m_ResizingEdge == ResizeEdge::BottomLeft)
                        wx -= (kMinW - ww);
                    ww = kMinW;
                }
                if (wh < kMinH) wh = kMinH;

                window.SetPosition(wx, wy);
                window.SetSize(ww, wh);
            }
        } else {
            m_ResizingEdge = ResizeEdge::None;
        }
    }
}

void EditorLayer::DrawDockspace() {
    KZ_TRACE_SCOPE("EditorLayer::DrawDockspace");
    static bool dockspaceOpen = true;
    ImGuiWindowFlags windowFlags = ImGuiWindowFlags_NoDocking;
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    windowFlags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                   ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;

    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    KZ_CORE_TRACE("EditorLayer::DrawDockspace — chamando ImGui::Begin (janela raiz)");
    ImGui::Begin("KizuriEditorDockspace", &dockspaceOpen, windowFlags);
    ImGui::PopStyleVar(3);
    KZ_CORE_TRACE("EditorLayer::DrawDockspace — janela raiz aberta, chamando DrawTitlebar");

    DrawTitlebar();
    KZ_CORE_TRACE("EditorLayer::DrawDockspace — DrawTitlebar ok");

    // Faixa do menu, logo abaixo da titlebar. Desenhada à mão (em vez de
    // ImGui::BeginMenuBar) pra ficar sob controle total do layout junto
    // com a titlebar customizada.
    ImVec2 winPos = ImGui::GetWindowPos();
    float width = ImGui::GetWindowSize().x;
    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 menuMin(winPos.x, winPos.y + kTitlebarHeight);
    ImVec2 menuMax(winPos.x + width, winPos.y + kTitlebarHeight + kMenubarHeight);
    dl->AddRectFilled(menuMin, menuMax, ImGui::GetColorU32(ImVec4(0.08f, 0.08f, 0.09f, 1.0f)));

    ImGui::SetCursorScreenPos(ImVec2(menuMin.x + 6.0f, menuMin.y + 1.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
    if (ImGui::Button("Arquivo")) ImGui::OpenPopup("##menu_arquivo");
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();
    if (ImGui::BeginPopup("##menu_arquivo")) {
        if (ImGui::MenuItem("Novo Projeto...")) {
            strncpy(m_NewProjectDirBuffer, "MeuJogo", sizeof(m_NewProjectDirBuffer));
            strncpy(m_NewProjectNameBuffer, "MeuJogo", sizeof(m_NewProjectNameBuffer));
            m_RequestOpenNewProjectPopup = true;
        }
        if (ImGui::MenuItem("Abrir Projeto...")) {
            m_RequestOpenLoadProjectPopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Carregar GameModule...")) {
            m_RequestOpenGameModulePopup = true;
        }
        ImGui::Separator();
        // Nova/Abrir Cena travados durante o Play (igual Salvar): trocar a
        // m_ActiveScene no meio do runtime descartaria a cópia em execução
        // por baixo e é comportamento de modo edição dentro do jogo.
        if (ImGui::MenuItem("Nova Cena", nullptr, false, m_SceneState == SceneState::Edit)) NewScene();
        ImGui::Separator();
        if (ImGui::MenuItem("Abrir Cena...", nullptr, false, m_SceneState == SceneState::Edit)) {
            strncpy(m_ScenePathBuffer, m_ScenePath.empty() ? "cena.kzscene" : m_ScenePath.c_str(), sizeof(m_ScenePathBuffer));
            m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';
            m_RequestOpenLoadPopup = true;
        }
        if (ImGui::MenuItem("Salvar Cena", nullptr, false, (bool)m_ActiveScene)) SaveScene();
        if (ImGui::MenuItem("Salvar Cena Como...")) SaveSceneAs();
        ImGui::Separator();
        if (ImGui::MenuItem("Sair")) Application::Get().Close();
        ImGui::EndPopup();
    }

    // Dockspace com uma pequena margem nas laterais/embaixo — é essa
    // margem que sobra pras alças de redimensionamento (DrawResizeBorders)
    // sem entrar em conflito com o conteúdo dos painéis dockados nela.
    ImVec2 dockPos(winPos.x + kResizeBorder, menuMax.y);
    ImVec2 dockSize(width - kResizeBorder * 2.0f, winPos.y + ImGui::GetWindowSize().y - menuMax.y - kResizeBorder);
    ImGui::SetCursorScreenPos(dockPos);

    KZ_CORE_TRACE("EditorLayer::DrawDockspace — menu/faixa ok, montando dockspace");
    ImGuiID dockspaceID = ImGui::GetID("KizuriDockspace");
    // ImGuiDockNodeFlags_AutoHideTabBar: esconde a aba nativa do ImGui
    // quando um nó de dock tem uma única janela — que é o caso de todos os
    // painéis do layout padrão. Sem isso, cada painel mostra dois títulos:
    // a aba nativa (ex: "Hierarquia") por cima e o PanelHeader customizado
    // com ícone (ex: "HIERARQUIA") logo abaixo, duplicando a informação.
    // Se o usuário arrastar dois painéis pra dividir a mesma aba, a barra
    // volta a aparecer automaticamente pra permitir alternar entre eles.
    // ImGuiDockNodeFlags_NoWindowMenuButton: tira o triângulo de menu que
    // o ImGui desenha no canto de cada nó de dock (undock/hide tab bar
    // pelo menu) — decoração que não combina com o resto da UI da Kizuri,
    // que já tem seu próprio PanelHeader fazendo esse papel visualmente.
    ImGui::DockSpace(dockspaceID, dockSize, ImGuiDockNodeFlags_AutoHideTabBar | ImGuiDockNodeFlags_NoWindowMenuButton);
    KZ_CORE_TRACE("EditorLayer::DrawDockspace — DockSpace ok");

    // Layout padrão de fábrica do Kizuri Editor: Hierarquia e Content
    // Browser empilhados na coluna esquerda, Viewport e Console
    // empilhados na área central, Inspetor ocupando a coluna direita
    // inteira. Sem imgui.ini (ver ImGuiLayer::OnAttach), não há layout
    // salvo pra respeitar — é construído uma vez por sessão, sempre do
    // mesmo jeito. Rearranjos manuais do usuário duram a sessão atual,
    // mas não sobrevivem a fechar o editor; é a troca deliberada por não
    // ter arquivo solto de configuração ao lado do executável.
    static bool shouldBuildDefaultLayout = true;

    if (shouldBuildDefaultLayout) {
        shouldBuildDefaultLayout = false;

        ImGui::DockBuilderRemoveNode(dockspaceID);
        ImGui::DockBuilderAddNode(dockspaceID, ImGuiDockNodeFlags_DockSpace | ImGuiDockNodeFlags_NoWindowMenuButton);
        ImGui::DockBuilderSetNodeSize(dockspaceID, dockSize);

        ImGuiID dockMainID = dockspaceID;
        ImGuiID dockLeftID  = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Left,  0.1417f, nullptr, &dockMainID);
        ImGuiID dockRightID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Right, 0.1566f, nullptr, &dockMainID);
        // dockMainID agora é só a área central (embaixo do menu, entre a
        // coluna esquerda e a direita) — dividir ela em cima/baixo dá
        // Viewport em cima e Console embaixo, sem afetar as outras colunas.
        ImGuiID dockCenterBottomID = ImGui::DockBuilderSplitNode(dockMainID, ImGuiDir_Down, 0.28f, nullptr, &dockMainID);
        // Mesma ideia na coluna esquerda: Hierarquia em cima, Content
        // Browser embaixo, cada um com sua área própria (não em abas).
        ImGuiID dockLeftBottomID = ImGui::DockBuilderSplitNode(dockLeftID, ImGuiDir_Down, 0.35f, nullptr, &dockLeftID);

        ImGui::DockBuilderDockWindow("Hierarquia", dockLeftID);
        ImGui::DockBuilderDockWindow("Content Browser", dockLeftBottomID);
        ImGui::DockBuilderDockWindow("Viewport", dockMainID);
        ImGui::DockBuilderDockWindow("Console", dockCenterBottomID);
        ImGui::DockBuilderDockWindow("Inspetor", dockRightID);

        ImGui::DockBuilderFinish(dockspaceID);
    }

    DrawResizeBorders();

    ImGui::End();
}

void EditorLayer::DrawSceneFileModals() {
    KZ_TRACE_SCOPE("EditorLayer::DrawSceneFileModals");
    // As flags são setadas pelos itens do menu Arquivo (que rodam dentro de
    // outro popup) e lidas aqui, no nível superior do frame, pra evitar
    // abrir um popup modal a partir de dentro de outro.
    if (m_RequestOpenSaveAsPopup) {
        m_RequestOpenSaveAsPopup = false;
        ImGui::OpenPopup("Salvar Cena Como");
    }
    if (m_RequestOpenLoadPopup) {
        m_RequestOpenLoadPopup = false;
        ImGui::OpenPopup("Abrir Cena");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Salvar Cena Como", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Caminho do arquivo de cena (.kzscene):");
        ImGui::SetNextItemWidth(-84.0f);
        bool enterPressed = ImGui::InputText("##save_path", m_ScenePathBuffer, sizeof(m_ScenePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##save_browse")) {
            std::string path = FileDialog::SaveFile("Cena Kizuri", "*.kzscene", "kzscene");
            if (!path.empty()) {
                strncpy(m_ScenePathBuffer, path.c_str(), sizeof(m_ScenePathBuffer) - 1);
                m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        bool save = ImGui::Button("Salvar") || enterPressed;
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancelar");

        if (save && m_ScenePathBuffer[0] != '\0') {
            m_ScenePath = m_ScenePathBuffer;
            SceneSerializer(m_ActiveScene).Serialize(m_ScenePath);
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Abrir Cena", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Caminho do arquivo de cena (.kzscene):");
        ImGui::SetNextItemWidth(-84.0f);
        bool enterPressed = ImGui::InputText("##open_path", m_ScenePathBuffer, sizeof(m_ScenePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##open_browse")) {
            std::string path = FileDialog::OpenFile("Cena Kizuri", "*.kzscene");
            if (!path.empty()) {
                strncpy(m_ScenePathBuffer, path.c_str(), sizeof(m_ScenePathBuffer) - 1);
                m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        bool open = ImGui::Button("Abrir") || enterPressed;
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancelar");

        if (open && m_ScenePathBuffer[0] != '\0') {
            OpenScene(m_ScenePathBuffer);
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawProjectModals() {
    KZ_TRACE_SCOPE("EditorLayer::DrawProjectModals");
    if (m_RequestOpenNewProjectPopup) {
        m_RequestOpenNewProjectPopup = false;
        ImGui::OpenPopup("Novo Projeto");
    }
    if (m_RequestOpenLoadProjectPopup) {
        m_RequestOpenLoadProjectPopup = false;
        ImGui::OpenPopup("Abrir Projeto");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Novo Projeto", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Diretório do projeto (será criado se não existir):");
        ImGui::SetNextItemWidth(-84.0f);
        ImGui::InputText("##new_project_dir", m_NewProjectDirBuffer, sizeof(m_NewProjectDirBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##new_project_browse")) {
            std::string dir = FileDialog::SelectFolder();
            if (!dir.empty()) {
                strncpy(m_NewProjectDirBuffer, dir.c_str(), sizeof(m_NewProjectDirBuffer) - 1);
                m_NewProjectDirBuffer[sizeof(m_NewProjectDirBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        ImGui::TextUnformatted("Nome do projeto:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##new_project_name", m_NewProjectNameBuffer, sizeof(m_NewProjectNameBuffer));

        ImGui::Spacing();
        ImGui::TextUnformatted("Modo inicial (define apenas câmera/grade iniciais, sem restrições):");
        ImGui::RadioButton("2D", &m_NewProjectModeIndex, 0); ImGui::SameLine();
        ImGui::RadioButton("3D", &m_NewProjectModeIndex, 1); ImGui::SameLine();
        ImGui::RadioButton("Vazio", &m_NewProjectModeIndex, 2);

        ImGui::Spacing();
        bool create = ImGui::Button("Criar Projeto");
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancelar");

        if (create && m_NewProjectDirBuffer[0] != '\0') {
            ProjectMode mode = m_NewProjectModeIndex == 0 ? ProjectMode::TwoD
                              : m_NewProjectModeIndex == 1 ? ProjectMode::ThreeD
                              : ProjectMode::Empty;
            Ref<Project> project = Project::New(m_NewProjectDirBuffer, m_NewProjectNameBuffer, mode);
            if (project) {
                // Sugere salvar a próxima cena já dentro da pasta de
                // assets do projeto novo, em vez do diretório de trabalho
                // solto de antes.
                std::string suggested = (std::filesystem::path(project->GetAssetDirectory()) / "cena.kzscene").string();
                strncpy(m_ScenePathBuffer, suggested.c_str(), sizeof(m_ScenePathBuffer));
                m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';

                m_ContentBrowserRoot = project->GetAssetDirectory();
                m_ContentBrowserCurrentDir = m_ContentBrowserRoot;

                // "Vazio" não mexe no que já estava selecionado — é
                // literalmente pra não empurrar uma opinião de câmera
                // padrão. 2D/3D aplicam o botão correspondente na toolbar.
                if (mode == ProjectMode::TwoD) m_ViewportMode = ViewportMode::Mode2D;
                else if (mode == ProjectMode::ThreeD) m_ViewportMode = ViewportMode::Mode3D;
            }
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }

    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(460.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Abrir Projeto", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextUnformatted("Caminho do arquivo de projeto (.kzproj):");
        ImGui::SetNextItemWidth(-84.0f);
        bool enterPressed = ImGui::InputText("##open_project_path", m_OpenProjectPathBuffer, sizeof(m_OpenProjectPathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##open_project_browse")) {
            std::string path = FileDialog::OpenFile("Projeto Kizuri", "*.kzproj");
            if (!path.empty()) {
                strncpy(m_OpenProjectPathBuffer, path.c_str(), sizeof(m_OpenProjectPathBuffer) - 1);
                m_OpenProjectPathBuffer[sizeof(m_OpenProjectPathBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        bool open = ImGui::Button("Abrir") || enterPressed;
        ImGui::SameLine();
        bool cancel = ImGui::Button("Cancelar");

        if (open && m_OpenProjectPathBuffer[0] != '\0') {
            Ref<Project> project = Project::Load(m_OpenProjectPathBuffer);
            if (project) {
                m_ContentBrowserRoot = project->GetAssetDirectory();
                m_ContentBrowserCurrentDir = m_ContentBrowserRoot;

                ProjectMode savedMode = project->GetConfig().DefaultMode;
                if (savedMode == ProjectMode::TwoD) m_ViewportMode = ViewportMode::Mode2D;
                else if (savedMode == ProjectMode::ThreeD) m_ViewportMode = ViewportMode::Mode3D;
            }
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}


void EditorLayer::DrawGameModuleModal() {
    KZ_TRACE_SCOPE("EditorLayer::DrawGameModuleModal");
    if (m_RequestOpenGameModulePopup) {
        m_RequestOpenGameModulePopup = false;
        ImGui::OpenPopup("Carregar GameModule");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(500.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Carregar GameModule", nullptr, ImGuiWindowFlags_NoResize)) {
        kizuri::editor::icons::PanelHeader("CARREGAR GAMEMODULE", kizuri::editor::icons::Folder);

        ImGui::TextWrapped(
            "Carregue a biblioteca dinâmica (.dll/.so) com o código do jogo em C++, compilada a "
            "partir da pasta Source/ do projeto. A biblioteca deve exportar a função "
            "RegisterScripts(ScriptRegistry&) com linkage C.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-84.0f);
        bool enterPressed = ImGui::InputText("##game_module_path", m_GameModulePathBuffer, sizeof(m_GameModulePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##game_module_browse")) {
            std::string path = FileDialog::OpenFile("Biblioteca Dinâmica", "*.dll;*.so");
            if (!path.empty()) {
                strncpy(m_GameModulePathBuffer, path.c_str(), sizeof(m_GameModulePathBuffer) - 1);
                m_GameModulePathBuffer[sizeof(m_GameModulePathBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();

        // --- Status de verdade: o que aconteceu na última tentativa, não só "fechou o popup e torce". ---
        bool loaded = ScriptEngine::IsModuleLoaded();
        const std::string& lastError = ScriptEngine::GetLastError();
        if (loaded) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.35f, 0.85f, 0.45f, 1.0f));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Módulo carregado: %s", ScriptEngine::GetLoadedPath().c_str());
            ImGui::PopStyleColor();

            auto classNames = ScriptEngine::GetRegistry().GetClassNames();
            ImGui::TextDisabled("Scripts registrados (%d):", (int)classNames.size());
            if (!classNames.empty()) {
                ImGui::Indent();
                for (auto& name : classNames) ImGui::BulletText("%s", name.c_str());
                ImGui::Unindent();
            }
        } else if (!lastError.empty()) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.35f, 0.35f, 1.0f));
            ImGui::Bullet();
            ImGui::SameLine();
            ImGui::TextWrapped("Falha no carregamento: %s", lastError.c_str());
            ImGui::PopStyleColor();
        } else {
            ImGui::TextDisabled("Nenhum módulo carregado.");
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        const ImVec4 accent(0.82f, 0.24f, 0.27f, 1.0f);
        const ImVec4 accentHover(0.90f, 0.32f, 0.35f, 1.0f);
        ImGui::PushStyleColor(ImGuiCol_Button, accent);
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, accentHover);
        bool load = ImGui::Button("Carregar", ImVec2(100.0f, 0.0f)) || enterPressed;
        ImGui::PopStyleColor(2);
        ImGui::SameLine();

        ImGui::BeginDisabled(!loaded);
        bool unload = ImGui::Button("Descarregar", ImVec2(100.0f, 0.0f));
        ImGui::EndDisabled();
        ImGui::SameLine();
        bool cancel = ImGui::Button("Fechar", ImVec2(80.0f, 0.0f));

        if (load && m_GameModulePathBuffer[0] != '\0') {
            // Só fecha se deu certo — se falhar, o popup fica aberto mostrando o erro em vez de
            // simplesmente sumir e deixar o usuário sem saber que nada foi carregado.
            if (ScriptEngine::LoadModule(m_GameModulePathBuffer)) ImGui::CloseCurrentPopup();
        } else if (unload) {
            ScriptEngine::UnloadModule();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawConsole() {
    KZ_TRACE_SCOPE("EditorLayer::DrawConsole");
    BeginPanelNoMenuButton();
    ImGui::Begin("Console");
    kizuri::editor::icons::PanelHeader("CONSOLE", kizuri::editor::icons::Console);

    if (ImGui::Button("Limpar")) LogHistory::Clear();
    ImGui::SameLine();
    ImGui::Checkbox("Trace", &m_ConsoleShowTrace); ImGui::SameLine();
    ImGui::Checkbox("Info", &m_ConsoleShowInfo); ImGui::SameLine();
    ImGui::Checkbox("Aviso", &m_ConsoleShowWarn); ImGui::SameLine();
    ImGui::Checkbox("Erro", &m_ConsoleShowError); ImGui::SameLine();
    ImGui::Checkbox("Rolagem automática", &m_ConsoleAutoScroll);

    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##console_search", "Pesquisar...", m_ConsoleSearchBuffer, sizeof(m_ConsoleSearchBuffer));

    ImGui::Separator();
    ImGui::BeginChild("##console_scroll", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

    std::string search = m_ConsoleSearchBuffer;
    for (auto& entry : LogHistory::GetEntries()) {
        bool visible = false;
        ImVec4 color(0.82f, 0.82f, 0.84f, 1.0f);
        switch (entry.Level) {
            case LogLevel::Trace:
            case LogLevel::Debug:
                visible = m_ConsoleShowTrace; color = ImVec4(0.55f, 0.55f, 0.58f, 1.0f); break;
            case LogLevel::Info:
                visible = m_ConsoleShowInfo; break;
            case LogLevel::Warn:
                visible = m_ConsoleShowWarn; color = ImVec4(0.92f, 0.75f, 0.25f, 1.0f); break;
            default:
                visible = m_ConsoleShowError; color = ImVec4(0.92f, 0.35f, 0.35f, 1.0f); break;
        }
        if (!visible) continue;
        if (!search.empty() && entry.Message.find(search) == std::string::npos) continue;

        ImGui::PushStyleColor(ImGuiCol_Text, color);
        ImGui::TextUnformatted(entry.Message.c_str());
        ImGui::PopStyleColor();
    }

    if (m_ConsoleAutoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 1.0f)
        ImGui::SetScrollHereY(1.0f);

    ImGui::EndChild();
    ImGui::End();
}

void EditorLayer::DrawContentBrowser() {
    KZ_TRACE_SCOPE("EditorLayer::DrawContentBrowser");
    BeginPanelNoMenuButton();
    ImGui::Begin("Content Browser");
    kizuri::editor::icons::PanelHeader("CONTENT BROWSER", kizuri::editor::icons::Folder);

    auto& project = Project::GetActive();
    if (!project) {
        ImGui::TextDisabled("Nenhum projeto aberto. Use Arquivo > Novo Projeto ou Abrir Projeto para começar.");
        ImGui::End();
        return;
    }

    if (m_ContentBrowserRoot.empty()) {
        m_ContentBrowserRoot = project->GetAssetDirectory();
        m_ContentBrowserCurrentDir = m_ContentBrowserRoot;
    }

    // Breadcrumb + botão "voltar" — só habilitado enquanto ainda estamos
    // dentro da raiz de assets (não deixa navegar pra fora dela).
    std::error_code eqEc;
    bool atRoot = std::filesystem::equivalent(m_ContentBrowserCurrentDir, m_ContentBrowserRoot, eqEc) || eqEc;
    ImGui::BeginDisabled(atRoot);
    if (ImGui::Button("← Voltar") && !atRoot)
        m_ContentBrowserCurrentDir = m_ContentBrowserCurrentDir.parent_path();
    ImGui::EndDisabled();
    ImGui::SameLine();
    std::error_code relEc;
    auto relPath = std::filesystem::relative(m_ContentBrowserCurrentDir, m_ContentBrowserRoot, relEc);
    ImGui::TextDisabled("%s", relEc ? "/" : relPath.string().c_str());

    ImGui::Separator();

    if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
        if (ImGui::MenuItem("Nova Pasta")) {
            std::error_code ec;
            std::filesystem::create_directory(m_ContentBrowserCurrentDir / "Nova Pasta", ec);
        }
        ImGui::EndPopup();
    }

    const float thumbSize = 72.0f;
    const float cellSize = thumbSize + 16.0f;
    float panelWidth = ImGui::GetContentRegionAvail().x;
    int columns = std::max(1, (int)(panelWidth / cellSize));
    if (!ImGui::BeginTable("##content_browser_grid", columns)) { ImGui::End(); return; }

    std::error_code ec;
    std::vector<std::filesystem::directory_entry> entries;
    for (auto& e : std::filesystem::directory_iterator(m_ContentBrowserCurrentDir, ec))
        entries.push_back(e);
    std::sort(entries.begin(), entries.end(), [](auto& a, auto& b) {
        if (a.is_directory() != b.is_directory()) return a.is_directory();
        return a.path().filename() < b.path().filename();
    });

    for (auto& entry : entries) {
        ImGui::TableNextColumn();
        std::string name = entry.path().filename().string();
        bool isDir = entry.is_directory();

        ImGui::PushID(name.c_str());
        ImGui::BeginGroup();

        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 cursor = ImGui::GetCursorScreenPos();
        ImGui::InvisibleButton("##thumb", ImVec2(thumbSize, thumbSize));
        bool clicked = ImGui::IsItemClicked();
        bool doubleClicked = ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && ImGui::IsItemHovered();

        // Arquivo arrastável pro viewport: payload com o caminho absoluto.
        if (!isDir && ImGui::BeginDragDropSource()) {
            std::string filePath = entry.path().string();
            ImGui::SetDragDropPayload("KZ_CONTENT_FILE", filePath.c_str(), filePath.size() + 1);
            ImGui::Text("%s", name.c_str());
            ImGui::EndDragDropSource();
        }

        ImU32 iconColor = isDir ? IM_COL32(217, 180, 100, 255) : IM_COL32(150, 150, 156, 255);
        if (isDir)
            kizuri::editor::icons::Folder(dl, ImVec2(cursor.x + thumbSize * 0.15f, cursor.y + thumbSize * 0.15f), thumbSize * 0.7f, iconColor);
        else
            dl->AddRect(ImVec2(cursor.x + thumbSize * 0.2f, cursor.y + thumbSize * 0.1f),
                        ImVec2(cursor.x + thumbSize * 0.8f, cursor.y + thumbSize * 0.9f), iconColor, 2.0f, 0, 2.0f);

        ImGui::TextWrapped("%s", name.c_str());
        ImGui::EndGroup();

        // Arrastar um arquivo daqui carrega o caminho absoluto no payload
        // — ainda não há nenhum slot no Inspetor que aceite esse drop
        // (entra quando a Pipeline de Assets da seção 7 do roadmap
        // avançar), mas a fonte já existir agora significa zero retrabalho
        // no editor quando isso acontecer.
        if (!isDir && ImGui::BeginDragDropSource()) {
            std::string fullPath = entry.path().string();
            ImGui::SetDragDropPayload("KZ_CONTENT_BROWSER_FILE", fullPath.c_str(), fullPath.size() + 1);
            ImGui::TextUnformatted(name.c_str());
            ImGui::EndDragDropSource();
        }

        if (doubleClicked) {
            if (isDir) {
                m_ContentBrowserCurrentDir = entry.path();
            } else if (entry.path().extension() == ".kzscene") {
                OpenScene(entry.path().string());
            }
        }
        (void)clicked;

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Excluir")) {
                std::error_code delEc;
                std::filesystem::remove_all(entry.path(), delEc);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndTable();
    ImGui::End();
}

void EditorLayer::DrawSceneHierarchy() {
    KZ_TRACE_SCOPE("EditorLayer::DrawSceneHierarchy");
    BeginPanelNoMenuButton();
    ImGui::Begin("Hierarquia");
    kizuri::editor::icons::PanelHeader("HIERARQUIA", kizuri::editor::icons::Hierarchy);

    // Durante o Play a hierarquia vira leitura: mostra a cena que está
    // rodando (a cópia), mas sem selecionar/criar/deletar/reparentar —
    // essas operações são de modo edição e mexeriam na cópia efêmera, que
    // é descartada no Stop de qualquer jeito.
    bool editable = m_SceneState == SceneState::Edit;

    // A entidade marcada pra deletar (se houver) só é destruída depois que
    // o .each() abaixo termina de percorrer a view — destruir no meio da
    // iteração invalidaria o iterador do EnTT.
    Entity entityToDelete;

    // Só entra na recursão a partir das raízes (sem pai) — DrawEntityNode
    // desenha os filhos dela mesma, então cada entidade aparece uma única
    // vez na árvore em vez de duplicada como lista plana.
    m_ActiveScene->GetRegistry().view<TagComponent, RelationshipComponent>().each(
        [&](auto entityHandle, TagComponent&, RelationshipComponent& rel) {
            if (rel.Parent.IsValid()) return;
            Entity entity{ entityHandle, m_ActiveScene.get() };
            DrawEntityNode(entity, entityToDelete, editable);
        });

    if (editable && entityToDelete) {
        if (m_SelectedEntity == entityToDelete) m_SelectedEntity = {};
        m_History.Push(CreateRef<DeleteEntityCommand>(entityToDelete));
        m_ActiveScene->DestroyEntity(entityToDelete);
    }

    if (editable) {
        // Espaço vazio do painel também aceita drop — soltar uma entidade aqui
        // fora de qualquer nó desanexa ela do pai (vira raiz de novo).
        ImGui::Dummy(ImGui::GetContentRegionAvail());
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ENTITY_UUID")) {
                uint64_t draggedId;
                std::memcpy(&draggedId, payload->Data, sizeof(uint64_t));
                Entity dragged = m_ActiveScene->GetEntityByUUID(UUID(draggedId));
                if (dragged) Reparent(dragged, {});
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
            if (ImGui::MenuItem("Criar Entidade Vazia")) {
                Entity created = m_ActiveScene->CreateEntity("Nova Entidade");
                m_History.Push(CreateRef<CreateEntityCommand>(created));
                m_SelectedEntity = created;
            }
            ImGui::EndPopup();
        }
    }
    ImGui::End();
}

void EditorLayer::Reparent(Entity child, Entity newParent) {
    KZ_TRACE_SCOPE("EditorLayer::Reparent");
    if (!child) return;
    UUID oldParentId = child.GetParent() ? child.GetParent().GetUUID() : UUID::Invalid();
    UUID newParentId = newParent ? newParent.GetUUID() : UUID::Invalid();
    if (oldParentId == newParentId) return; // nada mudou, não polui o histórico

    child.SetParent(newParent);
    m_History.Push(CreateRef<ReparentCommand>(child.GetUUID(), oldParentId, newParentId));
}

void EditorLayer::DrawEntityNode(Entity entity, Entity& outEntityToDelete, bool editable) {
    auto& tag = entity.GetComponent<TagComponent>().Tag;
    auto children = entity.GetChildren();

    ImGuiTreeNodeFlags flags = ((m_SelectedEntity == entity) ? ImGuiTreeNodeFlags_Selected : 0)
        | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool isLeaf = children.empty();
    if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());
    // Só o modo edição seleciona por clique — no Play a árvore é leitura.
    if (editable && ImGui::IsItemClicked()) {
        m_SelectedEntity = entity;
        AutoSwitchViewportMode(); // selecionar 3D/2D troca o modo do viewport
    }

    if (editable) {
        // Arrastar esta entidade pra reparentar em outra.
        if (ImGui::BeginDragDropSource()) {
            uint64_t id = (uint64_t)entity.GetUUID();
            ImGui::SetDragDropPayload("KZ_ENTITY_UUID", &id, sizeof(uint64_t));
            ImGui::Text("%s", tag.c_str());
            ImGui::EndDragDropSource();
        }

        // Soltar outra entidade em cima desta faz ela virar filha.
        if (ImGui::BeginDragDropTarget()) {
            if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_ENTITY_UUID")) {
                uint64_t draggedId;
                std::memcpy(&draggedId, payload->Data, sizeof(uint64_t));
                Entity dragged = m_ActiveScene->GetEntityByUUID(UUID(draggedId));
                if (dragged && dragged != entity) Reparent(dragged, entity);
            }
            ImGui::EndDragDropTarget();
        }

        if (ImGui::BeginPopupContextItem()) {
            if (ImGui::MenuItem("Excluir Entidade")) outEntityToDelete = entity;
            if (entity.GetParent() && ImGui::MenuItem("Desvincular do pai")) Reparent(entity, {});
            ImGui::EndPopup();
        }
    }

    if (opened && !isLeaf) {
        for (Entity child : children)
            DrawEntityNode(child, outEntityToDelete, editable);
        ImGui::TreePop();
    }
}

// Desenha o cabeçalho padrão de uma seção de componente removível: o
// TreeNodeEx normal + um botão "x" alinhado à direita pra remover o
// componente. Usado por todo componente opcional (tudo exceto Transform,
// que toda entidade sempre tem).
static bool DrawComponentHeader(const char* label, bool* removeRequested) {
    ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_AllowItemOverlap;
    bool open = ImGui::TreeNodeEx(label, flags);
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 20.0f);
    *removeRequested = ImGui::SmallButton("x");
    return open;
}

void EditorLayer::DrawInspector() {
    KZ_TRACE_SCOPE("EditorLayer::DrawInspector");
    BeginPanelNoMenuButton();
    ImGui::Begin("Inspetor");
    kizuri::editor::icons::PanelHeader("INSPETOR", kizuri::editor::icons::Inspector);

    // Início de uma edição (qualquer widget do inspetor ficando ativo pela
    // primeira vez neste frame): guarda o "antes". Cobre DragFloat,
    // ColorEdit, Checkbox, Combo e até os botões de Adicionar/Remover
    // Componente com o mesmo mecanismo — não precisa de fiação por widget.
    if (m_SelectedEntity && !m_InspectorWasActive) {
        m_InspectorEditEntity = m_SelectedEntity.GetUUID();
        m_InspectorEditBefore = EntitySnapshot::Capture(m_SelectedEntity);
    }

    if (m_SelectedEntity) {
        auto& tag = m_SelectedEntity.GetComponent<TagComponent>().Tag;
        char buffer[256];
        strncpy(buffer, tag.c_str(), sizeof(buffer));
        if (ImGui::InputText("Nome", buffer, sizeof(buffer))) tag = std::string(buffer);

        if (m_SelectedEntity.HasComponent<TransformComponent>()) {
            auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
            if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                bool transformEdited = ImGui::DragFloat3("Posição", &tc.Translation.x, 0.1f);
                transformEdited |= ImGui::DragFloat3("Rotação", &tc.Rotation.x, 0.1f);
                transformEdited |= ImGui::DragFloat3("Escala", &tc.Scale.x, 0.1f);
                if (transformEdited && m_SelectedEntity.HasComponent<CameraComponent>() &&
                    m_SelectedEntity.GetComponent<CameraComponent>().Primary)
                    m_UseEditorCameraOnPlay = false;
                ImGui::TreePop();
            }
        }

        bool removeThis = false;

        if (m_SelectedEntity.HasComponent<SpriteRendererComponent>()) {
            auto& sc = m_SelectedEntity.GetComponent<SpriteRendererComponent>();
            if (DrawComponentHeader("Sprite Renderer", &removeThis)) {
                char texBuf[512];
                strncpy(texBuf, sc.TexturePath.c_str(), sizeof(texBuf) - 1);
                texBuf[sizeof(texBuf) - 1] = '\0';
                if (ImGui::InputText("Textura", texBuf, sizeof(texBuf))) {
                    sc.TexturePath = texBuf;
                    sc.Texture = sc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(sc.TexturePath);
                }
                if (sc.Texture) {
                    uint32_t texID = sc.Texture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%ux%u", sc.Texture->GetWidth(), sc.Texture->GetHeight());
                }
                ImGui::ColorEdit4("Cor", &sc.Color.x);
                ImGui::DragFloat("Tiling", &sc.TilingFactor, 0.1f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<SpriteRendererComponent>();
        }

        if (m_SelectedEntity.HasComponent<CircleRendererComponent>()) {
            auto& cr = m_SelectedEntity.GetComponent<CircleRendererComponent>();
            if (DrawComponentHeader("Circle Renderer", &removeThis)) {
                ImGui::ColorEdit4("Cor", &cr.Color.x);
                ImGui::DragFloat("Espessura", &cr.Thickness, 0.025f, 0.0f, 1.0f);
                ImGui::DragFloat("Suavização", &cr.Fade, 0.001f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<CircleRendererComponent>();
        }

        if (m_SelectedEntity.HasComponent<CameraComponent>()) {
            auto& cc = m_SelectedEntity.GetComponent<CameraComponent>();
            if (DrawComponentHeader("Camera", &removeThis)) {
                ImGui::Checkbox("Principal", &cc.Primary);
                ImGui::DragFloat("Tamanho Ortho", &cc.OrthoSize, 0.1f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<CameraComponent>();
        }

        if (m_SelectedEntity.HasComponent<MeshRendererComponent>()) {
            auto& mr = m_SelectedEntity.GetComponent<MeshRendererComponent>();
            if (DrawComponentHeader("Mesh Renderer", &removeThis)) {
                auto& mat = mr.MeshMaterial;
                // Fonte da mesh: combobox com os builtins + campo livre pra .obj.
                const char* builtins[] = { "builtin:cube", "builtin:plane", "builtin:sphere" };
                int currentBuiltin = -1;
                for (int i = 0; i < 3; ++i) if (mr.MeshSource == builtins[i]) currentBuiltin = i;
                if (ImGui::Combo("Mesh pronta", &currentBuiltin, builtins, 3)) {
                    if (currentBuiltin >= 0) {
                        mr.MeshSource = builtins[currentBuiltin];
                        mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                    }
                }
                char meshBuf[512];
                strncpy(meshBuf, mr.MeshSource.c_str(), sizeof(meshBuf) - 1);
                meshBuf[sizeof(meshBuf) - 1] = '\0';
                if (ImGui::InputText("Malha (.obj)", meshBuf, sizeof(meshBuf))) {
                    mr.MeshSource = meshBuf;
                    if (!mr.MeshSource.empty()) mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                }
                ImGui::ColorEdit3("Albedo", &mat.Albedo.x);
                ImGui::DragFloat("Metallic", &mat.Metallic, 0.01f, 0.0f, 1.0f);
                ImGui::DragFloat("Roughness", &mat.Roughness, 0.01f, 0.02f, 1.0f);
                ImGui::DragFloat("AO", &mat.AO, 0.01f, 0.0f, 1.0f);
                char albBuf[512], nrmBuf[512];
                strncpy(albBuf, mat.AlbedoMapPath.c_str(), sizeof(albBuf) - 1); albBuf[sizeof(albBuf) - 1] = '\0';
                strncpy(nrmBuf, mat.NormalMapPath.c_str(), sizeof(nrmBuf) - 1); nrmBuf[sizeof(nrmBuf) - 1] = '\0';
                if (ImGui::InputText("Mapa de Albedo", albBuf, sizeof(albBuf))) {
                    mat.AlbedoMapPath = albBuf;
                    mat.AlbedoMap = mat.AlbedoMapPath.empty() ? nullptr : kizuri::Texture2D::Create(mat.AlbedoMapPath);
                }
                if (mat.AlbedoMap) {
                    uint32_t texID = mat.AlbedoMap->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(64.0f, 64.0f), ImVec2(0, 1), ImVec2(1, 0));
                }
                if (ImGui::InputText("Mapa de Normais", nrmBuf, sizeof(nrmBuf))) {
                    mat.NormalMapPath = nrmBuf;
                    mat.NormalMap = mat.NormalMapPath.empty() ? nullptr : kizuri::Texture2D::Create(mat.NormalMapPath);
                }
                if (mat.NormalMap) {
                    uint32_t texID = mat.NormalMap->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(64.0f, 64.0f), ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<MeshRendererComponent>();
        }

        if (m_SelectedEntity.HasComponent<TextComponent>()) {
            auto& tc = m_SelectedEntity.GetComponent<TextComponent>();
            if (DrawComponentHeader("Texto", &removeThis)) {
                char textBuf[1024];
                strncpy(textBuf, tc.Text.c_str(), sizeof(textBuf) - 1);
                textBuf[sizeof(textBuf) - 1] = '\0';
                if (ImGui::InputText("Texto", textBuf, sizeof(textBuf))) tc.Text = textBuf;
                ImGui::ColorEdit4("Cor", &tc.Color.x);
                ImGui::DragFloat("Tamanho (px)", &tc.FontSize, 1.0f, 8.0f, 512.0f);
                const char* alignItems[] = { "Esquerda", "Centro", "Direita" };
                int alignIdx = (int)tc.Alignment;
                if (ImGui::Combo("Alinhamento", &alignIdx, alignItems, 3))
                    tc.Alignment = (kizuri::TextAlignment)alignIdx;
                ImGui::TextDisabled("Fonte integrada JetBrains Mono. Use \\n para quebrar linha.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<TextComponent>();
        }

        if (m_SelectedEntity.HasComponent<SpriteAnimationComponent>()) {
            auto& sac = m_SelectedEntity.GetComponent<SpriteAnimationComponent>();
            if (DrawComponentHeader("Animação de Sprite", &removeThis)) {
                char sheetBuf[512];
                strncpy(sheetBuf, sac.SheetPath.c_str(), sizeof(sheetBuf) - 1);
                sheetBuf[sizeof(sheetBuf) - 1] = '\0';
                if (ImGui::InputText("Sprite Sheet", sheetBuf, sizeof(sheetBuf))) {
                    sac.SheetPath = sheetBuf;
                    sac.SheetTexture = sac.SheetPath.empty() ? nullptr : kizuri::Texture2D::Create(sac.SheetPath);
                }
                if (sac.SheetTexture) {
                    uint32_t texID = sac.SheetTexture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%ux%u", sac.SheetTexture->GetWidth(), sac.SheetTexture->GetHeight());
                }
                int cols = (int)sac.FramesPerRow, total = (int)sac.TotalFrames;
                if (ImGui::DragInt("Frames por linha", &cols, 1, 1, 64)) sac.FramesPerRow = (uint32_t)cols;
                if (ImGui::DragInt("Total de frames", &total, 1, 1, 4096)) sac.TotalFrames = (uint32_t)total;
                ImGui::DragFloat("FPS", &sac.FPS, 0.5f, 1.0f, 120.0f);
                ImGui::Checkbox("Em loop", &sac.Loop);
                ImGui::SameLine();
                ImGui::Checkbox("Em execução", &sac.Playing);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<SpriteAnimationComponent>();
        }

        if (m_SelectedEntity.HasComponent<TilemapComponent>()) {
            auto& tmc = m_SelectedEntity.GetComponent<TilemapComponent>();
            if (DrawComponentHeader("Tilemap", &removeThis)) {
                char atlasBuf[512];
                strncpy(atlasBuf, tmc.AtlasPath.c_str(), sizeof(atlasBuf) - 1);
                atlasBuf[sizeof(atlasBuf) - 1] = '\0';
                if (ImGui::InputText("Atlas de Tiles", atlasBuf, sizeof(atlasBuf))) {
                    tmc.AtlasPath = atlasBuf;
                    tmc.AtlasTexture = tmc.AtlasPath.empty() ? nullptr : kizuri::Texture2D::Create(tmc.AtlasPath);
                }
                if (tmc.AtlasTexture) {
                    uint32_t texID = tmc.AtlasTexture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%ux%u", tmc.AtlasTexture->GetWidth(), tmc.AtlasTexture->GetHeight());
                }
                int atCols = (int)tmc.AtlasColumns, atRows = (int)tmc.AtlasRows;
                if (ImGui::DragInt("Colunas no atlas", &atCols, 1, 1, 256)) tmc.AtlasColumns = (uint32_t)atCols;
                if (ImGui::DragInt("Linhas no atlas", &atRows, 1, 1, 256)) tmc.AtlasRows = (uint32_t)atRows;
                int mw = (int)tmc.MapWidth, mh = (int)tmc.MapHeight;
                if (ImGui::DragInt("Largura do mapa", &mw, 1, 1, 4096)) tmc.MapWidth = (uint32_t)mw;
                if (ImGui::DragInt("Altura do mapa", &mh, 1, 1, 4096)) tmc.MapHeight = (uint32_t)mh;
                ImGui::DragFloat2("Tamanho do tile", &tmc.TileSize.x, 0.1f, 0.1f, 100.0f);
                tmc.Tiles.resize((size_t)tmc.MapWidth * tmc.MapHeight);

                // Pincel do pintor de tilemap (funciona no viewport 2D).
                ImGui::DragInt("Pincel", &m_TilemapBrushValue, 1, 0, 4096);
                ImGui::TextDisabled("Pinte no viewport 2D: o botão esquerdo aplica o pincel, o botão direito apaga.");

                if (tmc.Tiles.size() > 0 && ImGui::CollapsingHeader("Tiles (valores)")) {
                    ImGui::TextDisabled("%zu tiles — 0=vazio; N=posição (N-1) no atlas.", tmc.Tiles.size());
                    static int s_EditIdx = -1;
                    ImGui::DragInt("Índice", &s_EditIdx, 1, -1, (int)tmc.Tiles.size() - 1);
                    if (s_EditIdx >= 0 && s_EditIdx < (int)tmc.Tiles.size()) {
                        int tv = (int)tmc.Tiles[s_EditIdx];
                        if (ImGui::InputInt("Tile", &tv, 1, 16))
                            tmc.Tiles[s_EditIdx] = (uint32_t)glm::max(tv, 0);
                    }
                }

                // Tiles sólidos: valores que geram collider Box2D no Play.
                if (ImGui::CollapsingHeader("Colisão (tiles sólidos)")) {
                    ImGui::TextDisabled("Tiles com colisor estático. Ex.: chão=1, plataforma=2.");
                    static int s_AddSolid = 1;
                    ImGui::SetNextItemWidth(80.0f);
                    ImGui::InputInt("Valor", &s_AddSolid, 1, 4);
                    ImGui::SameLine();
                    if (ImGui::Button("Adicionar") && s_AddSolid > 0) {
                        uint32_t v = (uint32_t)s_AddSolid;
                        if (std::find(tmc.SolidTileValues.begin(), tmc.SolidTileValues.end(), v) == tmc.SolidTileValues.end())
                            tmc.SolidTileValues.push_back(v);
                    }
                    int removeIdx = -1;
                    for (size_t i = 0; i < tmc.SolidTileValues.size(); ++i) {
                        ImGui::PushID((int)i);
                        ImGui::Text("Tile %u", tmc.SolidTileValues[i]);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("remover")) removeIdx = (int)i;
                        ImGui::PopID();
                    }
                    if (removeIdx >= 0) tmc.SolidTileValues.erase(tmc.SolidTileValues.begin() + removeIdx);
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<TilemapComponent>();
        }

        if (m_SelectedEntity.HasComponent<LightComponent>()) {
            auto& lc = m_SelectedEntity.GetComponent<LightComponent>();
            if (DrawComponentHeader("Light", &removeThis)) {
                const char* types[] = { "Direcional", "Point", "Spot" };
                int current = (int)lc.Type;
                if (ImGui::Combo("Tipo", &current, types, IM_ARRAYSIZE(types)))
                    lc.Type = (LightType)current;
                ImGui::ColorEdit3("Cor", &lc.Color.x);
                ImGui::DragFloat("Intensidade", &lc.Intensity, 0.05f, 0.0f, FLT_MAX);
                if (lc.Type != LightType::Directional)
                    ImGui::DragFloat("Alcance", &lc.Range, 0.1f, 0.1f, FLT_MAX);
                if (lc.Type == LightType::Spot) {
                    ImGui::DragFloat("Cone Interno (°)", &lc.InnerConeDeg, 0.5f, 0.0f, lc.OuterConeDeg);
                    ImGui::DragFloat("Cone Externo (°)", &lc.OuterConeDeg, 0.5f, lc.InnerConeDeg, 89.0f);
                }
                if (lc.Type != LightType::Point)
                    ImGui::TextDisabled("A direção acompanha a rotação do Transform da entidade.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<LightComponent>();
        }

        if (m_SelectedEntity.HasComponent<ParticleSystemComponent>()) {
            auto& pc = m_SelectedEntity.GetComponent<ParticleSystemComponent>();
            if (DrawComponentHeader("Particle System", &removeThis)) {
                ImGui::Checkbox("Em execução", &pc.Playing);
                ImGui::SameLine();
                ImGui::Checkbox("Modo aditivo (fogo/faíscas)", &pc.Additive);
                ImGui::DragFloat("Taxa (partículas/s)", &pc.EmissionRate, 0.5f, 0.0f, FLT_MAX);
                int maxP = (int)pc.MaxParticles;
                if (ImGui::DragInt("Máx. de partículas", &maxP, 1.0f, 1, (int)kMaxParticlesPerBatch))
                    pc.MaxParticles = (uint32_t)maxP;
                ImGui::DragFloatRange2("Tempo de vida (s)", &pc.LifetimeMin, &pc.LifetimeMax, 0.05f, 0.05f, 30.0f);
                ImGui::DragFloat3("Velocidade mín.", &pc.VelocityMin.x, 0.1f);
                ImGui::DragFloat3("Velocidade máx.", &pc.VelocityMax.x, 0.1f);
                ImGui::DragFloat3("Gravidade", &pc.Gravity.x, 0.1f);
                ImGui::ColorEdit4("Cor inicial", &pc.StartColor.x);
                ImGui::ColorEdit4("Cor final", &pc.EndColor.x);
                ImGui::DragFloat("Tamanho inicial", &pc.StartSize, 0.01f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Tamanho final", &pc.EndSize, 0.01f, 0.0f, FLT_MAX);
                ImGui::TextDisabled("%d partículas ativas.", (int)pc.ActiveParticles.size());
                ImGui::TextDisabled("A simulação ocorre apenas durante o Play, assim como a física.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<ParticleSystemComponent>();
        }

        if (m_SelectedEntity.HasComponent<AudioSourceComponent>()) {
            auto& ac = m_SelectedEntity.GetComponent<AudioSourceComponent>();
            if (DrawComponentHeader("Audio Source", &removeThis)) {
                char pathBuf[512];
                strncpy(pathBuf, ac.ClipPath.c_str(), sizeof(pathBuf) - 1);
                pathBuf[sizeof(pathBuf) - 1] = '\0';
                if (ImGui::InputText("Arquivo de áudio (.wav/.mp3/.ogg/.flac)", pathBuf, sizeof(pathBuf)))
                    ac.ClipPath = pathBuf;
                ImGui::Checkbox("Em loop", &ac.Loop);
                ImGui::SameLine();
                ImGui::Checkbox("Reproduzir ao iniciar", &ac.PlayOnStart);
                ImGui::Checkbox("Áudio espacial (3D)", &ac.Spatial);
                ImGui::DragFloat("Volume", &ac.Volume, 0.01f, 0.0f, 2.0f);
                if (ac.Spatial) {
                    ImGui::DragFloat("Distância mín.", &ac.MinDistance, 0.1f, 0.01f, ac.MaxDistance);
                    ImGui::DragFloat("Distância máx.", &ac.MaxDistance, 0.5f, ac.MinDistance, FLT_MAX);
                }
                ImGui::TextDisabled(ac.Handle != kInvalidSound ? "Carregado." : "Ainda não carregado.");
                ImGui::TextDisabled("A reprodução ocorre apenas durante o Play, assim como física e partículas.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<AudioSourceComponent>();
        }

        if (m_SelectedEntity.HasComponent<Rigidbody2DComponent>()) {
            auto& rb = m_SelectedEntity.GetComponent<Rigidbody2DComponent>();
            if (DrawComponentHeader("Rigidbody 2D", &removeThis)) {
                const char* types[] = { "Estático", "Dinâmico", "Cinemático" };
                int current = (int)rb.Type;
                if (ImGui::Combo("Tipo", &current, types, IM_ARRAYSIZE(types)))
                    rb.Type = (Rigidbody2DComponent::BodyType)current;
                ImGui::Checkbox("Rotação fixa", &rb.FixedRotation);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<Rigidbody2DComponent>();
        }

        if (m_SelectedEntity.HasComponent<BoxCollider2DComponent>()) {
            auto& bc = m_SelectedEntity.GetComponent<BoxCollider2DComponent>();
            if (DrawComponentHeader("Box Collider 2D", &removeThis)) {
                ImGui::DragFloat2("Offset", &bc.Offset.x, 0.05f);
                ImGui::DragFloat2("Tamanho", &bc.Size.x, 0.05f);
                ImGui::DragFloat("Densidade", &bc.Density, 0.05f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Fricção", &bc.Friction, 0.02f, 0.0f, 1.0f);
                ImGui::DragFloat("Restituição", &bc.Restitution, 0.02f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<BoxCollider2DComponent>();
        }

        if (m_SelectedEntity.HasComponent<NativeScriptComponent>()) {
            auto& nsc = m_SelectedEntity.GetComponent<NativeScriptComponent>();
            if (DrawComponentHeader("Script Nativo", &removeThis)) {
                auto classNames = ScriptEngine::GetRegistry().GetClassNames();
                if (classNames.empty()) {
                    ImGui::TextDisabled("Nenhum módulo carregado. Use Arquivo > Carregar GameModule.");
                    if (!nsc.ClassName.empty())
                        ImGui::TextDisabled("Classe vinculada na cena: %s (será religada ao carregar o módulo)", nsc.ClassName.c_str());
                } else {
                    std::string preview = nsc.ClassName.empty() ? "(nenhuma classe)" : nsc.ClassName;
                    if (ImGui::BeginCombo("Classe", preview.c_str())) {
                        for (auto& name : classNames) {
                            bool selected = (name == nsc.ClassName);
                            if (ImGui::Selectable(name.c_str(), selected))
                                nsc.BindByName(name);
                            if (selected) ImGui::SetItemDefaultFocus();
                        }
                        ImGui::EndCombo();
                    }
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<NativeScriptComponent>();
        }

        ImGui::Spacing();
        DrawAddComponentButton();
    }

    bool isActiveNow = ImGui::IsAnyItemActive();
    if (m_InspectorWasActive && !isActiveNow) {
        Entity edited = m_ActiveScene->GetEntityByUUID(m_InspectorEditEntity);
        if (edited) {
            EntitySnapshot after = EntitySnapshot::Capture(edited);
            if (after.DiffersFrom(m_InspectorEditBefore))
                m_History.Push(CreateRef<EntityEditCommand>(m_InspectorEditEntity, m_InspectorEditBefore, after));
        }
    }
    m_InspectorWasActive = isActiveNow;

    ImGui::End();
}

void EditorLayer::DrawAddComponentButton() {
    KZ_TRACE_SCOPE("EditorLayer::DrawAddComponentButton");
    if (ImGui::Button("+ Adicionar Componente", ImVec2(-1.0f, 0.0f)))
        ImGui::OpenPopup("##add_component");

    if (ImGui::BeginPopup("##add_component")) {
        if (!m_SelectedEntity.HasComponent<SpriteRendererComponent>() && ImGui::MenuItem("Sprite Renderer"))
            m_SelectedEntity.AddComponent<SpriteRendererComponent>();
        if (!m_SelectedEntity.HasComponent<CircleRendererComponent>() && ImGui::MenuItem("Circle Renderer"))
            m_SelectedEntity.AddComponent<CircleRendererComponent>();
        if (!m_SelectedEntity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera"))
            m_SelectedEntity.AddComponent<CameraComponent>();
        if (!m_SelectedEntity.HasComponent<MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer")) {
            auto& mr = m_SelectedEntity.AddComponent<MeshRendererComponent>();
            mr.MeshAsset = Mesh::CreateCube();
        }
        if (!m_SelectedEntity.HasComponent<LightComponent>() && ImGui::MenuItem("Light"))
            m_SelectedEntity.AddComponent<LightComponent>();
        if (!m_SelectedEntity.HasComponent<TextComponent>() && ImGui::MenuItem("Texto (HUD)"))
            m_SelectedEntity.AddComponent<TextComponent>();
        if (!m_SelectedEntity.HasComponent<SpriteAnimationComponent>() && ImGui::MenuItem("Animação de Sprite"))
            m_SelectedEntity.AddComponent<SpriteAnimationComponent>();
        if (!m_SelectedEntity.HasComponent<TilemapComponent>() && ImGui::MenuItem("Tilemap"))
            m_SelectedEntity.AddComponent<TilemapComponent>();
        if (!m_SelectedEntity.HasComponent<ParticleSystemComponent>() && ImGui::MenuItem("Particle System"))
            m_SelectedEntity.AddComponent<ParticleSystemComponent>();
        if (!m_SelectedEntity.HasComponent<AudioSourceComponent>() && ImGui::MenuItem("Audio Source"))
            m_SelectedEntity.AddComponent<AudioSourceComponent>();
        if (!m_SelectedEntity.HasComponent<Rigidbody2DComponent>() && ImGui::MenuItem("Rigidbody 2D"))
            m_SelectedEntity.AddComponent<Rigidbody2DComponent>();
        if (!m_SelectedEntity.HasComponent<BoxCollider2DComponent>() && ImGui::MenuItem("Box Collider 2D"))
            m_SelectedEntity.AddComponent<BoxCollider2DComponent>();
        if (!m_SelectedEntity.HasComponent<NativeScriptComponent>() && ImGui::MenuItem("Script Nativo"))
            m_SelectedEntity.AddComponent<NativeScriptComponent>();
        ImGui::EndPopup();
    }
}

void EditorLayer::OnImGuiRender() {
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — início");

    // NOTA DE DIAGNÓSTICO (ver KizuriEngine.log de 16/07/2026): o processo
    // travou silenciosamente em algum ponto entre a linha "início" acima e
    // a entrada em DrawDockspace() — nem "--> EditorLayer::DrawDockspace"
    // (que o KZ_TRACE_SCOPE dela loga como PRIMEIRA coisa que faz) chegou a
    // aparecer. Revisão linha a linha não achou nenhum bug lógico nesse
    // trecho (m_ActiveScene e m_SelectedEntity já estavam válidos nesse
    // ponto da execução, undo/redo com pilha vazia é no-op seguro). As
    // linhas KZ_CORE_TRACE abaixo, ausentes na versão anterior, existem só
    // pra isolar a linha exata se isso acontecer de novo — cada uma é um
    // ponto de log síncrono (flush imediato) que sobrevive mesmo se o
    // processo morrer logo em seguida.
    ImGuiIO& io = ImGui::GetIO();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — GetIO ok");

    // FIX: o gizmo (2D e 3D) não respondia a clique/arrasto porque
    // ImGuizmo::BeginFrame() nunca era chamado. A própria documentação do
    // ImGuizmo exige essa chamada uma vez por frame, logo após
    // ImGui::NewFrame() (que roda dentro de ImGuiLayer::Begin(), antes desta
    // função) — sem ela, o estado interno de mouse/hover do ImGuizmo não é
    // resetado a cada frame e Manipulate() nunca detecta IsOver()/IsUsing()
    // corretamente. Precisa vir ANTES de qualquer ImGui::Begin() de painel
    // (dockspace, viewport, etc), então fica logo no topo da função.
    ImGuizmo::SetImGuiContext(ImGui::GetCurrentContext());
    ImGuizmo::BeginFrame();

    // Atalhos de undo/redo. Só valem no modo de edição: durante o Play o
    // undo/redo mexeria na CÓPIA em runtime (e a pilha de comandos foi
    // gravada contra a cena mestra), o que só confundiria — desfazer/refazer
    // é ferramenta de edição. Também ignora enquanto o ImGui quer captura de
    // texto (WantTextInput) — senão Ctrl+Z dentro de um campo de nome
    // brigaria com o undo nativo do próprio InputText.
    //
    // Usa kizuri::Input (leitura direta do GLFW) em vez de
    // ImGui::IsKeyPressed(ImGuiKey_Z/_Y, false): essa segunda forma foi
    // isolada, via o trace fino logo acima, como o ponto exato de um crash
    // silencioso e reproduzível nesta função em pelo menos um ambiente
    // real (ver KizuriEngine.log — a execução nunca chegava a imprimir
    // "atalho Ctrl+Z detectado" nem a linha seguinte, mesmo com o predicado
    // inteiro sendo só leitura de bool + essa chamada). kizuri::Input já é
    // usado sem problema em todo o resto desta mesma função (câmera livre
    // do viewport, poucas linhas abaixo) — trocar pra ele aqui elimina a
    // API suspeita sem perder a funcionalidade. A troca de "está segurada"
    // (o que kizuri::Input dá) por "acabou de ser pressionada" (o que o
    // atalho precisa, senão Ctrl+Z segurado desfaria a cada frame) é feita
    // manualmente comparando com o estado do frame anterior.
    bool ctrlDown = io.KeyCtrl;
    bool zDown = kizuri::Input::IsKeyPressed(kizuri::Key::Z);
    bool yDown = kizuri::Input::IsKeyPressed(kizuri::Key::Y);
    bool zJustPressed = zDown && !m_PrevZKeyDown;
    bool yJustPressed = yDown && !m_PrevYKeyDown;
    m_PrevZKeyDown = zDown;
    m_PrevYKeyDown = yDown;
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — estado de teclas (Input::) lido ok");

    if (m_SceneState == SceneState::Edit && !io.WantTextInput) {
        if (ctrlDown && zJustPressed) {
            KZ_CORE_TRACE("EditorLayer::OnImGuiRender — atalho Ctrl+Z detectado");
            if (io.KeyShift) m_History.Redo(*m_ActiveScene);
            else m_History.Undo(*m_ActiveScene);
            KZ_CORE_TRACE("EditorLayer::OnImGuiRender — undo/redo (Z) ok");
        }
        if (ctrlDown && yJustPressed) {
            KZ_CORE_TRACE("EditorLayer::OnImGuiRender — atalho Ctrl+Y detectado");
            m_History.Redo(*m_ActiveScene);
            KZ_CORE_TRACE("EditorLayer::OnImGuiRender — redo (Y) ok");
        }
    }
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — atalhos undo/redo ok");

    // Um undo/redo pode ter destruído a entidade selecionada (ex: desfazer
    // a criação dela) — sem essa checagem, o Inspetor ficaria segurando um
    // handle apontando pra nada e travaria no primeiro GetComponent.
    // Blindagem extra: m_ActiveScene nunca deveria ser nulo aqui (o editor
    // sempre mantém uma cena ativa), mas se algum fluxo futuro criar uma
    // janela de "nenhum projeto aberto" sem cena, isso não pode virar
    // crash — só pula a checagem de seleção.
    if (m_SelectedEntity && m_ActiveScene && !m_ActiveScene->GetRegistry().valid(m_SelectedEntity.GetHandle()))
        m_SelectedEntity = {};
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — validação de seleção ok, chamando DrawDockspace");

    DrawDockspace();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawDockspace ok");
    DrawSceneFileModals();
    DrawProjectModals();
    DrawGameModuleModal();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — modais ok");
    DrawSceneHierarchy();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawSceneHierarchy ok");
    DrawInspector();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawInspector ok");
    DrawConsole();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawConsole ok");
    DrawContentBrowser();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawContentBrowser ok");

    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    BeginPanelNoMenuButton();
    ImGui::Begin("Viewport");
    ImGui::PopStyleVar();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — janela Viewport aberta");

    ImGui::Indent(8.0f);
    ImGui::Dummy(ImVec2(1.0f, 4.0f));
    kizuri::editor::icons::PanelHeader("VIEWPORT", kizuri::editor::icons::Viewport);
    DrawViewportToolbar();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — toolbar do viewport ok");
    ImGui::Unindent(8.0f);

    m_ViewportFocused = ImGui::IsWindowFocused();
    m_ViewportHovered = ImGui::IsWindowHovered();

    ImVec2 panelSize = ImGui::GetContentRegionAvail();
    m_ViewportSize = { panelSize.x, panelSize.y };

    ImVec2 viewportOffset = ImGui::GetWindowPos();
    ImVec2 minRegion = ImGui::GetWindowContentRegionMin();
    ImVec2 maxRegion = ImGui::GetWindowContentRegionMax();
    m_ViewportBounds[0] = { minRegion.x + viewportOffset.x, minRegion.y + viewportOffset.y };
    m_ViewportBounds[1] = { maxRegion.x + viewportOffset.x, maxRegion.y + viewportOffset.y };

    uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — chamando ImGui::Image (textureID={0}, {1}x{2})", textureID, panelSize.x, panelSize.y);
    // O framebuffer é preenchido de baixo para cima (origem OpenGL), então
    // invertemos as UVs verticalmente para a imagem aparecer com a
    // orientação correta dentro do ImGui.
    ImGui::Image((ImTextureID)(uint64_t)textureID, panelSize, ImVec2(0, 1), ImVec2(1, 0));
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — ImGui::Image ok");

    // O gizmo precisa desenhar ANTES do teste de clique abaixo — é isso
    // que atualiza ImGuizmo::IsOver()/IsUsing() pro estado do MOUSE ATUAL
    // deste frame. Na ordem antiga (picking primeiro, gizmo depois), o
    // teste usava o IsOver() do frame anterior: no exato frame em que o
    // mouse entrava num handle pela primeira vez, IsOver() ainda estava
    // "false" (só ficaria true depois que o Manipulate() daquele mesmo
    // frame rodasse), então o clique passava batido pelo picking, não
    // acertava nada em 3D, e desselecionava a entidade — o "seleciona e
    // desseleciona rapidinho" que já foi reportado.
    // Gizmo de transformação e gizmo de câmera são ferramentas de EDIÇÃO:
    // não existem durante o Play (a seleção já é limpa em OnScenePlay, mas
    // se algum fluxo futuro re-selecionasse durante o runtime, o gizmo
    // ficaria flutuando sobre a cena rodando e permitiria arrastar entidades
    // da cópia — comportamento de modo edição dentro do jogo).
    if (m_SceneState == SceneState::Edit) {
        KZ_CORE_TRACE("EditorLayer::OnImGuiRender — chamando DrawGizmo");
        DrawGizmo();
        KZ_CORE_TRACE("EditorLayer::OnImGuiRender — DrawGizmo ok");
        DrawCameraGizmo();
    } else {
        KZ_CORE_TRACE("EditorLayer::OnImGuiRender — gizmo pulado (Play)");
    }

    // Arrastar um arquivo do Content Browser pro viewport cria a entidade
    // na posição do mouse (só em modo edição).
    if (m_SceneState == SceneState::Edit && ImGui::BeginDragDropTarget()) {
        if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_CONTENT_FILE")) {
            std::string path((const char*)payload->Data);
            glm::vec2 mouse{ ImGui::GetMousePos().x, ImGui::GetMousePos().y };
            glm::vec2 local = mouse - m_ViewportBounds[0];
            glm::vec2 size = m_ViewportBounds[1] - m_ViewportBounds[0];
            glm::vec3 spawn = { 0.0f, 0.0f, 0.0f };
            if (size.x > 0.0f && size.y > 0.0f &&
                local.x >= 0.0f && local.y >= 0.0f && local.x <= size.x && local.y <= size.y) {
                glm::vec2 ndc{ (local.x / size.x) * 2.0f - 1.0f, 1.0f - (local.y / size.y) * 2.0f };
                if (m_ViewportMode == ViewportMode::Mode3D) {
                    glm::mat4 inv = glm::inverse(m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix());
                    glm::vec4 nearP = inv * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                    glm::vec4 farP  = inv * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
                    glm::vec3 o = glm::vec3(nearP) / nearP.w;
                    glm::vec3 d = glm::normalize(glm::vec3(farP) / farP.w - o);
                    float t = (0.0f - o.y) / glm::max(d.y, 0.0001f); // cai no chão (y=0)
                    spawn = (t > 0.0f) ? o + d * t : o;
                } else {
                    glm::mat4 inv = glm::inverse(m_Editor2DCamera.GetProjectionMatrix() * m_Editor2DCamera.GetViewMatrix());
                    glm::vec4 wp = inv * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
                    spawn = { wp.x / wp.w, wp.y / wp.w, 0.0f };
                }
            }
            Entity created = CreateEntityFromAsset(path, spawn);
            if (created) {
                m_SelectedEntity = created;
                AutoSwitchViewportMode();
            }
        }
        ImGui::EndDragDropTarget();
    }

    // Clique esquerdo no viewport seleciona a entidade sob o cursor. No
    // modo 3D é raycast contra o AABB dos meshes; no 2D é ponto-dentro-de-
    // quad/círculo/texto (PickEntity2D). Ignora clique no próprio gizmo e
    // só vale em modo de edição (no Play nada se seleciona).
    if (m_SceneState == SceneState::Edit &&
        ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
        !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        glm::vec2 mouse{ ImGui::GetMousePos().x, ImGui::GetMousePos().y };
        glm::vec2 local = mouse - m_ViewportBounds[0];
        glm::vec2 size = m_ViewportBounds[1] - m_ViewportBounds[0];

        if (local.x >= 0.0f && local.y >= 0.0f && local.x <= size.x && local.y <= size.y && size.x > 0.0f && size.y > 0.0f) {
            glm::vec2 ndc{ (local.x / size.x) * 2.0f - 1.0f, 1.0f - (local.y / size.y) * 2.0f };

            if (m_ViewportMode == ViewportMode::Mode3D) {
                glm::mat4 invViewProj = glm::inverse(m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix());
                glm::vec4 nearP = invViewProj * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                glm::vec4 farP  = invViewProj * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
                nearP /= nearP.w;
                farP /= farP.w;

                glm::vec3 rayOrigin = glm::vec3(nearP);
                glm::vec3 rayDir = glm::normalize(glm::vec3(farP - nearP));
                m_SelectedEntity = m_ActiveScene->PickEntity(rayOrigin, rayDir);
            } else {
                // 2D: projeta o NDC de volta pro plano Z=0 do mundo (câmera
                // ortográfica do editor) e testa ponto contra os renderers 2D.
                glm::mat4 invViewProj = glm::inverse(m_Editor2DCamera.GetProjectionMatrix() * m_Editor2DCamera.GetViewMatrix());
                glm::vec4 worldP = invViewProj * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
                worldP /= worldP.w;
                m_SelectedEntity = m_ActiveScene->PickEntity2D({ worldP.x, worldP.y });
            }
            AutoSwitchViewportMode(); // clicar num objeto 3D/2D ajusta o modo do viewport
            KZ_CORE_TRACE("EditorLayer::OnImGuiRender — picking por clique ok");
        }
    }

    // Pintor de tilemap: com uma entidade Tilemap selecionada no modo 2D,
    // arrastar com o botão esquerdo pinta o valor do pincel
    // (m_TilemapBrushValue) e o botão direito apaga (0). Só em modo edição
    // e sem conflitar com o gizmo. O undo é por gesto: snapshot no início e
    // um EntityEditCommand ao soltar o mouse.
    if (m_SceneState == SceneState::Edit && m_ViewportMode == ViewportMode::Mode2D &&
        m_SelectedEntity && m_SelectedEntity.HasComponent<TilemapComponent>() &&
        ImGui::IsItemHovered() && !ImGuizmo::IsOver() && !ImGuizmo::IsUsing()) {
        auto& tmc = m_SelectedEntity.GetComponent<TilemapComponent>();
        bool painting = ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool erasing = ImGui::IsMouseDown(ImGuiMouseButton_Right);
        if (painting || erasing) {
            if (!m_TilePainting) {
                m_TilePainting = true;
                m_TilePaintBefore = EntitySnapshot::Capture(m_SelectedEntity);
            }
            glm::vec2 mouse{ ImGui::GetMousePos().x, ImGui::GetMousePos().y };
            glm::vec2 local = mouse - m_ViewportBounds[0];
            glm::vec2 size = m_ViewportBounds[1] - m_ViewportBounds[0];
            if (size.x > 0.0f && size.y > 0.0f &&
                local.x >= 0.0f && local.y >= 0.0f && local.x <= size.x && local.y <= size.y) {
                glm::vec2 ndc{ (local.x / size.x) * 2.0f - 1.0f, 1.0f - (local.y / size.y) * 2.0f };
                glm::mat4 invViewProj = glm::inverse(m_Editor2DCamera.GetProjectionMatrix() * m_Editor2DCamera.GetViewMatrix());
                glm::vec4 worldP = invViewProj * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
                worldP /= worldP.w;

                glm::vec3 mapPos = glm::vec3(m_ActiveScene->GetWorldTransform(m_SelectedEntity)[3]);
                int tx = (int)glm::floor((worldP.x - mapPos.x) / tmc.TileSize.x);
                int ty = (int)glm::floor((worldP.y - mapPos.y) / tmc.TileSize.y);
                if (tx >= 0 && ty >= 0 && tx < (int)tmc.MapWidth && ty < (int)tmc.MapHeight) {
                    uint32_t idx = (uint32_t)ty * tmc.MapWidth + (uint32_t)tx;
                    if (idx < tmc.Tiles.size())
                        tmc.Tiles[idx] = erasing ? 0 : (uint32_t)glm::max(m_TilemapBrushValue, 0);
                }
            }
        } else if (m_TilePainting) {
            m_TilePainting = false;
            EntitySnapshot after = EntitySnapshot::Capture(m_SelectedEntity);
            if (m_TilePaintBefore.DiffersFrom(after))
                m_History.Push(CreateRef<EntityEditCommand>(m_SelectedEntity.GetUUID(), m_TilePaintBefore, after));
        }
    }

    ImGui::End();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — fim");
}
