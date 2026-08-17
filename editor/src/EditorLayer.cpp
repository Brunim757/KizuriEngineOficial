// IMGUI_DEFINE_MATH_OPERATORS precisa estar definido ANTES da primeira vez
// que imgui.h é incluído neste arquivo (o header guard do imgui.h impede
// que a macro tenha efeito numa inclusão posterior) — e EditorLayer.hpp já
// inclui imgui.h de forma transitiva via Kizuri.hpp -> ImGuiLayer.hpp, por
// isso o #define precisa vir antes até desse include.
#define IMGUI_DEFINE_MATH_OPERATORS
#include "EditorLayer.hpp"
#include <glad/gl.h>
#include <imgui.h>
#include <imgui_internal.h>
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/matrix_decompose.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "UI/Icons.hpp"
#include "UI/Panels/EditorPanel.hpp"
#include "UI/Panels/ProfilerPanel.hpp"
#include "UI/Panels/GameViewPanel.hpp"
#include "UI/Panels/MaterialEditorPanel.hpp"
#include "UI/Panels/ProjectSettingsPanel.hpp"
#include "UI/Panels/ParticleEditorPanel.hpp"
#include "UI/Panels/AnimatorPanel.hpp"
#include <kizuri/project/GameExporter.hpp>
#include <kizuri/core/Version.hpp>
#include "Updater.hpp"
#include <kizuri/scripting/ScriptEngine.hpp>
#include <kizuri/net/NetworkFacade.hpp>
#include <kizuri/core/CommandLineArgs.hpp>
#include "kizuri/renderer/TextRenderer.hpp"
#include <fstream>
#include <cfloat>
#include <cctype>
#include <cstdio>
#include <algorithm>
#include <filesystem>
#include <vector>
#include <cstring>
#include <nlohmann/json.hpp>

using namespace kizuri;

// Localiza um arquivo do Content Pack no disco. O zip da Release é
// "exe + content/" lado a lado, então o caminho certo é relativo ao
// diretório de trabalho ("content/..."); em builds de dev o conteúdo costuma
// ficar um nível acima ("../content/..."). Tenta os dois — nunca quebra.
static std::string FindContentFile(const std::string& relative) {
    std::string a = "content/" + relative;
    if (std::filesystem::exists(a)) return a;
    std::string b = "../content/" + relative;
    if (std::filesystem::exists(b)) return b;
    return "";
}

// Escolhe a fonte de um asset de DEMONSTRAÇÃO com prioridade:
//   1. Embutido no executável (kzres://) — sempre funciona, mesmo sem o
//      Content Pack no disco e mesmo se o usuário apagar o arquivo externo.
//   2. Content Pack no disco (content/ ou ../content) — asset mais rico
//      quando presente.
//   3. String vazia → a demo cai pros builtins (malhas procedurais).
// É isso que deixa o editor/engine "pesado de verdade": os padrões usados
// nas demonstrações vêm EMBUTIDOS, a função nunca quebra por falta de arquivo.
static std::string PickDemoAsset(const char* kzresName, const char* contentRelative) {
    if (HasEmbeddedResource(kzresName)) return std::string("kzres://") + kzresName;
    return FindContentFile(contentRelative);
}

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

EditorLayer::~EditorLayer() {
    // Evita crash no encerramento se um check/download de atualização
    // ainda estiver rodando na thread.
    if (m_UpdateThread.joinable()) m_UpdateThread.join();
}

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

    // Projetos recentes pro hub (KizuriRecents.json no diretório de trabalho).
    LoadRecentProjects();

    // Configurações gráficas (settings.json no diretório de trabalho) —
    // carrega antes de qualquer render e aplica VSync pra janela.
    LoadGraphicsSettingsFromDisk();

    // ---- Painéis dockáveis (Profiler, Game View, Material, Animator, Settings) ----
    // Cada painel é uma classe própria em UI/Panels; aqui só os criamos e
    // ligamos o contexto compartilhado. Nenhuma lógica de cena entra neles.
    m_PanelContext = std::make_unique<EditorContext>();
    m_PanelContext->Graphics = &m_GraphicsSettings;
    m_PanelContext->EditorCamFlySpeed = &m_EditorCamFlySpeed;
    m_PanelContext->EditorCamSensitivity = &m_EditorCamSensitivity;
    m_PanelContext->GizmoSnapTranslation = &m_GizmoSnapTranslation;
    m_PanelContext->GizmoSnapRotation = &m_GizmoSnapRotation;
    m_PanelContext->AutoCompileOnPlay = &m_AutoCompileOnPlay;
    m_PanelContext->ShowColliders = &m_ShowColliders;
    m_PanelContext->SelectEntity = [this](Entity e) { m_SelectedEntity = e; AutoSwitchViewportMode(); };
    m_PanelContext->TogglePlay = [this]() { (m_SceneState == SceneState::Edit) ? OnScenePlay() : OnSceneStop(); };
    m_PanelContext->RevealInContentBrowser = [this](const std::string& p) { RevealFileInContentBrowser(p); };

    auto makePanel = [&](std::unique_ptr<EditorPanel> p) { m_Panels.push_back(std::move(p)); };
    makePanel(std::make_unique<ProfilerPanel>(*m_PanelContext));
    makePanel(std::make_unique<GameViewPanel>(*m_PanelContext));
    makePanel(std::make_unique<MaterialEditorPanel>(*m_PanelContext));
    makePanel(std::make_unique<ParticleEditorPanel>(*m_PanelContext));
    makePanel(std::make_unique<AnimatorPanel>(*m_PanelContext));
    makePanel(std::make_unique<ProjectSettingsPanel>(*m_PanelContext));

    // Painéis que fazem sentido já abertos no layout padrão.
    m_Panels[0]->SetVisible(true);  // Profiler
    m_Panels[1]->SetVisible(true);  // Game View
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

    // O conteúdo padrão respeita o MODO do projeto ativo — 2D ganha câmera
    // ortográfica + sprites + física; 3D ganha câmera de perspectiva + cubo.
    // Isso é o que faz um jogo 100% 2D (ou 100% 3D) rodar de verdade no Play:
    // o Play renderiza o passe 2D só se a cena tem câmera primária ortográfica
    // e o passe 3D só se tem câmera de perspectiva.
    ProjectMode mode = ProjectMode::ThreeD;
    auto& project = Project::GetActive();
    if (project) mode = project->GetConfig().DefaultMode;

    Entity camera = m_ActiveScene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Primary = true;

    if (mode == ProjectMode::TwoD) {
        cc.Type = CameraComponent::ProjectionType::Orthographic2D;
        cc.OrthoSize = 10.0f;
        camera.GetComponent<TransformComponent>().Translation = { 0.0f, 0.0f, 0.0f };

        // Chão 2D com física (Box2D) pra começar.
        Entity ground = m_ActiveScene->CreateEntity("Chão");
        auto& gs = ground.AddComponent<SpriteRendererComponent>();
        gs.Color = { 0.16f, 0.17f, 0.2f, 1.0f };
        auto& gt = ground.GetComponent<TransformComponent>();
        gt.Translation = { 0.0f, -4.0f, 0.0f };
        gt.Scale = { 12.0f, 1.0f, 1.0f };
        ground.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Static;
        ground.AddComponent<BoxCollider2DComponent>().Size = { 12.0f, 1.0f };

        // Caixa que cai (dinâmica).
        Entity box = m_ActiveScene->CreateEntity("Caixa");
        auto& bs = box.AddComponent<SpriteRendererComponent>();
        bs.Color = { 0.85f, 0.25f, 0.3f, 1.0f };
        auto& bt = box.GetComponent<TransformComponent>();
        bt.Translation = { 0.0f, 3.0f, 0.0f };
        bt.Scale = { 1.0f, 1.0f, 1.0f };
        box.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
        auto& bcol = box.AddComponent<BoxCollider2DComponent>();
        bcol.Size = { 1.0f, 1.0f };

        m_SelectedEntity = box;
    } else {
        cc.Type = CameraComponent::ProjectionType::Perspective3D;
        auto& camTransform = camera.GetComponent<TransformComponent>();
        camTransform.Translation = { 0.0f, 2.0f, 6.0f };
        camTransform.Rotation = { glm::radians(-15.0f), 0.0f, 0.0f }; // pitch, yaw

        Entity cube = m_ActiveScene->CreateEntity("Cubo de Exemplo");
        auto& mr = cube.AddComponent<MeshRendererComponent>();
        mr.MeshAsset = Mesh::CreateCube();
        mr.MeshMaterial.Albedo = { 0.3f, 0.55f, 0.85f };

        Entity sun = m_ActiveScene->CreateEntity("Sol");
        sun.AddComponent<LightComponent>().Type = LightType::Directional;

        m_SelectedEntity = cube;
    }
}

// Cena de demonstração 3D — showcase: Fox esquelético animado, DamagedHelmet
// PBR, primitivas, HDRI de céu e fog. Os assets padrão vêm EMBUTIDOS no
// executável (kzres:// — a CI injeta Fox + DamagedHelmet no binário); o
// Content Pack no disco só é preferido quando presente (asset mais rico).
// Sem nenhum dos dois, cai nos builtins (malhas procedurais + céu embutido).
void EditorLayer::CreateDemoScene3D() {
    if (m_SceneState != SceneState::Edit) return;

    m_ActiveScene = CreateRef<Scene>("Demonstração 3D");
    m_ScenePath.clear();
    m_SelectedEntity = {};

    Entity camera = m_ActiveScene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Perspective3D;
    cc.PerspectiveFOV = 50.0f;
    auto& camT = camera.GetComponent<TransformComponent>();
    camT.Translation = { 0.0f, 2.6f, 7.5f };
    camT.Rotation = { glm::radians(-12.0f), 0.0f, 0.0f };

    Entity sun = m_ActiveScene->CreateEntity("Sol");
    auto& lc = sun.AddComponent<LightComponent>();
    lc.Type = LightType::Directional;
    lc.Color = { 1.0f, 0.95f, 0.85f };
    lc.Intensity = 2.0f;
    sun.GetComponent<TransformComponent>().Rotation = { glm::radians(50.0f), glm::radians(30.0f), 0.0f };

    Entity light2 = m_ActiveScene->CreateEntity("Luz de Preenchimento");
    auto& lc2 = light2.AddComponent<LightComponent>();
    lc2.Type = LightType::Directional;
    lc2.Color = { 0.35f, 0.45f, 0.8f };
    lc2.Intensity = 0.35f;
    light2.GetComponent<TransformComponent>().Rotation = { glm::radians(30.0f), glm::radians(-140.0f), 0.0f };

    Entity ground = m_ActiveScene->CreateEntity("Chão");
    auto& gm = ground.AddComponent<MeshRendererComponent>();
    gm.MeshSource = "builtin:plane";
    gm.MeshAsset = Mesh::FromSource(gm.MeshSource);
    gm.MeshMaterial.Albedo = { 0.15f, 0.16f, 0.19f };
    gm.MeshMaterial.Roughness = 0.9f;
    ground.GetComponent<TransformComponent>().Scale = { 12.0f, 1.0f, 12.0f };

    // Piso espelhado (metal polido) — reflete o ambiente (IBL) e, com o SSR
    // 3.3-safe ligado, reflete as meshes da cena (reflexos por raio).
    Entity mirrorFloor = m_ActiveScene->CreateEntity("Piso Espelhado");
    auto& mm = mirrorFloor.AddComponent<MeshRendererComponent>();
    mm.MeshSource = "builtin:plane";
    mm.MeshAsset = Mesh::FromSource(mm.MeshSource);
    mm.MeshMaterial.Albedo = { 0.02f, 0.02f, 0.025f };
    mm.MeshMaterial.Metallic = 1.0f;
    mm.MeshMaterial.Roughness = 0.04f;
    auto& mt = mirrorFloor.GetComponent<TransformComponent>();
    mt.Translation = { 0.0f, 0.012f, 0.0f };
    mt.Scale = { 6.0f, 1.0f, 6.0f };

    // Céu: atmosférico procedural por padrão (estável). Prioridade:
    // Content Pack (HDRI rico) → EMBUTIDO (kzres://skies/sky_gradient.hdr,
    // sempre disponível) → procedural. Se apagarem o arquivo externo, o
    // embutido assume — o céu nunca deixa de funcionar.
    Renderer3D::SetEnvironmentHDRIPath("");
    std::string hdri = FindContentFile("skies/qwantani_puresky_1k.hdr");
    if (!hdri.empty()) {
        Renderer3D::SetEnvironmentHDRIPath(hdri);
        KZ_CORE_INFO("Demo 3D: céu HDRI do content pack carregado ({0}).", hdri);
    } else if (HasEmbeddedResource("skies/sky_gradient.hdr")) {
        Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");
        KZ_CORE_INFO("Demo 3D: céu EMBUTIDO (kzres://skies/sky_gradient.hdr).");
    }

    // Cubo EMBUTIDO (kzres://models/Cube.glb) — sempre carrega, mesmo sem o
    // Content Pack: prova do conteúdo embutido no executável.
    Entity embeddedCube = m_ActiveScene->CreateEntity("Cubo Embutido (kzres)");
    auto& ecm = embeddedCube.AddComponent<MeshRendererComponent>();
    ecm.MeshSource = "kzres://models/Cube.glb";
    ecm.MeshAsset = Mesh::FromSource(ecm.MeshSource);
    ecm.MeshMaterial.Albedo = { 0.85f, 0.35f, 0.3f };
    ecm.MeshMaterial.Roughness = 0.4f;
    embeddedCube.GetComponent<TransformComponent>().Translation = { -2.6f, 0.5f, 1.6f };

    // Fox esquelético animado (skinning) — o coração do v0.3. EMBUTIDO na CI
    // (kzres://models/Fox.glb), senão Content Pack, senão não carrega (a cena
    // segue completa com builtins).
    std::string foxPath = PickDemoAsset("models/Fox.glb", "models/Fox.glb");
    if (!foxPath.empty()) {
        Entity fox = m_ActiveScene->CreateEntity("Fox (animado)");
        auto& fm = fox.AddComponent<MeshRendererComponent>();
        fm.MeshSource = foxPath;
        fm.MeshAsset = Mesh::FromSource(foxPath);
        fm.MeshMaterial = Mesh::ExtractMaterialFromGLTF(foxPath);
        auto& fa = fox.AddComponent<AnimatorComponent>();
        fa.MeshPath = foxPath;
        fa.Skin = SkinData::CreateFromGLTF(foxPath);
        fa.ClipName = "Survey";
        auto& ft = fox.GetComponent<TransformComponent>();
        ft.Translation = { -1.8f, 0.0f, 0.5f };
        ft.Scale = { 1.5f, 1.5f, 1.5f };
        KZ_CORE_INFO("Demo 3D: Fox carregado de '{0}' ({1} juntas, {2} animações).",
                     foxPath, fa.Skin ? fa.Skin->Joints.size() : 0, fa.Skin ? fa.Skin->Clips.size() : 0);
    }

    // DamagedHelmet PBR num pedestal — também embutido na CI.
    std::string helmetPath = PickDemoAsset("models/DamagedHelmet.glb", "models/DamagedHelmet.glb");
    if (!helmetPath.empty()) {
        Entity pedestal = m_ActiveScene->CreateEntity("Pedestal");
        auto& pm = pedestal.AddComponent<MeshRendererComponent>();
        pm.MeshSource = "builtin:cylinder";
        pm.MeshAsset = Mesh::FromSource(pm.MeshSource);
        pm.MeshMaterial.Albedo = { 0.1f, 0.1f, 0.12f };
        pm.MeshMaterial.Roughness = 0.6f;
        auto& pt = pedestal.GetComponent<TransformComponent>();
        pt.Translation = { 2.2f, 0.25f, 0.0f };
        pt.Scale = { 0.7f, 0.5f, 0.7f };

        Entity helmet = m_ActiveScene->CreateEntity("Capacete (PBR)");
        auto& hm = helmet.AddComponent<MeshRendererComponent>();
        hm.MeshSource = helmetPath;
        hm.MeshAsset = Mesh::FromSource(helmetPath);
        hm.MeshMaterial = Mesh::ExtractMaterialFromGLTF(helmetPath);
        auto& ht = helmet.GetComponent<TransformComponent>();
        ht.Translation = { 2.2f, 0.8f, 0.0f };
        ht.Scale = { 1.4f, 1.4f, 1.4f };
    }

    // Primitivas de exemplo (novas builtins).
    Entity torus = m_ActiveScene->CreateEntity("Torus Metálico");
    auto& tm = torus.AddComponent<MeshRendererComponent>();
    tm.MeshSource = "builtin:torus";
    tm.MeshAsset = Mesh::FromSource(tm.MeshSource);
    tm.MeshMaterial.Albedo = { 0.75f, 0.55f, 0.2f };
    tm.MeshMaterial.Metallic = 1.0f;
    tm.MeshMaterial.Roughness = 0.25f;
    auto& tt = torus.GetComponent<TransformComponent>();
    tt.Translation = { 0.0f, 1.0f, -2.2f };
    tt.Rotation = { glm::radians(70.0f), 0.0f, 0.0f };

    Entity neon = m_ActiveScene->CreateEntity("Esfera Emissiva");
    auto& nm = neon.AddComponent<MeshRendererComponent>();
    nm.MeshSource = "builtin:sphere";
    nm.MeshAsset = Mesh::FromSource(nm.MeshSource);
    nm.MeshMaterial.Albedo = { 0.02f, 0.02f, 0.05f };
    nm.MeshMaterial.Emissive = { 0.1f, 0.6f, 1.0f };
    nm.MeshMaterial.EmissiveStrength = 6.0f; // alimenta o bloom
    neon.GetComponent<TransformComponent>().Translation = { -2.6f, 0.8f, -1.5f };

    // Fog exponencial dá atmosfera (desligável nas Configurações Gráficas).
    auto settings = Renderer3D::GetGraphicsSettings();
    settings.FogEnabled = true;
    settings.FogDensity = 0.02f;
    Renderer3D::SetGraphicsSettings(settings);

    m_ViewportMode = ViewportMode::Mode3D;
    m_EditorCamPos = { 0.0f, 2.6f, 7.5f };
    m_EditorCamYaw = -90.0f;
    m_EditorCamPitch = -12.0f;
    m_SelectedEntity = sun;
    KZ_CORE_INFO("Cena de demonstração 3D criada.");
}

// ---------------------------------------------------------------------------
// Demo de IA (pilar AAA v0.34): arena com NavGrid + obstáculos, um jogador
// (WASD) e 3 inimigos com EnemyAIComponent (patrulha → persegue → ataca)
// usando NavAgent. Aperte Play e fuja dos inimigos.
// ---------------------------------------------------------------------------

// Script do jogador da demo — move no plano XZ e gira pro lado do movimento.
class DemoPlayerMove : public NativeScript {
public:
    void OnUpdate(Timestep ts) override {
        auto& tc = GetComponent<TransformComponent>();
        const float speed = 5.5f;
        glm::vec2 input{ 0.0f };
        if (Input::IsKeyPressed(Key::A)) input.x -= 1.0f;
        if (Input::IsKeyPressed(Key::D)) input.x += 1.0f;
        if (Input::IsKeyPressed(Key::W)) input.y += 1.0f;
        if (Input::IsKeyPressed(Key::S)) input.y -= 1.0f;
        if (glm::length(input) > 0.01f) {
            input = glm::normalize(input);
            tc.Translation.x += input.x * speed * (float)ts;
            tc.Translation.z += input.y * speed * (float)ts;
            tc.Rotation.y = std::atan2(input.x, input.y);
        }
    }
};

// Script do inimigo da demo — trata o ataque disparado pelo EnemyAIComponent.
class DemoEnemyScript : public NativeScript {
public:
    void OnEnemyAttack(float amount) override {
        KZ_CORE_INFO("Inimigo {0} atacou! (dano {1})", GetEntity().GetName(), amount);
    }
};

void EditorLayer::CreateDemoSceneAI() {
    if (m_SceneState != SceneState::Edit) return;

    m_ActiveScene = CreateRef<Scene>("Demonstração IA");
    m_ScenePath.clear();
    m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode3D;

    // Registra os scripts da demo — BindByName sobrevive à cópia JSON do Play.
    ScriptEngine::GetRegistry().Register<DemoPlayerMove>("DemoPlayerMove");
    ScriptEngine::GetRegistry().Register<DemoEnemyScript>("DemoEnemyScript");

    auto& scene = m_ActiveScene;

    // Câmera em ângulo sobre a arena, seguindo o jogador.
    Entity camera = scene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Perspective3D;
    cc.Primary = true;
    cc.PerspectiveFOV = 50.0f;
    auto& camT = camera.GetComponent<TransformComponent>();
    camT.Translation = { 0.0f, 12.0f, 12.0f };
    camT.Rotation = { glm::radians(-40.0f), 0.0f, 0.0f };
    auto& cf = camera.AddComponent<CameraFollowComponent>();
    cf.TargetName = "Jogador";
    cf.Offset = { 0.0f, 14.0f, -14.0f };
    cf.UseWorldOffset = true;
    cf.FollowRotation = false;

    Entity sun = scene->CreateEntity("Sol");
    auto& sl = sun.AddComponent<LightComponent>();
    sl.Type = LightType::Directional;
    sl.Color = { 1.0f, 0.95f, 0.85f };
    sl.Intensity = 1.8f;
    sun.GetComponent<TransformComponent>().Rotation = { glm::radians(55.0f), glm::radians(30.0f), 0.0f };

    // Chão.
    Entity ground = scene->CreateEntity("Chão");
    auto& gm = ground.AddComponent<MeshRendererComponent>();
    gm.MeshSource = "builtin:plane";
    gm.MeshAsset = Mesh::FromSource(gm.MeshSource);
    gm.MeshMaterial.Albedo = { 0.20f, 0.22f, 0.26f };
    gm.MeshMaterial.Roughness = 0.9f;
    ground.GetComponent<TransformComponent>().Scale = { 30.0f, 1.0f, 30.0f };

    // Grade de navegação (cobre ±25 do centro).
    Entity navGrid = scene->CreateEntity("NavGrade");
    auto& ng = navGrid.AddComponent<NavGridComponent>();
    ng.Origin = { -25.0f, 0.0f, -25.0f };
    ng.Width = 50;
    ng.Depth = 50;
    ng.CellSize = 1.0f;

    // Obstáculos (visual + bloco da navegação).
    const glm::vec3 obstaclePos[6] = {
        { -6.0f, 0.0f, -4.0f }, { 5.0f, 0.0f, -6.0f }, { 0.0f, 0.0f, 3.0f },
        { -8.0f, 0.0f, 6.0f },  { 7.0f, 0.0f, 5.0f },  { -2.0f, 0.0f, -9.0f },
    };
    for (int i = 0; i < 6; ++i) {
        Entity ob = scene->CreateEntity("Obstáculo " + std::to_string(i + 1));
        auto& om = ob.AddComponent<MeshRendererComponent>();
        om.MeshSource = "builtin:cube";
        om.MeshAsset = Mesh::FromSource(om.MeshSource);
        om.MeshMaterial.Albedo = { 0.55f, 0.32f, 0.24f };
        om.MeshMaterial.Roughness = 0.6f;
        auto& ot = ob.GetComponent<TransformComponent>();
        ot.Translation = obstaclePos[i];
        ot.Scale = { 2.0f, 2.5f, 2.0f };
        ob.AddComponent<NavObstacleComponent>(); // meio-vão vazio = usa a escala
    }

    // Jogador.
    Entity player = scene->CreateEntity("Jogador");
    auto& pm = player.AddComponent<MeshRendererComponent>();
    pm.MeshSource = "builtin:cube";
    pm.MeshAsset = Mesh::FromSource(pm.MeshSource);
    pm.MeshMaterial.Albedo = { 0.25f, 0.6f, 0.9f };
    pm.MeshMaterial.Roughness = 0.4f;
    auto& pt = player.GetComponent<TransformComponent>();
    pt.Translation = { -8.0f, 0.5f, -2.0f };
    pt.Scale = { 1.0f, 1.0f, 1.0f };
    auto& pns = player.AddComponent<NativeScriptComponent>();
    pns.BindByName("DemoPlayerMove");

    // Inimigos: patrulham; perseguem quando veem o jogador; atacam no alcance.
    const glm::vec3 enemyPos[3] = { { 8.0f, 0.5f, 8.0f }, { -9.0f, 0.5f, -8.0f }, { 9.0f, 0.5f, -8.0f } };
    const glm::vec3 patrolPoints[3][3] = {
        { { 10.0f, 0.5f, 8.0f }, { -6.0f, 0.5f, 8.0f }, { 10.0f, 0.5f, -6.0f } },
        { { -10.0f, 0.5f, -6.0f }, { 10.0f, 0.5f, -6.0f }, { -10.0f, 0.5f, 10.0f } },
        { { 10.0f, 0.5f, 10.0f }, { -10.0f, 0.5f, -10.0f }, { 10.0f, 0.5f, -10.0f } },
    };
    for (int i = 0; i < 3; ++i) {
        Entity enemy = scene->CreateEntity("Inimigo " + std::to_string(i + 1));
        auto& em = enemy.AddComponent<MeshRendererComponent>();
        em.MeshSource = "builtin:sphere";
        em.MeshAsset = Mesh::FromSource(em.MeshSource);
        em.MeshMaterial.Albedo = { 0.85f, 0.25f, 0.2f };
        em.MeshMaterial.Roughness = 0.35f;
        auto& et = enemy.GetComponent<TransformComponent>();
        et.Translation = enemyPos[i];
        et.Scale = { 1.1f, 1.1f, 1.1f };

        auto& na = enemy.AddComponent<NavAgentComponent>();
        na.Speed = 3.2f + (float)i * 0.5f;
        na.TurnSpeed = 6.0f;

        auto& ai = enemy.AddComponent<EnemyAIComponent>();
        ai.SightRange = 14.0f;
        ai.LoseRange = 20.0f;
        ai.ChaseRange = 1.6f;
        ai.AttackCooldown = 1.0f;
        ai.AttackDamage = 10.0f;
        ai.PatrolWait = 1.0f;
        ai.TargetTag = "Jogador";
        for (int p = 0; p < 3; ++p) ai.PatrolPoints.push_back(patrolPoints[i][p]);

        auto& ens = enemy.AddComponent<NativeScriptComponent>();
        ens.BindByName("DemoEnemyScript");
    }

    // Rótulo 2D de instrução.
    Entity label = scene->CreateEntity("Instruções");
    auto& lt = label.AddComponent<TextComponent>();
    lt.Text = "Demo IA — WASD pra fugir dos inimigos (Eles patrulham, perseguem e atacam)";
    lt.FontSize = 16.0f;
    lt.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    label.GetComponent<TransformComponent>().Translation = { -9.0f, 5.5f, 0.0f };

    m_EditorCamPos = { 0.0f, 18.0f, 18.0f };
    m_EditorCamYaw = -90.0f;
    m_EditorCamPitch = -40.0f;
    m_SelectedEntity = player;
    KZ_CORE_INFO("Cena de demonstração IA criada (NavGrid + NavAgent + EnemyAI).");
}

// ---------------------------------------------------------------------------
// Demo de REDE (pilar AAA v0.34): duas instâncias jogando na mesma máquina.
// 1ª instância = host (move o cubo com WASD); 2ª instância = cliente (vê o
// cubo do host se mover). Aperte Play na 1ª e abra OUTRA janela do editor
// (ou o KizuriGame com --net-connect) pra ser o cliente.
// ---------------------------------------------------------------------------

// Script do cubo de rede: no host controla e ENVIA o transform; no cliente
// RECEBE e aplica. Mesmo prefab/cena roda nos dois lados.
class DemoNetCube : public NativeScript {
public:
    void OnCreate() override {
        m_Host = kizuri::Network::Host(26000);
        if (!m_Host) {
            KZ_CORE_INFO("Porta 26000 ocupada — conectando em 127.0.0.1 (cliente).");
            kizuri::Network::Connect("127.0.0.1", 26000);
        } else {
            KZ_CORE_INFO("Demo de rede: HOST na porta 26000. Abra outra instância (cliente).");
        }
    }

    void OnUpdate(Timestep ts) override {
        auto& tc = GetComponent<TransformComponent>();

        // Processa eventos (os dois lados).
        kizuri::net::Event ev;
        while (kizuri::Network::PollEvent(ev)) {
            if (ev.Type == kizuri::net::EventType::Connect)
                KZ_CORE_INFO("Rede: jogador {0} conectou!", ev.Peer);
            else if (ev.Type == kizuri::net::EventType::Data && ev.Data.size() >= kizuri::net::kNetTransformSize) {
                kizuri::net::NetTransform t = kizuri::net::ReadNetTransform(ev.Data.data());
                if (!m_Host) { // cliente: aplica o estado do host
                    tc.Translation = { t.X, t.Y, t.Z };
                    tc.Rotation.y = t.Yaw;
                }
            }
        }

        // Host: controla com WASD e envia o transform (cadência ~10Hz).
        if (m_Host) {
            const float speed = 4.0f;
            glm::vec2 input{ 0.0f };
            if (Input::IsKeyPressed(Key::A)) input.x -= 1.0f;
            if (Input::IsKeyPressed(Key::D)) input.x += 1.0f;
            if (Input::IsKeyPressed(Key::W)) input.y += 1.0f;
            if (Input::IsKeyPressed(Key::S)) input.y -= 1.0f;
            if (glm::length(input) > 0.01f) {
                input = glm::normalize(input);
                tc.Translation.x += input.x * speed * (float)ts;
                tc.Translation.z += input.y * speed * (float)ts;
                tc.Rotation.y = std::atan2(input.x, input.y);
            }
            m_SendTimer -= (float)ts;
            if (m_SendTimer <= 0.0f) {
                m_SendTimer = 0.1f;
                kizuri::net::NetTransform t;
                t.EntityId = 1;
                t.X = tc.Translation.x; t.Y = tc.Translation.y; t.Z = tc.Translation.z;
                t.Yaw = tc.Rotation.y;
                uint8_t buf[kizuri::net::kNetTransformSize];
                kizuri::net::WriteNetTransform(t, buf);
                kizuri::Network::Send(1, buf, kizuri::net::kNetTransformSize);
            }
        }
    }

private:
    bool m_Host = false;
    float m_SendTimer = 0.0f;
};

void EditorLayer::CreateDemoSceneNet() {
    if (m_SceneState != SceneState::Edit) return;

    m_ActiveScene = CreateRef<Scene>("Demonstração Rede");
    m_ScenePath.clear();
    m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode3D;

    ScriptEngine::GetRegistry().Register<DemoNetCube>("DemoNetCube");

    auto& scene = m_ActiveScene;

    Entity camera = scene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Perspective3D;
    cc.Primary = true;
    cc.PerspectiveFOV = 55.0f;
    auto& camT = camera.GetComponent<TransformComponent>();
    camT.Translation = { 0.0f, 6.0f, 8.0f };
    camT.Rotation = { glm::radians(-35.0f), 0.0f, 0.0f };
    auto& cf = camera.AddComponent<CameraFollowComponent>();
    cf.TargetName = "Cubo de Rede";
    cf.Offset = { 0.0f, 5.0f, -7.0f };
    cf.UseWorldOffset = true;
    cf.FollowRotation = false;

    Entity sun = scene->CreateEntity("Sol");
    auto& sl = sun.AddComponent<LightComponent>();
    sl.Type = LightType::Directional;
    sl.Color = { 1.0f, 0.95f, 0.85f };
    sl.Intensity = 1.8f;
    sun.GetComponent<TransformComponent>().Rotation = { glm::radians(55.0f), glm::radians(30.0f), 0.0f };

    Entity ground = scene->CreateEntity("Chão");
    auto& gm = ground.AddComponent<MeshRendererComponent>();
    gm.MeshSource = "builtin:plane";
    gm.MeshAsset = Mesh::FromSource(gm.MeshSource);
    gm.MeshMaterial.Albedo = { 0.18f, 0.2f, 0.24f };
    gm.MeshMaterial.Roughness = 0.9f;
    ground.GetComponent<TransformComponent>().Scale = { 20.0f, 1.0f, 20.0f };

    Entity cube = scene->CreateEntity("Cubo de Rede");
    auto& cm = cube.AddComponent<MeshRendererComponent>();
    cm.MeshSource = "builtin:cube";
    cm.MeshAsset = Mesh::FromSource(cm.MeshSource);
    cm.MeshMaterial.Albedo = { 0.2f, 0.7f, 0.9f };
    cm.MeshMaterial.Roughness = 0.35f;
    cube.GetComponent<TransformComponent>().Translation = { 0.0f, 0.5f, 0.0f };
    auto& cns = cube.AddComponent<NativeScriptComponent>();
    cns.BindByName("DemoNetCube");

    Entity label = scene->CreateEntity("Instruções");
    auto& lt = label.AddComponent<TextComponent>();
    lt.Text = "Demo Rede — 1ª janela = HOST (WASD) · 2ª janela = cliente (vê o cubo)";
    lt.FontSize = 15.0f;
    lt.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    label.GetComponent<TransformComponent>().Translation = { -8.0f, 4.0f, 0.0f };

    m_EditorCamPos = { 0.0f, 6.0f, 8.0f };
    m_EditorCamYaw = -90.0f;
    m_EditorCamPitch = -35.0f;
    m_SelectedEntity = cube;
    KZ_CORE_INFO("Cena de demonstração Rede criada (host/cliente na porta 26000).");
}

// Moeda da demo completa: gira, e quando o jogador chega perto pontua e
// some (destruição deferida é segura dentro do OnUpdate — ver Scene.cpp).
class DemoCoinScript : public NativeScript {
public:
    static int s_Score;
    void OnUpdate(Timestep ts) override {
        auto& tc = GetComponent<TransformComponent>();
        tc.Rotation.y += 2.2f * (float)ts;
        Entity player = GetScene()->FindEntityByName("Jogador");
        if (!player) return;
        auto& pt = player.GetComponent<TransformComponent>();
        glm::vec2 me(tc.Translation.x, tc.Translation.z);
        glm::vec2 pp(pt.Translation.x, pt.Translation.z);
        if (glm::distance(me, pp) < 1.6f) {
            ++s_Score;
            Entity placar = GetScene()->FindEntityByName("Placar");
            if (placar) {
                const std::string msg = s_Score >= 8
                    ? "PONTOS 8/8 — VOCÊ VENCEU! (Play de novo pra rejogar)"
                    : "Pontos: " + std::to_string(s_Score) + " / 8  ·  WASD pra coletar";
                placar.GetComponent<TextComponent>().Text = msg;
            }
            DestroyEntity();
        }
    }
};
int DemoCoinScript::s_Score = 0;

void EditorLayer::CreateDemoSceneGame() {
    if (m_SceneState != SceneState::Edit) return;

    m_ActiveScene = CreateRef<Scene>("Demo Completa");
    m_ScenePath.clear();
    m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode3D;

    ScriptEngine::GetRegistry().Register<DemoPlayerMove>("DemoPlayerMove");
    ScriptEngine::GetRegistry().Register<DemoEnemyScript>("DemoEnemyScript");
    ScriptEngine::GetRegistry().Register<DemoCoinScript>("DemoCoinScript");
    DemoCoinScript::s_Score = 0;

    auto& scene = m_ActiveScene;

    Entity camera = scene->CreateEntity("Câmera Principal");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Perspective3D;
    cc.Primary = true;
    cc.PerspectiveFOV = 55.0f;
    auto& camT = camera.GetComponent<TransformComponent>();
    camT.Translation = { 0.0f, 15.0f, 15.0f };
    camT.Rotation = { glm::radians(-45.0f), 0.0f, 0.0f };
    auto& cf = camera.AddComponent<CameraFollowComponent>();
    cf.TargetName = "Jogador";
    cf.Offset = { 0.0f, 16.0f, -16.0f };
    cf.UseWorldOffset = true;
    cf.FollowRotation = false;

    Entity sun = scene->CreateEntity("Sol");
    auto& sl = sun.AddComponent<LightComponent>();
    sl.Type = LightType::Directional;
    sl.Color = { 1.0f, 0.95f, 0.85f };
    sl.Intensity = 1.8f;
    sun.GetComponent<TransformComponent>().Rotation = { glm::radians(55.0f), glm::radians(30.0f), 0.0f };

    Entity ground = scene->CreateEntity("Chão");
    auto& gm = ground.AddComponent<MeshRendererComponent>();
    gm.MeshSource = "builtin:plane";
    gm.MeshAsset = Mesh::FromSource(gm.MeshSource);
    gm.MeshMaterial.Albedo = { 0.18f, 0.20f, 0.24f };
    gm.MeshMaterial.Roughness = 0.9f;
    ground.GetComponent<TransformComponent>().Scale = { 22.0f, 1.0f, 22.0f };

    // Jogador (mesmo script WASD da demo IA).
    Entity player = scene->CreateEntity("Jogador");
    auto& pm = player.AddComponent<MeshRendererComponent>();
    pm.MeshSource = "builtin:cube";
    pm.MeshAsset = Mesh::FromSource(pm.MeshSource);
    pm.MeshMaterial.Albedo = { 0.25f, 0.6f, 0.9f };
    pm.MeshMaterial.Roughness = 0.4f;
    player.GetComponent<TransformComponent>().Translation = { 0.0f, 0.5f, 6.0f };
    player.AddComponent<NativeScriptComponent>().BindByName("DemoPlayerMove");

    // 8 moedas douradas (cilindros girando).
    const glm::vec3 coinPos[8] = {
        { -6.0f, 0.5f, -6.0f }, { 6.0f, 0.5f, -6.0f }, { 6.0f, 0.5f, 6.0f }, { -6.0f, 0.5f, 6.0f },
        { 0.0f, 0.5f, -8.0f },  { 8.0f, 0.5f, 0.0f },  { 0.0f, 0.5f, 8.0f },  { -8.0f, 0.5f, 0.0f },
    };
    for (int i = 0; i < 8; ++i) {
        Entity coin = scene->CreateEntity("Moeda " + std::to_string(i + 1));
        auto& cim = coin.AddComponent<MeshRendererComponent>();
        cim.MeshSource = "builtin:cylinder";
        cim.MeshAsset = Mesh::FromSource(cim.MeshSource);
        cim.MeshMaterial.Albedo = { 1.0f, 0.75f, 0.15f };
        cim.MeshMaterial.Metallic = 0.8f;
        cim.MeshMaterial.Roughness = 0.35f;
        auto& ct = coin.GetComponent<TransformComponent>();
        ct.Translation = coinPos[i];
        ct.Scale = { 0.9f, 0.15f, 0.9f };
        coin.AddComponent<NativeScriptComponent>().BindByName("DemoCoinScript");
    }

    // 2 inimigos guardando as moedas (patrulham, perseguem, atacam).
    const glm::vec3 enePos[2] = { { -4.0f, 0.5f, 4.0f }, { 4.0f, 0.5f, -4.0f } };
    const glm::vec3 patl[2][2] = {
        { { -6.0f, 0.5f, 6.0f }, { -2.0f, 0.5f, 2.0f } },
        { { 6.0f, 0.5f, -6.0f }, { 2.0f, 0.5f, -2.0f } },
    };
    for (int i = 0; i < 2; ++i) {
        Entity enemy = scene->CreateEntity("Inimigo " + std::to_string(i + 1));
        auto& em = enemy.AddComponent<MeshRendererComponent>();
        em.MeshSource = "builtin:sphere";
        em.MeshAsset = Mesh::FromSource(em.MeshSource);
        em.MeshMaterial.Albedo = { 0.85f, 0.22f, 0.18f };
        em.MeshMaterial.Roughness = 0.35f;
        auto& et = enemy.GetComponent<TransformComponent>();
        et.Translation = enePos[i];
        et.Scale = { 1.2f, 1.2f, 1.2f };
        auto& na = enemy.AddComponent<NavAgentComponent>();
        na.Speed = 3.4f + (float)i * 0.4f;
        na.TurnSpeed = 6.0f;
        auto& ai = enemy.AddComponent<EnemyAIComponent>();
        ai.SightRange = 13.0f;
        ai.LoseRange = 19.0f;
        ai.ChaseRange = 1.7f;
        ai.AttackCooldown = 1.2f;
        ai.AttackDamage = 10.0f;
        ai.PatrolWait = 0.8f;
        ai.TargetTag = "Jogador";
        for (int p = 0; p < 2; ++p) ai.PatrolPoints.push_back(patl[i][p]);
        enemy.AddComponent<NativeScriptComponent>().BindByName("DemoEnemyScript");
    }

    // Placar (HUD) + instruções (texto 2D sobre a cena).
    Entity placar = scene->CreateEntity("Placar");
    auto& ptxt = placar.AddComponent<TextComponent>();
    ptxt.Text = "Pontos: 0 / 8  ·  WASD pra coletar as moedas";
    ptxt.FontSize = 18.0f;
    ptxt.Color = { 1.0f, 0.85f, 0.3f, 1.0f };
    placar.GetComponent<TransformComponent>().Translation = { -12.0f, 6.5f, 0.0f };

    Entity label = scene->CreateEntity("Instruções");
    auto& lt = label.AddComponent<TextComponent>();
    lt.Text = "DEMO COMPLETA — colete as 8 moedas douradas; os inimigos patrulham, perseguem e atacam";
    lt.FontSize = 14.0f;
    lt.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    label.GetComponent<TransformComponent>().Translation = { -12.0f, 5.4f, 0.0f };

    m_EditorCamPos = { 0.0f, 15.0f, 15.0f };
    m_EditorCamYaw = -90.0f;
    m_EditorCamPitch = -45.0f;
    m_SelectedEntity = player;
    KZ_CORE_INFO("Demo completa criada — colete 8 moedas fugindo dos inimigos.");
}

// Cena de demonstração 2D — showcase do pipeline 2D: sprites, física Box2D
// (chão + caixas caindo), círculos, texto e UI (canvas + botão). Roda 100%
// no Play com a câmera ortográfica.
void EditorLayer::CreateDemoScene2D() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<Scene>("Demonstração 2D");
    m_ScenePath.clear();
    m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode2D;
    // Reseta a câmera de edição 2D pra enquadrar a demo (sem isso o pan/zoom
    // anterior deixa o viewport "fora da cena" e parece bugado).
    m_Editor2DZoom = 10.0f;
    m_Editor2DCamPos = { 0.0f, 0.0f };
    m_Editor2DFirstMouseLook = true;

    Entity camera = m_ActiveScene->CreateEntity("Câmera 2D");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true;
    cc.OrthoSize = 10.0f;

    // Fundo: z=0 (dentro do range ortográfico -1..1; z=-1 ficava no plano
    // near e podia ser recortado) + camada de ordenação mais baixa.
    Entity bg = m_ActiveScene->CreateEntity("Fundo");
    auto& bs = bg.AddComponent<SpriteRendererComponent>();
    bs.Color = { 0.10f, 0.11f, 0.14f, 1.0f };
    bs.SortingLayer = -5;
    auto& bt = bg.GetComponent<TransformComponent>();
    bt.Translation = { 0.0f, 0.0f, 0.0f };
    bt.Scale = { 40.0f, 25.0f, 1.0f };

    Entity ground = m_ActiveScene->CreateEntity("Chão");
    auto& gs = ground.AddComponent<SpriteRendererComponent>();
    gs.Color = { 0.28f, 0.32f, 0.38f, 1.0f };
    auto& gt = ground.GetComponent<TransformComponent>();
    gt.Translation = { 0.0f, -8.0f, 0.0f };
    gt.Scale = { 20.0f, 1.0f, 1.0f };
    ground.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Static;
    auto& gcol = ground.AddComponent<BoxCollider2DComponent>();
    gcol.Size = { 20.0f, 1.0f };

    // Caixas que caem (física Box2D de verdade no Play).
    for (int i = 0; i < 5; ++i) {
        Entity box = m_ActiveScene->CreateEntity("Caixa " + std::to_string(i + 1));
        auto& bxs = box.AddComponent<SpriteRendererComponent>();
        float t = (float)i / 4.0f;
        bxs.Color = { 0.3f + t * 0.6f, 0.35f, 0.85f, 1.0f };
        auto& bxt = box.GetComponent<TransformComponent>();
        bxt.Translation = { -6.0f + i * 2.8f, 2.0f + i * 1.2f, 0.0f };
        bxt.Rotation.z = glm::radians((float)(i * 18 - 36));
        box.AddComponent<Rigidbody2DComponent>().Type = Rigidbody2DComponent::BodyType::Dynamic;
        auto& bxc = box.AddComponent<BoxCollider2DComponent>();
        bxc.Size = { 1.0f, 1.0f };
    }

    // Moedas: círculos preenchidos (Thickness 1.0 = disco cheio; 0.9 era anel).
    for (int i = 0; i < 6; ++i) {
        Entity coin = m_ActiveScene->CreateEntity("Moeda " + std::to_string(i + 1));
        auto& cs = coin.AddComponent<CircleRendererComponent>();
        cs.Color = { 1.0f, 0.8f, 0.2f, 1.0f };
        cs.Thickness = 1.0f;
        coin.GetComponent<TransformComponent>().Translation = { -6.0f + i * 2.4f, 6.5f, 0.0f };
    }

    Entity title = m_ActiveScene->CreateEntity("Título");
    auto& tc = title.AddComponent<TextComponent>();
    tc.Text = "Demonstração 2D — aperte Play";
    tc.FontSize = 36.0f;
    tc.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    tc.SortingLayer = 5;
    title.GetComponent<TransformComponent>().Translation = { -8.0f, 8.4f, 0.0f };

    // UI: canvas + botão com texto (espaço de tela, 0,0 = centro).
    Entity canvas = m_ActiveScene->CreateEntity("Canvas");
    canvas.AddComponent<UICanvasComponent>();
    Entity button = m_ActiveScene->CreateEntity("Botão");
    auto& ur = button.AddComponent<UIRectComponent>();
    ur.Position = { 0.0f, -3.5f };
    ur.Size = { 4.5f, 1.2f };
    ur.Color = { 0.82f, 0.24f, 0.27f, 1.0f };
    button.AddComponent<UIButtonComponent>();
    auto& btext = button.AddComponent<TextComponent>();
    btext.Text = "Kizuri 2D!";
    btext.FontSize = 14.0f; // pixels de tela
    btext.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    button.SetParent(canvas);

    KZ_CORE_INFO("Cena de demonstração 2D criada.");
}

void EditorLayer::OnDetach() {
    // Não pode fechar o editor com a thread de build do C# viva (o
    // destruidor de std::thread chamaria std::terminate). Espera terminar.
    if (m_PlayBuildActive || m_PlayBuildThread.joinable()) {
        m_PlayBuildCancelled = true;
        if (m_PlayBuildThread.joinable()) m_PlayBuildThread.join();
    }
}

void EditorLayer::OnUpdate(Timestep ts) {
    KZ_CORE_TRACE("EditorLayer::OnUpdate — início (viewport {0}x{1})", m_ViewportSize.x, m_ViewportSize.y);

    // Checagem automática de atualização no início (a thread dorme ~2s pra
    // não grudar o startup).
    if (m_UpdateCheckOnStartup && !m_UpdateStartupCheckDone) {
        m_UpdateStartupCheckDone = true;
        StartUpdateCheck();
    }

    // FPS suavizado pro Profiler do viewport (média móvel exponencial).
    if ((float)ts > 0.0f) {
        float inst = 1.0f / (float)ts;
        m_FpsSmoothed = m_FpsSmoothed > 0.0f ? m_FpsSmoothed * 0.95f + inst * 0.05f : inst;
    }

    // Contexto compartilhado dos painéis dockáveis (preenchido todo frame).
    if (m_PanelContext) {
        m_PanelContext->ActiveScene = m_ActiveScene;
        m_PanelContext->EditorScene = m_EditorScene;
        m_PanelContext->SelectedEntity = m_SelectedEntity;
        m_PanelContext->IsPlay = (m_SceneState == SceneState::Play);
        m_PanelContext->DeltaTime = (float)ts;
        m_PanelContext->FpsSmoothed = m_FpsSmoothed;
        m_PanelContext->ViewportSize = m_ViewportSize;
        m_PanelContext->ViewportFocused = m_ViewportFocused;
        m_PanelContext->ViewportHovered = m_ViewportHovered;
    }

    // Carregamento assíncrono de cena: processa um lote por orçamento de
    // tempo (~4ms) a cada frame. A janela continua viva (eventos processados,
    // tela de carregamento com progresso desenhada) — nada de travar.
    if (m_SceneLoading) {
        float progress = m_PendingLoadProgress;
        bool done = false;
        if (m_PendingLoader)
            done = m_PendingLoader->StepDeserializeTime(0.004f, progress);

        if (done) {
            m_PendingLoadProgress = 1.0f;
            m_ActiveScene = m_PendingScene;
            m_SelectedEntity = {};
            ClearMultiSelection();
            m_ScenePath = m_PendingScenePath;
            m_History.Clear();
            m_SceneLoading = false;
            m_PendingLoader.reset();
            m_PendingScene.reset();
            KZ_CORE_INFO("Cena carregada com sucesso: {0}", m_ScenePath);
            // Se a troca veio de um Scene.Load durante o Play, religa o
            // runtime da cena nova (a cópia antiga já foi parada).
            if (m_SceneState == SceneState::Play) {
                m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
                m_ActiveScene->OnRuntimeStart();
            }
        } else {
            m_PendingLoadProgress = progress;
        }

        // Durante a transição Hub -> Editor a Tela de Carregamento também
        // avança o relógio (o mínimo de tempo só conta depois que o load
        // termina; a tela cobre o carregamento inteiro).
        if (m_EditorState == EditorState::Loading) m_LoadingElapsed += (float)ts;
        return; // nada de atualizar a cena enquanto o load roda
    }

    // Compilação do C# em segundo plano (Play): espera o build terminar e
    // entra no Play — a janela continua viva com o overlay "Compilando...".
    if (m_PlayBuildActive) {
        if (m_PlayBuildDone && !m_PlayBuildCancelled) {
            if (m_PlayBuildOk) {
                if (!ScriptEngine::LoadModule(m_PlayBuildDll))
                    KZ_CORE_ERROR("Play: não foi possível carregar o assembly compilado: {0}",
                                  ScriptEngine::GetLastError());
            } else {
                KZ_CORE_ERROR("Play cancelado — falha ao compilar o jogo:\n{0}", m_PlayBuildError);
            }
            // SEMPRE espera a thread do build terminar antes de seguir (ou de
            // soltar m_PlayBuildActive) — sem o join aqui, a thread ficava
            // joinable e o Play seguinte (novo std::thread) chamava
            // std::terminate() e FECHAVA a engine.
            if (m_PlayBuildThread.joinable()) m_PlayBuildThread.join();
            m_PlayBuildActive = false;
            if (m_PlayBuildOk) StartPlayInternal();
        } else if (m_PlayBuildDone && m_PlayBuildCancelled) {
            m_PlayBuildActive = false;
            if (m_PlayBuildThread.joinable()) m_PlayBuildThread.join(); // deixa a thread terminar em paz
        }
        return; // não atualiza a cena enquanto o build roda
    }

    // Telinha de carregamento: avança o relógio e entra no editor quando o
    // tempo mínimo passa (o carregamento em si é quase instantâneo — o
    // mínimo existe pra tela ser percebida, como em engines maiores).
    if (m_EditorState == EditorState::Loading) {
        m_LoadingElapsed += (float)ts;
        if (m_LoadingElapsed >= kHubLoadingMinSeconds)
            m_EditorState = EditorState::Editor;
    }

    // Hub/telinha de carregamento: nada de cena pra renderizar — a tela é
    // 100% ImGui. Só garante o viewport da janela pra UI desenhar certinho.
    if (m_EditorState != EditorState::Editor) {
        Application& app = Application::Get();
        auto& window = app.GetWindow();
        RenderCommand::SetViewport(0, 0, window.GetWidth(), window.GetHeight());
        return;
    }

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
        // Entrega o mouse (NDC relativo ao viewport) pro Scene fazer o
        // hit-test dos UIButton. m_ViewportBounds é do frame anterior
        // (setado no OnImGuiRender) — suficiente, só muda em resize.
        glm::vec2 vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
        auto [mx, my] = Input::GetMousePosition();
        glm::vec2 ndc{ 0.0f, 0.0f };
        if (vpSize.x > 1.0f && vpSize.y > 1.0f) {
            glm::vec2 local{ mx - m_ViewportBounds[0].x, my - m_ViewportBounds[0].y };
            ndc = { (local.x / vpSize.x) * 2.0f - 1.0f, 1.0f - (local.y / vpSize.y) * 2.0f };
        }
        m_ActiveScene->SetUIMouseNDC(ndc, Input::IsMouseButtonPressed(Mouse::Left));

        // Play: roda a LÓGICA do jogo uma vez e renderiza o viewport.
        // Padrão (recomendado): CÂMERA DO JOGO — o que o jogador vê, WASD
        // move o personagem na tela. A câmera do editor (voar pela cena) é
        // um modo opcional pra cenas sem câmera própria ou quando o usuário
        // desligar a preferência nas Configurações.
        m_ActiveScene->OnUpdateRuntimeLogic(ts);
        if (m_PlayUsesGameCamera && m_ActiveScene->HasPrimaryCamera()) {
            m_ActiveScene->RenderRuntimeView();
        } else {
            if (m_ViewportMode == ViewportMode::Mode2D) UpdateEditor2DCamera(ts);
            else UpdateEditorCamera(ts);
            m_ActiveScene->RenderRuntimeWithEditorCamera(m_EditorCamera);
        }

        std::string nextScene;
        if (m_ActiveScene->PollPendingLoad(nextScene)) {
            m_ActiveScene->OnRuntimeStop();
            AudioEngine::StopAll();
            // Carrega a cena pedida pelo script de forma ASSÍNCRONA — o
            // runtime não congela e a conclusão religa o Play (ver bloco de
            // m_SceneLoading no topo do OnUpdate).
            auto loaded = CreateRef<Scene>("Cena");
            auto loader = std::make_unique<SceneSerializer>(loaded);
            if (!loader->BeginDeserializeStepwiseFile(Project::ResolvePath(nextScene))) {
                KZ_CORE_ERROR("Falha ao carregar cena pedida pelo script: {0}", nextScene);
                OnSceneStop();
            } else {
                m_PendingScene = loaded;
                m_PendingLoader = std::move(loader);
                m_PendingScenePath = nextScene;
                m_PendingLoadProgress = 0.0f;
                m_SceneLoading = true;
            }
        }
    } else if (m_ViewportMode == ViewportMode::Mode3D) {
        UpdateEditorCamera(ts);
        KZ_CORE_TRACE("EditorLayer::OnUpdate — chamando OnUpdateEditor3D");
        m_ActiveScene->OnUpdateEditor3D(ts, m_EditorCamera);

        // Escultura de terreno (pilar AAA v0.34): pincel no viewport 3D.
        // Esquerdo = levanta, Shift+esquerdo = afunda.
        if (m_TerrainSculpting && m_ViewportHovered && m_SelectedEntity &&
            m_SelectedEntity.HasComponent<TerrainComponent>() &&
            Input::IsMouseButtonPressed(Mouse::Left)) {
            auto& terr = m_SelectedEntity.GetComponent<TerrainComponent>();
            if (terr.Heightmap.size() < (size_t)(terr.Segments + 1) * (terr.Segments + 1)) {
                // Converte o fbm atual em heightmap (a 1ª pincelada congela).
                terr.Regenerate();
                const auto& verts = terr.GeneratedMesh ? terr.GeneratedMesh->GetVertices() : std::vector<Vertex3D>{};
                terr.Heightmap.resize((size_t)(terr.Segments + 1) * (terr.Segments + 1), 0.0f);
                for (size_t k = 0; k < verts.size() && k < terr.Heightmap.size(); ++k)
                    terr.Heightmap[k] = verts[k].Position.y;
            }

            // Raio do mouse (mesma matemática do picking).
            glm::vec2 mouse{ Input::GetMouseX(), Input::GetMouseY() };
            glm::vec2 local = mouse - m_ViewportBounds[0];
            glm::vec2 size = m_ViewportBounds[1] - m_ViewportBounds[0];
            if (local.x >= 0.0f && local.y >= 0.0f && local.x <= size.x && local.y <= size.y && size.x > 0.0f && size.y > 0.0f) {
                glm::vec2 ndc{ (local.x / size.x) * 2.0f - 1.0f, 1.0f - (local.y / size.y) * 2.0f };
                glm::mat4 invViewProj = glm::inverse(m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix());
                glm::vec4 nearP = invViewProj * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
                glm::vec4 farP  = invViewProj * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
                nearP /= nearP.w; farP /= farP.w;
                glm::vec3 rayOrigin = glm::vec3(nearP);
                glm::vec3 rayDir = glm::normalize(glm::vec3(farP - nearP));

                // Intersecta o raio com o plano y = altura média do terreno.
                glm::vec3 terrPos = glm::vec3(m_ActiveScene->GetWorldTransform(m_SelectedEntity)[3]);
                float planeY = terrPos.y + terr.HeightScale * 0.5f;
                if (std::abs(rayDir.y) > 1e-5f) {
                    float t = (planeY - rayOrigin.y) / rayDir.y;
                    if (t > 0.0f) {
                        glm::vec3 hit = rayOrigin + rayDir * t;
                        float dir = Input::IsKeyPressed(Key::LeftShift) ? -1.0f : 1.0f;
                        float strength = m_TerrainBrushStrength * (float)ts * dir;

                        uint32_t seg = terr.Segments;
                        float half = terr.Size * 0.5f;
                        float cell = terr.Size / (float)seg;
                        int cx = (int)glm::round((hit.x - terrPos.x + half) / cell);
                        int cz = (int)glm::round((hit.z - terrPos.z + half) / cell);
                        int radiusCells = (int)glm::ceil(m_TerrainBrushRadius / cell);
                        for (int i = glm::max(0, cx - radiusCells); i <= glm::min((int)seg, cx + radiusCells); ++i) {
                            for (int j = glm::max(0, cz - radiusCells); j <= glm::min((int)seg, cz + radiusCells); ++j) {
                                float dx = (i - cx) * cell;
                                float dz = (j - cz) * cell;
                                float d = std::sqrt(dx * dx + dz * dz);
                                if (d > m_TerrainBrushRadius) continue;
                                float falloff = 1.0f - d / m_TerrainBrushRadius;
                                falloff *= falloff;
                                float& h = terr.Heightmap[(size_t)i * (seg + 1) + j];
                                h = glm::clamp(h + strength * falloff, -20.0f, 20.0f);
                            }
                        }
                        terr.Regenerate();
                        if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
                            m_SelectedEntity.GetComponent<MeshRendererComponent>().MeshAsset = terr.GeneratedMesh;
                    }
                }
            }
        }

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

    // Render dos painéis com FBO próprio (Game View, Material Editor...):
    // DEPOIS do viewport, pra pegar o estado atualizado da cena.
    if (m_PanelContext) {
        for (auto& panel : m_Panels)
            if (panel->IsVisible()) panel->OnUpdate(ts);
    }

    // Arquivos soltos do SISTEMA (Explorer/gerenciador de arquivos) na janela:
    // cria a entidade correspondente na posição do mouse, como o arrasto
    // interno do Content Browser. Só em modo edição.
    if (m_SceneState == SceneState::Edit) {
        auto& dropped = Application::Get().GetWindow().GetDroppedFiles();
        if (!dropped.empty()) {
            for (const std::string& dropPath : dropped) {
                Entity created = CreateEntityFromAsset(dropPath, MouseDropWorldPos());
                if (created) {
                    m_SelectedEntity = created;
                    AutoSwitchViewportMode();
                }
            }
            dropped.clear();
        }
    }
}

// Posição de mundo (3D: no chão y=0; 2D: no plano) para onde o mouse do
// viewport aponta — usada no drop de arquivos do sistema e no Content Browser.
glm::vec3 EditorLayer::MouseDropWorldPos() const {
    glm::vec2 mouse{ ImGui::GetMousePos().x, ImGui::GetMousePos().y };
    glm::vec2 local = mouse - m_ViewportBounds[0];
    glm::vec2 size = m_ViewportBounds[1] - m_ViewportBounds[0];
    if (size.x <= 0.0f || size.y <= 0.0f ||
        local.x < 0.0f || local.y < 0.0f || local.x > size.x || local.y > size.y)
        return { 0.0f, 0.0f, 0.0f };
    glm::vec2 ndc{ (local.x / size.x) * 2.0f - 1.0f, 1.0f - (local.y / size.y) * 2.0f };
    if (m_ViewportMode == ViewportMode::Mode3D) {
        glm::mat4 inv = glm::inverse(m_EditorCamera.GetProjectionMatrix() * m_EditorCamera.GetViewMatrix());
        glm::vec4 nearP = inv * glm::vec4(ndc.x, ndc.y, -1.0f, 1.0f);
        glm::vec4 farP  = inv * glm::vec4(ndc.x, ndc.y, 1.0f, 1.0f);
        glm::vec3 o = glm::vec3(nearP) / nearP.w;
        glm::vec3 d = glm::normalize(glm::vec3(farP) / farP.w - o);
        float t = (0.0f - o.y) / glm::max(d.y, 0.0001f);
        return (t > 0.0f) ? o + d * t : o;
    }
    glm::mat4 inv = glm::inverse(m_Editor2DCamera.GetProjectionMatrix() * m_Editor2DCamera.GetViewMatrix());
    glm::vec4 wp = inv * glm::vec4(ndc.x, ndc.y, 0.0f, 1.0f);
    return { wp.x / wp.w, wp.y / wp.w, 0.0f };
}

void EditorLayer::OnEvent(Event& e) {
    (void)e;
}

void EditorLayer::UpdateEditorCamera(Timestep ts) {
    KZ_TRACE_SCOPE("EditorLayer::UpdateEditorCamera");
    bool flying = m_ViewportHovered && Input::IsMouseButtonPressed(Mouse::Right);

    auto [mx, my] = Input::GetMousePosition();
    glm::vec2 mousePos{ mx, my };

    // Mesma convenção de PerspectiveCamera::RecalculateViewMatrix
    // (Camera.cpp) — precisa bater pra WASD mover na direção que a
    // câmera está de fato olhando.
    glm::vec3 forward{
        cos(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch)),
        sin(glm::radians(m_EditorCamPitch)),
        sin(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch))
    };
    forward = glm::normalize(forward);

    if (flying) {
        if (m_FirstMouseLook) {
            m_LastMousePos = mousePos;
            m_FirstMouseLook = false;
        }
        glm::vec2 delta = mousePos - m_LastMousePos;

        constexpr float kLookSensitivity = 0.12f;
        m_EditorCamYaw += delta.x * m_EditorCamSensitivity;
        m_EditorCamPitch -= delta.y * kLookSensitivity;        m_EditorCamPitch = std::clamp(m_EditorCamPitch, -89.0f, 89.0f);

        glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));
        glm::vec3 up = glm::cross(right, forward);

        float speed = (Input::IsKeyPressed(Key::LeftShift) ? m_EditorCamFlySpeed * 3.0f : m_EditorCamFlySpeed) * (float)ts;
        if (Input::IsKeyPressed(Key::W)) m_EditorCamPos += forward * speed;
        if (Input::IsKeyPressed(Key::S)) m_EditorCamPos -= forward * speed;
        if (Input::IsKeyPressed(Key::A)) m_EditorCamPos -= right * speed;
        if (Input::IsKeyPressed(Key::D)) m_EditorCamPos += right * speed;
        if (Input::IsKeyPressed(Key::E)) m_EditorCamPos += up * speed;
        if (Input::IsKeyPressed(Key::Q)) m_EditorCamPos -= up * speed;
    } else if (m_ViewportHovered && Input::IsKeyPressed(Key::LeftAlt) &&
               (Input::IsMouseButtonPressed(Mouse::Left) ||
                Input::IsMouseButtonPressed(Mouse::Right) ||
                Input::IsMouseButtonPressed(Mouse::Middle))) {
        // ---- ÓRBITA (pivô): estilo Blender — Alt + arrastar gira a câmera
        // ao redor do alvo (entidade selecionada, ou o ponto em que ela
        // está olhando) em vez de girar no próprio eixo.
        if (m_FirstMouseLook) {
            m_LastMousePos = mousePos;
            m_FirstMouseLook = false;
        }
        glm::vec2 delta = mousePos - m_LastMousePos;

        glm::vec3 target = m_EditorOrbitTarget;
        if (m_SelectedEntity && m_SelectedEntity.HasComponent<TransformComponent>()) {
            const glm::mat4& world = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
            target = glm::vec3(world[3]);
            m_EditorOrbitTarget = target;
        } else if (m_EditorOrbitDist < 0.0f) {
            // Sem alvo ainda: usa o ponto central da tela na distância atual.
            m_EditorOrbitTarget = m_EditorCamPos + forward * glm::length(m_EditorCamPos);
            m_EditorOrbitDist = glm::length(m_EditorCamPos - m_EditorOrbitTarget);
        }

        // Distância alvo->câmera (recaptura a cada órbita; zoom no scroll já
        // existe e redimensiona essa distância quando orbitando).
        glm::vec3 toCam = m_EditorCamPos - m_EditorOrbitTarget;
        float dist = std::max(glm::length(toCam), 0.5f);

        constexpr float kOrbitLookSensitivity = 0.12f;
        m_EditorCamYaw += delta.x * m_EditorCamSensitivity;
        m_EditorCamPitch = std::clamp(m_EditorCamPitch - delta.y * kOrbitLookSensitivity, -89.0f, 89.0f);

        glm::vec3 fwd{
            cos(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch)),
            sin(glm::radians(m_EditorCamPitch)),
            sin(glm::radians(m_EditorCamYaw)) * cos(glm::radians(m_EditorCamPitch))
        };
        m_EditorCamPos = m_EditorOrbitTarget - fwd * dist;
        m_EditorOrbitDist = dist;

        // Zoom continua funcionando na órbita (recalcula depois).
        if (ImGui::GetIO().MouseWheel != 0.0f) {
            float k = 1.0f - ImGui::GetIO().MouseWheel * 0.12f;
            m_EditorOrbitDist = std::max(0.5f, m_EditorOrbitDist * k);
            m_EditorCamPos = m_EditorOrbitTarget - fwd * m_EditorOrbitDist;
        }
    } else {
        // Solta o botão direito -> próxima vez que apertar não deve "pular"
        // usando o delta acumulado enquanto o mouse não estava sendo lido.
        m_FirstMouseLook = true;
    }

    m_LastMousePos = mousePos;

    // Zoom com a rodinha (só com o viewport sob o mouse): move a câmera na
    // direção do olhar — igual Unity/Godot. Multiplicativo pra ser suave
    // tanto de perto quanto de longe.
    if (m_ViewportHovered && !flying) {
        float scroll = ImGui::GetIO().MouseWheel;
        if (scroll != 0.0f) {
            float distance = glm::length(m_EditorCamPos);
            m_EditorCamPos += forward * (scroll * distance * 0.12f);
        }
    }

    m_EditorCamera.SetPosition(m_EditorCamPos);
    // ---- Vistas rápidas estilo Blender: 1 = frente (-Z), 3 = direita (+X),
    // 7 = topo (-Y). Só com o viewport 3D sob o cursor e sem voar/digitando.
    if (m_ViewportMode == ViewportMode::Mode3D && m_ViewportHovered &&
        !Input::IsMouseButtonPressed(Mouse::Right) &&
        !Input::IsKeyPressed(Key::LeftAlt) && !ImGui::GetIO().WantTextInput) {
        if (Input::IsKeyPressed(Key::D1)) {
            m_EditorCamYaw = -90.0f; m_EditorCamPitch = -10.0f; // frente
        } else if (Input::IsKeyPressed(Key::D3)) {
            m_EditorCamYaw = 0.0f;   m_EditorCamPitch = 0.0f;   // direita
        } else if (Input::IsKeyPressed(Key::D7)) {
            m_EditorCamPitch = -89.5f;                          // topo
        }
    }

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

namespace {
// Botão de toolbar que desenha um ícone vetorial por cima do próprio botão —
// o padrão das engines: ferramenta identificada por ícone, não por texto.
bool ToolbarIconButton(kizuri::editor::icons::IconFn icon, const char* id,
                       const ImVec2& size, ImU32 color) {
    ImGui::PushID(id);
    bool pressed = ImGui::Button("", size);
    ImVec2 min = ImGui::GetItemRectMin();
    ImVec2 max = ImGui::GetItemRectMax();
    float s = size.y - 6.0f;
    ImVec2 tl((min.x + max.x) * 0.5f - s * 0.5f, (min.y + max.y) * 0.5f - s * 0.5f);
    icon(ImGui::GetWindowDrawList(), tl, s, color);
    ImGui::PopID();
    return pressed;
}
} // namespace

void EditorLayer::DrawViewportToolbar() {
    KZ_TRACE_SCOPE("EditorLayer::DrawViewportToolbar");
    const ImVec4 accent(0.82f, 0.24f, 0.27f, 1.0f);
    const ImVec4 inactive(0.18f, 0.18f, 0.20f, 1.0f);
    const ImVec4 activeText(1.0f, 1.0f, 1.0f, 1.0f);
    const ImVec4 idleText(0.74f, 0.74f, 0.78f, 1.0f);

    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);

    // --- Ferramentas de gizmo por ÍCONE: Mover / Rotacionar / Escalar (W/E/R). ---
    const kizuri::editor::icons::IconFn gizmoIcons[3] =
        { kizuri::editor::icons::Move, kizuri::editor::icons::Rotate, kizuri::editor::icons::Scale };
    const ImGuizmo::OPERATION gizmoOps[3] =
        { ImGuizmo::TRANSLATE, ImGuizmo::ROTATE, ImGuizmo::SCALE };
    const char* gizmoHints[3] = { "Mover entidade (W)", "Rotacionar entidade (E)", "Escalar entidade (R)" };
    const char* gizmoIds[3] = { "##gizmo_move", "##gizmo_rotate", "##gizmo_scale" };
    for (int i = 0; i < 3; ++i) {
        bool active = m_GizmoOperation == gizmoOps[i];
        ImGui::PushStyleColor(ImGuiCol_Button, active ? accent : inactive);
        if (ToolbarIconButton(gizmoIcons[i], gizmoIds[i], ImVec2(34.0f, 30.0f),
                              ImGui::GetColorU32(active ? activeText : idleText)))
            m_GizmoOperation = gizmoOps[i];
        ImGui::PopStyleColor();
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("%s", gizmoHints[i]);
        if (i < 2) ImGui::SameLine(0.0f, 4.0f);
    }

    ImGui::SameLine(0.0f, 16.0f);

    // --- Modo de navegação 2D/3D do editor (só muda a câmera de edição,
    // nunca trava a cena — uma entidade 2D e uma 3D convivem). ---
    bool is2D = m_ViewportMode == ViewportMode::Mode2D;
    bool is3D = m_ViewportMode == ViewportMode::Mode3D;

    ImGui::PushStyleColor(ImGuiCol_Button, is2D ? accent : inactive);
    ImGui::PushStyleColor(ImGuiCol_Text, is2D ? activeText : idleText);
    if (ImGui::Button("2D", ImVec2(36.0f, 30.0f))) m_ViewportMode = ViewportMode::Mode2D;
    ImGui::PopStyleColor(2);
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, is3D ? accent : inactive);
    ImGui::PushStyleColor(ImGuiCol_Text, is3D ? activeText : idleText);
    if (ImGui::Button("3D", ImVec2(36.0f, 30.0f))) m_ViewportMode = ViewportMode::Mode3D;
    ImGui::PopStyleColor(2);

    // --- Play/Stop por ícone no canto direito: física, scripts, partículas e
    // áudio só rodam de verdade numa cópia isolada da cena (Scene::Copy) —
    // editar durante o Play é seguro e o Stop nunca "perde" nada. ---
    bool isPlaying = m_SceneState == SceneState::Play;
    ImGui::SameLine(ImGui::GetWindowContentRegionMax().x - 84.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, isPlaying ? accent : inactive);
    if (ToolbarIconButton(isPlaying ? kizuri::editor::icons::Stop : kizuri::editor::icons::Play,
                          "##play_stop", ImVec2(32.0f, 30.0f),
                          ImGui::GetColorU32(activeText))) {
        if (isPlaying) OnSceneStop(); else OnScenePlay();
    }
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(isPlaying ? "Parar o Play e voltar pra cena de edição (Shift+F5)"
                                    : "Testar a cena — física, scripts, partículas e áudio (F5)");

    // --- Fullscreen do viewport: esconde os painéis laterais e o viewport
    // ocupa o espaço todo (aperte de novo pra voltar). ---
    ImGui::SameLine(0.0f, 4.0f);
    ImGui::PushStyleColor(ImGuiCol_Button, m_ViewportMaximized ? accent : inactive);
    if (ToolbarIconButton(kizuri::editor::icons::Maximize, "##viewport_maximize", ImVec2(34.0f, 30.0f),
                          ImGui::GetColorU32(m_ViewportMaximized ? activeText : idleText)))
        m_ViewportMaximized = !m_ViewportMaximized;
    ImGui::PopStyleColor();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip(m_ViewportMaximized ? "Sair do fullscreen (mostra os painéis)"
                                              : "Fullscreen do viewport (F11)");

    // F11 também alterna o fullscreen do viewport (edge-detect).
    bool f11Down = kizuri::Input::IsKeyPressed(kizuri::Key::F11);
    bool f11JustPressed = f11Down && !m_PrevF11KeyDown;
    m_PrevF11KeyDown = f11Down;
    if (m_ViewportHovered && f11JustPressed)
        m_ViewportMaximized = !m_ViewportMaximized;

    ImGui::PopStyleVar();

    // Linha de dica de navegação — dá cara de toolbar e lembra os atalhos.
    ImGui::TextDisabled("%s", is2D
        ? "Botão direito arrasta para navegar; scroll aplica zoom"
        : "Navegue com botão direito + WASD; Q/E sobe e desce; W/E/R troca a ferramenta do gizmo");
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

// Aceita drop de arquivo do Content Browser no widget que acabou de desenhar
// (ex.: campo de textura/material no Inspetor). Devolve o caminho absoluto
// do arquivo solto, ou false se nada foi solto aqui. É o mesmo payload dos
// dois drag sources do Content Browser (KZ_CONTENT_FILE / KZ_CONTENT_BROWSER_FILE).
static bool AcceptAssetDrop(std::string& outPath) {
    if (!ImGui::BeginDragDropTarget()) return false;
    bool accepted = false;
    if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_CONTENT_FILE")) {
        outPath.assign((const char*)payload->Data);
        accepted = true;
    } else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("KZ_CONTENT_BROWSER_FILE")) {
        outPath.assign((const char*)payload->Data);
        accepted = true;
    }
    ImGui::EndDragDropTarget();
    return accepted;
}

// Botão "..." que abre o diálogo NATIVO de arquivos do sistema (Windows:
// IFileDialog, a mesma janela do Explorer) e preenche 'outPath' com o caminho
// RELATIVO ao projeto. Em plataformas sem backend o diálogo volta vazio e o
// campo de texto manual continua sendo a alternativa (nunca travar nisso).
static bool FileBrowseButton(const char* filterName, const char* filterPattern, std::string& outPath) {
    ImGui::SameLine();
    if (ImGui::Button("...")) {
        std::string picked = kizuri::FileDialog::OpenFile(filterName, filterPattern);
        if (!picked.empty()) {
            outPath = kizuri::Project::MakeRelativePath(picked);
            return true;
        }
    }
    return false;
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

    // A view da câmera do jogo agora vem da MATRIZ do Transform (ver
    // PerspectiveCamera::SetWorldTransform) — o gizmo usa a MESMA base
    // (forward/up das colunas), então desenha exatamente onde a câmera vê,
    // mesmo com rotações como -90° em Y ou roll.
    glm::vec3 forward = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 0.0f, -1.0f));
    glm::vec3 worldUp = glm::normalize(glm::mat3(world) * glm::vec3(0.0f, 1.0f, 0.0f));
    // Base ortonormal do lookAt: right = forward x up, up = right x forward.
    glm::vec3 right = glm::normalize(glm::cross(forward, worldUp));
    glm::vec3 up = glm::cross(right, forward);

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

// Marcador visual de luz no viewport 3D: círculo com raios (ponto), seta
// (direcional) ou círculo + seta (spot). Mesmo estilo do gizmo de câmera —
// ImDrawList em espaço de tela, sempre por cima da cena.
void EditorLayer::DrawLightGizmo() {
    if (!m_SelectedEntity || !m_SelectedEntity.HasComponent<LightComponent>()) return;
    if (m_ViewportMode != ViewportMode::Mode3D) return;

    auto& lc = m_SelectedEntity.GetComponent<LightComponent>();
    glm::mat4 world = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
    glm::vec3 pos = glm::vec3(world[3]);

    glm::mat4 viewProj = m_EditorCamera.GetViewProjectionMatrix();
    glm::vec2 vpPos = m_ViewportBounds[0], vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    ImVec2 screen;
    if (!ProjectToViewport(viewProj, pos, vpPos, vpSize, screen)) return;

    // Direção da luz derivada do Transform (mesma convenção fps do render).
    glm::vec3 euler, t, s;
    DecomposeTransform(world, t, euler, s);
    glm::vec3 forward = glm::normalize(glm::vec3(
        glm::cos(euler.y) * glm::cos(euler.x), glm::sin(euler.x), glm::sin(euler.y) * glm::cos(euler.x)));

    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(255, 200, 80, 255);
    const float r = 10.0f;
    dl->AddCircle(screen, r, color, 24, 2.0f);

    ImVec2 tip;
    bool hasTip = ProjectToViewport(viewProj, pos + forward * 1.2f, vpPos, vpSize, tip);

    if (lc.Type == LightType::Point) {
        // raios nas diagonais
        for (int i = 0; i < 4; ++i) {
            float a = glm::radians(45.0f + i * 90.0f);
            ImVec2 d(cosf(a), sinf(a));
            dl->AddLine(ImVec2(screen.x + d.x * r, screen.y + d.y * r),
                        ImVec2(screen.x + d.x * (r + 8.0f), screen.y + d.y * (r + 8.0f)), color, 2.0f);
        }
    } else {
        // direcional/spot: linha grossa pra frente + barra na ponta
        if (hasTip) {
            dl->AddLine(screen, tip, color, 3.0f);
            glm::vec2 d(tip.x - screen.x, tip.y - screen.y);
            float len = sqrtf(d.x * d.x + d.y * d.y);
            if (len > 0.001f) {
                d /= len;
                glm::vec2 perp(-d.y, d.x);
                dl->AddLine(ImVec2(tip.x + perp.x * 5.0f, tip.y + perp.y * 5.0f),
                            ImVec2(tip.x - perp.x * 5.0f, tip.y - perp.y * 5.0f), color, 2.5f);
            }
        }
    }
}

// Wireframe dos colisores da entidade selecionada (verde, estilo "debug draw"
// de engine): círculo/box 2D no modo 2D; box/esfera 3D no modo 3D. Usa
// ImDrawList em espaço de tela (mesmo padrão dos gizmos de câmera/luz).
void EditorLayer::DrawColliderGizmo() {
    if (!m_SelectedEntity || m_SceneState != SceneState::Edit) return;

    glm::mat4 viewProj = (m_ViewportMode == ViewportMode::Mode2D)
        ? m_Editor2DCamera.GetViewProjectionMatrix()
        : m_EditorCamera.GetViewProjectionMatrix();
    glm::vec2 vpPos = m_ViewportBounds[0], vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(120, 220, 120, 220);
    glm::vec3 pos = glm::vec3(m_ActiveScene->GetWorldTransform(m_SelectedEntity)[3]);

    auto Project = [&](const glm::vec3& wp, ImVec2& out) {
        return ProjectToViewport(viewProj, wp, vpPos, vpSize, out);
    };
    auto Line = [&](const glm::vec3& a, const glm::vec3& b) {
        ImVec2 sa, sb;
        if (Project(a, sa) && Project(b, sb)) dl->AddLine(sa, sb, color, 1.5f);
    };

    if (m_ViewportMode == ViewportMode::Mode2D) {
        if (m_SelectedEntity.HasComponent<CircleCollider2DComponent>()) {
            auto& col = m_SelectedEntity.GetComponent<CircleCollider2DComponent>();
            ImVec2 center;
            glm::vec3 c = pos + glm::vec3(col.Offset.x, col.Offset.y, 0.0f);
            glm::vec3 e = c + glm::vec3(col.Radius, 0.0f, 0.0f);
            if (Project(c, center)) {
                ImVec2 edge;
                if (Project(e, edge)) {
                    float rpx = glm::length(glm::vec2(edge.x - center.x, edge.y - center.y));
                    dl->AddCircle(center, rpx, color, 48, 1.5f);
                }
            }
        }
        if (m_SelectedEntity.HasComponent<BoxCollider2DComponent>()) {
            auto& col = m_SelectedEntity.GetComponent<BoxCollider2DComponent>();
            glm::mat4 world = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
            glm::vec3 h(col.Size.x * 0.5f, col.Size.y * 0.5f, 0.0f);
            glm::vec3 corners[4];
            for (int i = 0; i < 4; ++i) {
                glm::vec3 s(((i & 1) ? h.x : -h.x), ((i & 2) ? h.y : -h.y), 0.0f);
                corners[i] = glm::vec3(world * glm::vec4(s + glm::vec3(col.Offset.x, col.Offset.y, 0.0f), 1.0f));
            }
            for (int i = 0; i < 4; ++i) Line(corners[i], corners[(i + 1) % 4]);
        }
    } else {
        // 3D: linhas com TESTE DE PROFUNDIDADE (não flutuam por cima do objeto)
        // e rotação pelo transform mundial (o wireframe abraça a malha mesmo
        // com o corpo rotacionado).
        glm::mat4 world = m_ActiveScene->GetWorldTransform(m_SelectedEntity);
        const glm::vec3 dbgColor(120.0f / 255.0f, 220.0f / 255.0f, 120.0f / 255.0f);
        auto SubmitLine = [&](const glm::vec3& a, const glm::vec3& b) {
            kizuri::Renderer3D::SubmitDebugLine(a, b, dbgColor);
        };

        if (m_SelectedEntity.HasComponent<SphereCollider3DComponent>()) {
            auto& col = m_SelectedEntity.GetComponent<SphereCollider3DComponent>();
            const int segs = 24;
            auto ring = [&](const glm::vec3& n0, const glm::vec3& n1) {
                glm::vec3 prev = glm::vec3(world * glm::vec4(n0 * col.Radius + n1 * col.Radius, 1.0f));
                for (int i = 1; i <= segs; ++i) {
                    float t = (float)i / (float)segs * 3.14159265f * 2.0f;
                    glm::vec3 p = glm::vec3(world * glm::vec4((n0 * cosf(t) + n1 * sinf(t)) * col.Radius, 1.0f));
                    SubmitLine(prev, p);
                    prev = p;
                }
            };
            ring({ 1,0,0 }, { 0,1,0 }); // XZ -> círculo no plano YZ? (X,Y) no plano Z
            ring({ 1,0,0 }, { 0,0,1 }); // plano XZ
            ring({ 0,1,0 }, { 0,0,1 }); // plano YZ
        }
        if (m_SelectedEntity.HasComponent<BoxCollider3DComponent>()) {
            auto& col = m_SelectedEntity.GetComponent<BoxCollider3DComponent>();
            glm::vec3 h = col.HalfExtents;
            glm::vec3 corners[8];
            for (int i = 0; i < 8; ++i) {
                glm::vec3 s((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
                corners[i] = glm::vec3(world * glm::vec4(s, 1.0f));
            }
            const int edges[12][2] = { {0,1},{1,3},{3,2},{2,0}, {4,5},{5,7},{7,6},{6,4}, {0,4},{1,5},{2,6},{3,7} };
            for (auto& e : edges) SubmitLine(corners[e[0]], corners[e[1]]);
        }
    }
}

// Overlay de física debug: desenha o wireframe de TODOS os colliders da cena
// (2D: caixa/círculo; 3D: caixa/esfera), não só o da entidade selecionada.
// Cor mais apagada pra não competir com o gizmo de seleção. Aproximação
// axis-aligned (não rotaciona pelo corpo) — suficiente pra visualizar.
// Debug da IA (v0.34): desenha a grade de navegação (células bloqueadas em
// vermelho) e os caminhos atuais de cada NavAgent (amarelo, com o destino
// em verde). Visível durante o Play e em edição com o overlay de colliders.
void EditorLayer::DrawNavDebug() {
    if (!m_ActiveScene) return;
    if (m_SceneState == SceneState::Edit && !m_ShowColliders) return;

    glm::mat4 viewProj = (m_ViewportMode == ViewportMode::Mode2D)
        ? m_Editor2DCamera.GetViewProjectionMatrix()
        : m_EditorCamera.GetViewProjectionMatrix();
    glm::vec2 vpPos = m_ViewportBounds[0], vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    ImDrawList* dl = ImGui::GetWindowDrawList();

    auto Project = [&](const glm::vec3& wp, ImVec2& out) {
        return ProjectToViewport(viewProj, wp, vpPos, vpSize, out);
    };
    auto Line = [&](const glm::vec3& a, const glm::vec3& b, ImU32 color) {
        ImVec2 sa, sb;
        if (Project(a, sa) && Project(b, sb)) dl->AddLine(sa, sb, color, 1.5f);
    };

    auto& registry = m_ActiveScene->GetRegistry();

    // Grade de navegação.
    registry.view<kizuri::TransformComponent, kizuri::NavGridComponent>().each([&](auto, auto&, auto& ngc) {
        if (!ngc.Grid || ngc.Grid->GetWidth() <= 0) return;
        const kizuri::NavGrid& g = *ngc.Grid;
        const ImU32 gridCol = IM_COL32(120, 180, 255, 60);
        const ImU32 blockCol = IM_COL32(255, 90, 80, 110);
        int w = g.GetWidth(), d = g.GetDepth();
        float cs = g.GetCellSize();
        // Linhas da grade (a cada 4 células, pra não poluir).
        int step = std::max(1, (int)(w / 20));
        for (int x = 0; x <= w; x += step) {
            Line({ g.GetOriginX() + x * cs, 0.02f, g.GetOriginZ() },
                 { g.GetOriginX() + x * cs, 0.02f, g.GetOriginZ() + d * cs }, gridCol);
        }
        for (int z = 0; z <= d; z += step) {
            Line({ g.GetOriginX(), 0.02f, g.GetOriginZ() + z * cs },
                 { g.GetOriginX() + w * cs, 0.02f, g.GetOriginZ() + z * cs }, gridCol);
        }
        // Células bloqueadas (quadradinhos).
        ImVec2 a, b;
        if (Project({ g.GetOriginX(), 0.0f, g.GetOriginZ() }, a) &&
            Project({ g.GetOriginX() + cs, 0.0f, g.GetOriginZ() + cs }, b)) {
            float px = std::max(2.0f, (b.x - a.x) * 0.8f);
            for (int z = 0; z < d; ++z)
                for (int x = 0; x < w; ++x) {
                    if (!g.IsBlocked(x, z)) continue;
                    ImVec2 c;
                    if (Project(g.CellToWorld(x, z), c))
                        dl->AddRectFilled({ c.x - px * 0.5f, c.y - px * 0.5f },
                                          { c.x + px * 0.5f, c.y + px * 0.5f }, blockCol);
                }
        }
    });

    // Caminhos dos agentes.
    registry.view<kizuri::TransformComponent, kizuri::NavAgentComponent>().each([&](auto, auto&, auto& na) {
        if (!na.HasDestination) return;
        const ImU32 pathCol = IM_COL32(255, 220, 90, 200);
        const ImU32 destCol = IM_COL32(120, 255, 120, 220);
        glm::vec3 prev;
        bool hasPrev = false;
        for (size_t i = na.PathIndex; i < na.Path.size(); ++i) {
            glm::vec3 p = na.Path[i];
            p.y = 0.05f;
            if (hasPrev) Line(prev, p, pathCol);
            prev = p;
            hasPrev = true;
        }
        glm::vec3 dest = na.Destination;
        dest.y = 0.05f;
        ImVec2 sc;
        if (Project(dest, sc)) dl->AddCircleFilled(sc, 4.0f, destCol);
    });
}

void EditorLayer::DrawAllColliders() {
    if (m_SceneState != SceneState::Edit || !m_ActiveScene) return;

    glm::mat4 viewProj = (m_ViewportMode == ViewportMode::Mode2D)
        ? m_Editor2DCamera.GetViewProjectionMatrix()
        : m_EditorCamera.GetViewProjectionMatrix();
    glm::vec2 vpPos = m_ViewportBounds[0], vpSize = m_ViewportBounds[1] - m_ViewportBounds[0];
    ImDrawList* dl = ImGui::GetWindowDrawList();
    const ImU32 color = IM_COL32(120, 220, 120, 90);

    auto Project = [&](const glm::vec3& wp, ImVec2& out) {
        return ProjectToViewport(viewProj, wp, vpPos, vpSize, out);
    };
    auto Line = [&](const glm::vec3& a, const glm::vec3& b) {
        ImVec2 sa, sb;
        if (Project(a, sa) && Project(b, sb)) dl->AddLine(sa, sb, color, 1.0f);
    };

    auto& registry = m_ActiveScene->GetRegistry();
    registry.view<kizuri::TransformComponent>().each([&](auto entityHandle, const kizuri::TransformComponent&) {
        kizuri::Entity e{ entityHandle, m_ActiveScene.get() };
        glm::vec3 pos = glm::vec3(m_ActiveScene->GetWorldTransform(e)[3]);

        if (const auto* c2 = m_ActiveScene->GetRegistry().try_get<kizuri::CircleCollider2DComponent>(entityHandle)) {
            glm::vec3 c = pos + glm::vec3(c2->Offset.x, c2->Offset.y, 0.0f);
            glm::vec3 ed = c + glm::vec3(c2->Radius, 0.0f, 0.0f);
            ImVec2 center, edge;
            if (Project(c, center) && Project(ed, edge)) {
                float rpx = glm::length(glm::vec2(edge.x - center.x, edge.y - center.y));
                dl->AddCircle(center, rpx, color, 32, 1.0f);
            }
        }
        if (const auto* b2 = m_ActiveScene->GetRegistry().try_get<kizuri::BoxCollider2DComponent>(entityHandle)) {
            glm::mat4 world = m_ActiveScene->GetWorldTransform(e);
            glm::vec3 h(b2->Size.x * 0.5f, b2->Size.y * 0.5f, 0.0f);
            glm::vec3 corners[4];
            for (int i = 0; i < 4; ++i) {
                glm::vec3 s(((i & 1) ? h.x : -h.x), ((i & 2) ? h.y : -h.y), 0.0f);
                corners[i] = glm::vec3(world * glm::vec4(s + glm::vec3(b2->Offset.x, b2->Offset.y, 0.0f), 1.0f));
            }
            for (int i = 0; i < 4; ++i) Line(corners[i], corners[(i + 1) % 4]);
        }
        if (const auto* s3 = m_ActiveScene->GetRegistry().try_get<kizuri::SphereCollider3DComponent>(entityHandle)) {
            glm::mat4 world = m_ActiveScene->GetWorldTransform(e);
            const glm::vec3 dbg(120.0f / 255.0f, 220.0f / 255.0f, 120.0f / 255.0f);
            const int segs = 24;
            auto ring = [&](const glm::vec3& n0, const glm::vec3& n1) {
                glm::vec3 prev = glm::vec3(world * glm::vec4(n0 * s3->Radius + n1 * s3->Radius, 1.0f));
                for (int i = 1; i <= segs; ++i) {
                    float t = (float)i / (float)segs * 3.14159265f * 2.0f;
                    glm::vec3 p = glm::vec3(world * glm::vec4((n0 * cosf(t) + n1 * sinf(t)) * s3->Radius, 1.0f));
                    kizuri::Renderer3D::SubmitDebugLine(prev, p, dbg);
                    prev = p;
                }
            };
            ring({ 1,0,0 }, { 0,1,0 });
            ring({ 1,0,0 }, { 0,0,1 });
            ring({ 0,1,0 }, { 0,0,1 });
        }
        if (const auto* b3 = m_ActiveScene->GetRegistry().try_get<kizuri::BoxCollider3DComponent>(entityHandle)) {
            glm::mat4 world = m_ActiveScene->GetWorldTransform(e);
            const glm::vec3 dbg(120.0f / 255.0f, 220.0f / 255.0f, 120.0f / 255.0f);
            glm::vec3 h = b3->HalfExtents;
            glm::vec3 corners[8];
            for (int i = 0; i < 8; ++i) {
                glm::vec3 s((i & 1) ? h.x : -h.x, (i & 2) ? h.y : -h.y, (i & 4) ? h.z : -h.z);
                corners[i] = glm::vec3(world * glm::vec4(s, 1.0f));
            }
            const int edges[12][2] = { {0,1},{1,3},{3,2},{2,0}, {4,5},{5,7},{7,6},{6,4}, {0,4},{1,5},{2,6},{3,7} };
            for (auto& e : edges) kizuri::Renderer3D::SubmitDebugLine(corners[e[0]], corners[e[1]], dbg);
        }
    });
}

kizuri::Ref<kizuri::Texture2D> EditorLayer::GetThumbnail(const std::string& path) {
    auto it = m_ThumbCache.find(path);
    if (it != m_ThumbCache.end()) return it->second;

    // Pasta com milhares de imagens não pode travar o editor: só decodifica
    // até o orçamento do frame (m_ThumbBudget); o resto ganha o placeholder e
    // a miniatura real aparece nos frames seguintes.
    if (m_ThumbBudget <= 0) return nullptr;
    --m_ThumbBudget;

    auto tex = kizuri::Texture2D::Create(path);
    m_ThumbCache[path] = tex;
    return tex;
}

void EditorLayer::LoadGraphicsSettingsFromDisk() {
    m_GraphicsSettings = kizuri::Renderer3D::GetGraphicsSettings();
    if (kizuri::LoadGraphicsSettings("settings.json", m_GraphicsSettings)) {
        kizuri::Renderer3D::SetGraphicsSettings(m_GraphicsSettings);
        KZ_CORE_INFO("Configurações gráficas carregadas de settings.json (preset {0}).", (int)m_GraphicsSettings.Preset);
    }
    strncpy(m_EnvironmentHDRIPathBuffer, kizuri::Renderer3D::GetEnvironmentHDRIPath().c_str(),
            sizeof(m_EnvironmentHDRIPathBuffer) - 1);
    m_EnvironmentHDRIPathBuffer[sizeof(m_EnvironmentHDRIPathBuffer) - 1] = '\0';
    Application& app = Application::Get();
    if (app.GetWindow().IsVSync() != m_GraphicsSettings.VSync)
        app.GetWindow().SetVSync(m_GraphicsSettings.VSync);
}

void EditorLayer::SaveGraphicsSettingsToDisk() {
    if (kizuri::SaveGraphicsSettings("settings.json", m_GraphicsSettings))
        KZ_CORE_INFO("Configurações gráficas salvas em settings.json.");
    else
        KZ_CORE_ERROR("Falha ao salvar settings.json.");
}

void EditorLayer::DrawSettings() {
    if (!m_ShowSettings) return;
    ImGui::SetNextWindowSize(ImVec2(780.0f, 560.0f), ImGuiCond_FirstUseEver);
    ImGui::Begin("Configurações", &m_ShowSettings);
    kizuri::editor::icons::PanelHeader("CONFIGURAÇÕES", kizuri::editor::icons::Settings);

    ImGui::BeginChild("##settings_sidebar", ImVec2(150.0f, 0.0f), true);
    const char* sections[] = { "Gráficos", "Geral", "Editor" };
    for (int i = 0; i < 3; ++i) {
        if (ImGui::Selectable(sections[i], m_SettingsSection == i)) m_SettingsSection = i;
    }
    ImGui::EndChild();
    ImGui::SameLine();
    ImGui::BeginChild("##settings_body");
    if (m_SettingsSection == 1) {
        DrawSettingsGeneral();
    } else if (m_SettingsSection == 2) {
        DrawSettingsEditor();
    } else {
        DrawSettingsGraphics();
    }
    ImGui::EndChild();

    ImGui::End();
}

// Seção Gráficos das Configurações: qualidade, MSAA/SSAO/bloom/fog/HDRI.
void EditorLayer::DrawSettingsGraphics() {
    // Combo SEMPRE visível (inclui "Custom" como item) — clicar num preset
    // sempre re-aplica na hora (o antigo escondia o combo em Custom e clicar
    // no preset já selecionado não fazia nada).
    const char* presets[] = { "Ultra", "High", "Medium", "Low", "Custom" };
    int presetIdx = (int)m_GraphicsSettings.Preset;
    bool presetApplied = false;
    if (ImGui::Combo("Qualidade", &presetIdx, presets, 5)) {
        if (presetIdx == 4) {
            m_GraphicsSettings.Preset = kizuri::QualityPreset::Custom; // mantém valores atuais
        } else {
            m_GraphicsSettings.ApplyPreset((kizuri::QualityPreset)presetIdx);
            kizuri::Renderer3D::SetGraphicsSettings(m_GraphicsSettings);
        }
        presetApplied = true;
    } else if ((int)m_GraphicsSettings.Preset == 4) {
        presetApplied = true; // já está em Custom: nenhum preset pra re-aplicar
    }

    bool customTweak = false;
    customTweak |= ImGui::DragFloat("Resolução interna", &m_GraphicsSettings.RenderScale, 0.01f, 0.25f, 2.0f);
    const char* msaaNames[] = { "Desligado", "1x", "2x", "4x", "8x" };
    int msaaValues[] = { 0, 1, 2, 4, 8 };
    int msaaIdx = 0;
    for (int i = 0; i < 5; ++i) if (m_GraphicsSettings.MSAA == msaaValues[i]) msaaIdx = i;
    if (ImGui::Combo("MSAA", &msaaIdx, msaaNames, 5)) m_GraphicsSettings.MSAA = msaaValues[msaaIdx];
    customTweak |= (ImGui::IsItemActive() || ImGui::IsItemActivated());

    const char* shadowNames[] = { "512", "1024", "2048", "4096" };
    int shadowValues[] = { 512, 1024, 2048, 4096 };
    int shadowIdx = 0;
    for (int i = 0; i < 4; ++i) if (m_GraphicsSettings.ShadowMapSize == shadowValues[i]) shadowIdx = i;
    if (ImGui::Combo("Shadow map (CSM)", &shadowIdx, shadowNames, 4)) m_GraphicsSettings.ShadowMapSize = shadowValues[shadowIdx];
    customTweak |= (ImGui::IsItemActive() || ImGui::IsItemActivated());
    customTweak |= ImGui::SliderInt("Suavização de sombra (PCF)", &m_GraphicsSettings.ShadowPCFRadius, 0, 3);
    customTweak |= ImGui::DragFloat("Penumbra (PCSS)", &m_GraphicsSettings.ShadowSoftness, 0.01f, 0.0f, 1.0f);
    ImGui::Separator();
    customTweak |= ImGui::Checkbox("Bloom", &m_GraphicsSettings.BloomEnabled);
    if (m_GraphicsSettings.BloomEnabled) {
        customTweak |= ImGui::DragFloat("Limiar do bloom", &m_GraphicsSettings.BloomThreshold, 0.01f, 0.1f, 10.0f);
        customTweak |= ImGui::DragFloat("Intensidade do bloom", &m_GraphicsSettings.BloomIntensity, 0.01f, 0.0f, 3.0f);
    }
    customTweak |= ImGui::SliderInt("Iterações do bloom (glow)", &m_GraphicsSettings.BloomIterations, 1, 12);
    const char* tmNames[] = { "ACES (cinematográfico)", "Reinhard (suave)", "Filmic (contraste)" };
    customTweak |= ImGui::Combo("Tonemapping", &m_GraphicsSettings.ToneMapping, tmNames, 3);
    ImGui::Separator();
    customTweak |= ImGui::Checkbox("SSAO", &m_GraphicsSettings.SSAOEnabled);
    if (m_GraphicsSettings.SSAOEnabled) {
        customTweak |= ImGui::SliderInt("Amostras SSAO", &m_GraphicsSettings.SSAOSamples, 8, 64);
        customTweak |= ImGui::DragFloat("Raio SSAO", &m_GraphicsSettings.SSAORadius, 0.01f, 0.05f, 2.0f);
    }
    ImGui::Separator();
    // SSR (reflexos em espaço de tela) — agora 3.3-safe: loop de passos fixos
    // (constante no shader), funciona em qualquer driver GL 3.3 core.
    customTweak |= ImGui::Checkbox("Reflexos por raio (SSR)", &m_GraphicsSettings.SSREnabled);
    if (m_GraphicsSettings.SSREnabled) {
        customTweak |= ImGui::SliderInt("Passos do raio", &m_GraphicsSettings.SSRMaxSteps, 8, 48);
        customTweak |= ImGui::DragFloat("Intensidade da reflexão", &m_GraphicsSettings.SSRIntensity, 0.01f, 0.0f, 2.0f);
        customTweak |= ImGui::DragFloat("Distância da marcha", &m_GraphicsSettings.SSRMarchDistance, 0.5f, 1.0f, 100.0f);
        customTweak |= ImGui::DragFloat("Espessura do depth", &m_GraphicsSettings.SSRThickness, 0.005f, 0.01f, 1.0f);
    }
    ImGui::Separator();
    customTweak |= ImGui::DragFloat("Exposição", &m_GraphicsSettings.Exposure, 0.01f, 0.1f, 8.0f);
    customTweak |= ImGui::Checkbox("Anti-aliasing temporal (TAA)", &m_GraphicsSettings.TAAEnabled);
    customTweak |= ImGui::Checkbox("God rays (luz volumétrica)", &m_GraphicsSettings.GodRaysEnabled);
    if (m_GraphicsSettings.GodRaysEnabled)
        customTweak |= ImGui::DragFloat("Intensidade dos god rays", &m_GraphicsSettings.GodRaysIntensity, 0.01f, 0.0f, 3.0f);
    customTweak |= ImGui::Checkbox("Depth of field (bokeh)", &m_GraphicsSettings.DOFEnabled);
    if (m_GraphicsSettings.DOFEnabled) {
        customTweak |= ImGui::DragFloat("Distância focal", &m_GraphicsSettings.DOFFocusDistance, 0.1f, 0.1f, 500.0f);
        customTweak |= ImGui::DragFloat("Faixa em foco", &m_GraphicsSettings.DOFFocusRange, 0.1f, 0.1f, 100.0f);
        customTweak |= ImGui::DragFloat("Força do bokeh", &m_GraphicsSettings.DOFStrength, 0.05f, 0.0f, 5.0f);
    }
    customTweak |= ImGui::Checkbox("Motion blur", &m_GraphicsSettings.MotionBlurEnabled);
    if (m_GraphicsSettings.MotionBlurEnabled)
        customTweak |= ImGui::DragFloat("Intensidade do motion blur", &m_GraphicsSettings.MotionBlurIntensity, 0.01f, 0.0f, 2.0f);
    ImGui::Separator();
    customTweak |= ImGui::Checkbox("SSGI (iluminação global)", &m_GraphicsSettings.SSGIEnabled);
    if (m_GraphicsSettings.SSGIEnabled)
        customTweak |= ImGui::DragFloat("Intensidade SSGI", &m_GraphicsSettings.SSGIIntensity, 0.01f, 0.0f, 2.0f);
    customTweak |= ImGui::Checkbox("Nuvens volumétricas", &m_GraphicsSettings.CloudsEnabled);
    customTweak |= ImGui::Checkbox("Lens flare", &m_GraphicsSettings.LensFlareEnabled);
    if (m_GraphicsSettings.LensFlareEnabled)
        customTweak |= ImGui::DragFloat("Intensidade do lens flare", &m_GraphicsSettings.LensFlareIntensity, 0.01f, 0.0f, 3.0f);
    customTweak |= ImGui::Checkbox("FXAA (anti-aliasing extra)", &m_GraphicsSettings.FXAAEnabled);
    ImGui::Separator();
    customTweak |= ImGui::DragFloat("Saturação", &m_GraphicsSettings.Saturation, 0.01f, 0.0f, 2.0f);
    customTweak |= ImGui::DragFloat("Contraste", &m_GraphicsSettings.Contrast, 0.01f, 0.0f, 2.0f);
    customTweak |= ImGui::DragFloat("Bloom anamórfico", &m_GraphicsSettings.BloomAnamorphic, 0.01f, 0.0f, 1.0f);
    ImGui::Separator();
    customTweak |= ImGui::DragFloat("Altura da névoa", &m_GraphicsSettings.FogHeight, 0.1f, -100.0f, 100.0f);
    customTweak |= ImGui::DragFloat("Atenuação da névoa por altura", &m_GraphicsSettings.FogHeightFalloff, 0.1f, 0.0f, 200.0f);
    customTweak |= ImGui::Checkbox("VSync", &m_GraphicsSettings.VSync);
    ImGui::Separator();
    customTweak |= ImGui::Checkbox("Névoa (fog exponencial)", &m_GraphicsSettings.FogEnabled);
    if (m_GraphicsSettings.FogEnabled) {
        customTweak |= ImGui::DragFloat("Densidade da névoa", &m_GraphicsSettings.FogDensity, 0.001f, 0.0f, 0.2f);
        customTweak |= ImGui::ColorEdit3("Cor da névoa", m_GraphicsSettings.FogColor);
    }
    ImGui::Separator();
    customTweak |= ImGui::DragFloat("Vinheta", &m_GraphicsSettings.Vignette, 0.01f, 0.0f, 1.0f);
    customTweak |= ImGui::DragFloat("Aberração cromática", &m_GraphicsSettings.ChromaticAberration, 0.0005f, 0.0f, 0.02f);
    customTweak |= ImGui::DragFloat("Grão de filme", &m_GraphicsSettings.FilmGrain, 0.005f, 0.0f, 0.2f);

    ImGui::Separator();
    ImGui::Checkbox("Céu atmosférico Rayleigh/Mie", &m_GraphicsSettings.AtmosphereSky);
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        m_GraphicsSettings.Preset = kizuri::QualityPreset::Custom;
        kizuri::Renderer3D::SetGraphicsSettings(m_GraphicsSettings);
        SaveGraphicsSettingsToDisk();
    }
    ImGui::TextDisabled("Raymarch físico — desligue em GPUs fracas/emuladores (pontilhado).");
    ImGui::TextDisabled("Ambiente (céu) — vazio = procedural, ou um .hdr equirectangular:");
    bool applyHDRI = false;
    ImGui::InputText("HDRI do céu", m_EnvironmentHDRIPathBuffer, sizeof(m_EnvironmentHDRIPathBuffer));
    std::string hdriPick;
    if (FileBrowseButton("HDRI (céu)", "*.hdr;*.exr", hdriPick)) {
        strncpy(m_EnvironmentHDRIPathBuffer, hdriPick.c_str(), sizeof(m_EnvironmentHDRIPathBuffer) - 1);
        m_EnvironmentHDRIPathBuffer[sizeof(m_EnvironmentHDRIPathBuffer) - 1] = '\0';
        applyHDRI = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(m_EnvironmentHDRIPathBuffer);
    ImGui::SameLine();
    applyHDRI |= ImGui::Button("Aplicar");
    if (ImGui::Button("Voltar ao céu procedural")) {
        m_EnvironmentHDRIPathBuffer[0] = '\0';
        applyHDRI = true;
    }

    ImGui::Separator();
    if (ImGui::Button("Salvar")) SaveGraphicsSettingsToDisk();
    ImGui::SameLine();
    if (ImGui::Button("Restaurar padrão (Ultra)")) m_GraphicsSettings.ApplyPreset(kizuri::QualityPreset::Ultra);

    if (customTweak && !presetApplied) m_GraphicsSettings.Preset = kizuri::QualityPreset::Custom;
    m_GraphicsSettings.Clamp();

    // Aplica em runtime (recursos que dependem de tamanho/MSAA recriados
    // lazy no próximo frame pelo Renderer3D).
    kizuri::Renderer3D::SetGraphicsSettings(m_GraphicsSettings);
    Application& app = Application::Get();
    if (app.GetWindow().IsVSync() != m_GraphicsSettings.VSync)
        app.GetWindow().SetVSync(m_GraphicsSettings.VSync);
    if (applyHDRI) kizuri::Renderer3D::SetEnvironmentHDRIPath(m_EnvironmentHDRIPathBuffer);
}

// Seção Geral: projeto, janela e persistência.
void EditorLayer::DrawSettingsGeneral() {
    auto& project = Project::GetActive();
    ImGui::TextUnformatted("Projeto");
    ImGui::Separator();
    if (project) {
        // Project Settings: nome, cena inicial e GameModule editáveis
        // (persistem no .kzproj via Project::Save).
        static char s_nameBuf[128] = { 0 };
        static char s_startBuf[512] = { 0 };
        static char s_moduleBuf[512] = { 0 };
        static bool s_projInited = false;
        if (!s_projInited) {
            strncpy(s_nameBuf, project->GetConfig().Name.c_str(), sizeof(s_nameBuf) - 1);
            strncpy(s_startBuf, project->GetConfig().StartScenePath.c_str(), sizeof(s_startBuf) - 1);
            strncpy(s_moduleBuf, project->GetConfig().GameModulePath.c_str(), sizeof(s_moduleBuf) - 1);
            s_projInited = true;
        }
        ImGui::InputText("Nome do projeto", s_nameBuf, sizeof(s_nameBuf));
        ImGui::InputText("Cena inicial", s_startBuf, sizeof(s_startBuf));
        std::string startPick;
        if (FileBrowseButton("Cena inicial", "*.kzscene", startPick)) {
            strncpy(s_startBuf, startPick.c_str(), sizeof(s_startBuf) - 1);
            s_startBuf[sizeof(s_startBuf) - 1] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(s_startBuf);
        ImGui::InputText("GameModule (DLL)", s_moduleBuf, sizeof(s_moduleBuf));
        std::string modulePick;
        if (FileBrowseButton("GameModule", "*.dll", modulePick)) {
            strncpy(s_moduleBuf, modulePick.c_str(), sizeof(s_moduleBuf) - 1);
            s_moduleBuf[sizeof(s_moduleBuf) - 1] = '\0';
        }
        ImGui::SameLine();
        if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(s_moduleBuf);
        if (ImGui::Button("Salvar configurações do projeto")) {
            project->GetConfig().Name = s_nameBuf;
            project->GetConfig().StartScenePath = s_startBuf;
            project->GetConfig().GameModulePath = s_moduleBuf;
            project->Save();
        }
        ImGui::SameLine();
        if (ImGui::Button("Voltar a carregar do disco")) s_projInited = false;
        ImGui::Spacing();
        ImGui::TextWrapped("Caminho: %s", project->GetFilePath().c_str());
        ImGui::Text("Modo: %s",
            project->GetConfig().DefaultMode == ProjectMode::TwoD ? "2D" :
            project->GetConfig().DefaultMode == ProjectMode::ThreeD ? "3D" : "Vazio");
        ImGui::TextWrapped("Pasta de assets: %s", project->GetAssetDirectory().c_str());
    } else {
        ImGui::TextDisabled("Nenhum projeto aberto.");
    }
    ImGui::Spacing();
    ImGui::TextUnformatted("Janela");
    ImGui::Separator();
    Application& app = Application::Get();
    static int winW = (int)app.GetWindow().GetWidth();
    static int winH = (int)app.GetWindow().GetHeight();
    static bool s_winEdited = false;
    // Mantém os campos sincronizados quando a janela muda por fora (ex.: o
    // usuário redimensionou manualmente e reabriu as Configurações).
    if (!s_winEdited) {
        winW = (int)app.GetWindow().GetWidth();
        winH = (int)app.GetWindow().GetHeight();
    }
    ImGui::SetNextItemWidth(110); ImGui::InputInt("Largura", &winW);
    if (ImGui::IsItemEdited()) s_winEdited = true;
    ImGui::SetNextItemWidth(110); ImGui::InputInt("Altura", &winH);
    if (ImGui::IsItemEdited()) s_winEdited = true;
    ImGui::SameLine();
    if (ImGui::Button("Aplicar resolução")) {
        winW = std::max(640, winW); winH = std::max(360, winH);
        app.GetWindow().SetSize(winW, winH);
        s_winEdited = false;
    }
    if (ImGui::Button("Maximizar")) app.GetWindow().ToggleMaximize();
    ImGui::SameLine();
    if (ImGui::Button("Minimizar")) app.GetWindow().Minimize();
    if (ImGui::Checkbox("VSync", &m_GraphicsSettings.VSync))
        app.GetWindow().SetVSync(m_GraphicsSettings.VSync);
    ImGui::Spacing();
    ImGui::TextUnformatted("Renderização");
    ImGui::Separator();
    ImGui::Text("OpenGL: %s", kizuri::GetOpenGLVersionString().c_str());
    ImGui::TextDisabled("Qualidade automática pelo hardware; configurações avançadas em Gráficos.");
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("OpenGL %d.%d detectado. Quanto mais novo, mais efeitos ligam automaticamente.",
                          kizuri::GetGLSLVersion() / 100, (kizuri::GetGLSLVersion() / 10) % 10);

    ImGui::Spacing();
    ImGui::TextUnformatted("Persistência");
    ImGui::Separator();
    if (ImGui::Button("Salvar configurações (settings.json)")) SaveGraphicsSettingsToDisk();
    ImGui::SameLine();
    if (ImGui::Button("Carregar do disco")) LoadGraphicsSettingsFromDisk();
    ImGui::Spacing();
    ImGui::TextDisabled("settings.json fica no diretório de trabalho (bin/).");
}

// Seção Editor: comportamento do editor.
void EditorLayer::DrawSettingsEditor() {
    ImGui::TextUnformatted("Play");
    ImGui::Separator();
    ImGui::Checkbox("Compilar C# no Play (estilo Unity)", &m_AutoCompileOnPlay);
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Roda dotnet build no Source/*.csproj antes de entrar no Play.");
    ImGui::Checkbox("Play no viewport usa a CÂMERA DO JOGO", &m_PlayUsesGameCamera);
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Mostra no viewport principal o que o jogador vê (WASD move o personagem). Desligue pra voar pela cena com a câmera do editor.");
    ImGui::Spacing();

    ImGui::TextUnformatted("Câmera do editor");
    ImGui::Separator();
    ImGui::DragFloat("Velocidade de voo", &m_EditorCamFlySpeed, 0.1f, 0.5f, 60.0f);
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Botão direito + WASD no viewport 3D. Segurar Shift triplica a velocidade.");
    ImGui::DragFloat("Sensibilidade do mouse", &m_EditorCamSensitivity, 0.005f, 0.01f, 1.0f);
    ImGui::Spacing();

    ImGui::TextUnformatted("Gizmos");
    ImGui::Separator();
    ImGui::DragFloat("Snap de translação", &m_GizmoSnapTranslation, 0.05f, 0.05f, 10.0f);
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Segure Ctrl enquanto arrasta o gizmo de mover pra usar o snap.");
    ImGui::DragFloat("Snap de rotação (graus)", &m_GizmoSnapRotation, 1.0f, 1.0f, 90.0f);
    ImGui::Spacing();

    ImGui::TextUnformatted("Viewport");
    ImGui::Separator();
    ImGui::Checkbox("Mostrar estatísticas (FPS / draw calls / triângulos)", &m_ShowStats);
    ImGui::SameLine();
    if (ImGui::Button("Diagnóstico de Texto")) m_ShowTextDiag = !m_ShowTextDiag;
    ImGui::SameLine();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Janela que mostra o atlas da fonte + estado do blending — pra diagnosticar texto em retângulo branco.");
    ImGui::Checkbox("Mostrar colliders de todos os objetos (overlay de física)", &m_ShowColliders);
    ImGui::Checkbox("Maximizar viewport (botão fullscreen da toolbar)", &m_ViewportMaximized);
    ImGui::Spacing();
    ImGui::TextUnformatted("Cenas de demonstração");
    ImGui::Separator();
    if (ImGui::Button("Criar demonstração 2D")) CreateDemoScene2D();
    ImGui::SameLine();
    if (ImGui::Button("Criar demonstração 3D")) CreateDemoScene3D();
    ImGui::SameLine();
    if (ImGui::Button("Criar demo IA")) CreateDemoSceneAI();
    ImGui::SameLine();
    if (ImGui::Button("Criar demo Rede")) CreateDemoSceneNet();
    ImGui::SameLine();
    if (ImGui::Button("Criar DEMO COMPLETA (mini-jogo)")) CreateDemoSceneGame();
    ImGui::TextDisabled("Demo completa: colete as 8 moedas fugindo dos inimigos — WASD pra mover, pontuação no placar.");
}

void EditorLayer::DrawGizmo() {
    KZ_TRACE_SCOPE("EditorLayer::DrawGizmo");
    if (!m_SelectedEntity || !m_SelectedEntity.HasComponent<TransformComponent>()) return;

    // Atalhos de operação só valem com o viewport focado e a câmera livre
    // desativada (botão direito solto) — senão W entraria em conflito com
    // o "andar pra frente" da fly camera. E nada disso vale enquanto o
    // usuário está DIGITANDO num campo de texto (ex.: renomeando objeto) —
    // ImGui marcou WantTextInput; W/E/R devem ir pro campo, não pro gizmo.
    bool flying = Input::IsMouseButtonPressed(Mouse::Right);
    if (m_ViewportHovered && !flying && !ImGui::GetIO().WantTextInput) {
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
    float snapAmount = (m_GizmoOperation == ImGuizmo::OPERATION::ROTATE) ? m_GizmoSnapRotation : m_GizmoSnapTranslation;
    float snapValues[3] = { snapAmount, snapAmount, snapAmount };

    // No modo 2D, o eixo Z do gizmo fica de perfil pra câmera ortográfica
    // (que olha reto por Z) e o cone/seta desse eixo achata numa mancha
    // triangular colorida, com as linhas dos outros eixos em leque — era
    // esse o "triângulo verde" bugado no viewport 2D. Em 2D só faz sentido
    // manipular X/Y (translate) ou rotação em Z (rotate); então filtramos
    // a operação pros eixos relevantes em vez de usar o enum cheio de 3D.
    ImGuizmo::OPERATION op = m_GizmoOperation;
    if (m_ViewportMode == ViewportMode::Mode2D) {
        if (m_GizmoOperation == ImGuizmo::TRANSLATE)
            op = (ImGuizmo::OPERATION)(ImGuizmo::TRANSLATE_X | ImGuizmo::TRANSLATE_Y);
        else if (m_GizmoOperation == ImGuizmo::SCALE)
            op = (ImGuizmo::OPERATION)(ImGuizmo::SCALE_X | ImGuizmo::SCALE_Y);
        else if (m_GizmoOperation == ImGuizmo::ROTATE)
            op = ImGuizmo::ROTATE_Z;
    }

    ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(proj),
                          op, ImGuizmo::LOCAL,
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
    if (m_SceneLoading || m_PlayBuildActive) return;

    // Compilar no Play, estilo Unity, em SEGUNDO PLANO: dotnet build pode
    // levar segundos (e baixar pacotes na 1ª vez) — síncrono travava o
    // editor. Enquanto compila, um overlay de "Compilando C#..." aparece e
    // o Play entra quando o build termina (consumido no OnUpdate).
    if (m_AutoCompileOnPlay) {
        std::string csproj, engineRoot;
        GetGameBuildInfo(csproj, engineRoot);
        if (!csproj.empty()) {
            // Nunca deixa uma thread antiga do build viva ao iniciar outro:
            // reassignar std::thread joinable chamava std::terminate (a engine
            // fechava no 2º Play quando o 1º build tinha falhado).
            if (m_PlayBuildThread.joinable()) m_PlayBuildThread.join();
            m_PlayBuildError.clear();
            m_PlayBuildDll.clear();
            m_PlayBuildOk = false;
            m_PlayBuildDone = false;
            m_PlayBuildCancelled = false;
            m_PlayBuildActive = true;
            m_PlayBuildThread = std::thread([this, csproj, engineRoot]() {
                std::string dll, err;
                m_PlayBuildOk = GameExporter::BuildGameModule(csproj, engineRoot, dll, err);
                m_PlayBuildDll = dll;
                m_PlayBuildError = err;
                m_PlayBuildDone = true;
            });
            return; // entra no Play quando o build terminar
        }
    }

    StartPlayInternal();
}

// Entra de fato no Play (cópia da cena + runtime) — usado direto pelo
// OnScenePlay quando não há build pendente, ou pelo OnUpdate quando o
// build em segundo plano termina.
void EditorLayer::StartPlayInternal() {
    if (m_PlayBuildThread.joinable()) m_PlayBuildThread.join();

    m_EditorScene = m_ActiveScene;
    m_ActiveScene = Scene::Copy(m_EditorScene);
    m_ActiveScene->OnViewportResize((uint32_t)m_ViewportSize.x, (uint32_t)m_ViewportSize.y);
    // Play usa a câmera da PRÓPRIA cena, exatamente como autorada (a câmera
    // livre do editor é só navegação) — igual Unity/Godot.
    m_SelectedEntity = {}; // handle da cena antiga não é válido na cópia
    ClearMultiSelection();
    m_SceneState = SceneState::Play;
    m_ActiveScene->OnRuntimeStart();
}

void EditorLayer::OnSceneStop() {
    KZ_TRACE_SCOPE("EditorLayer::OnSceneStop");

    // Se um build de Play está em andamento, cancela a entrada no Play.
    if (m_PlayBuildActive) {
        m_PlayBuildCancelled = true;
        return;
    }

    m_ActiveScene->OnRuntimeStop();
    AudioEngine::StopAll(); // OnRuntimeStop só cuida de física/scripts — sem isso, som ficava tocando pra sempre

    // Ajustes de CÂMERA feitos durante o Play (inspetor / Game View) voltam
    // pra cena original — "mexeu ao vivo, não perde ao parar".
    if (m_ActiveScene && m_EditorScene) {
        auto& copyReg = m_ActiveScene->GetRegistry();
        auto camView = copyReg.view<kizuri::TransformComponent, kizuri::CameraComponent>();
        for (auto e : camView) {
            kizuri::Entity copyEnt{ e, m_ActiveScene.get() };
            auto& copyCam = camView.get<kizuri::CameraComponent>(e);
            kizuri::Entity orig = m_EditorScene->GetEntityByUUID(copyEnt.GetUUID());
            if (!orig || !orig.HasComponent<kizuri::CameraComponent>()) continue;
            auto& oc = orig.GetComponent<kizuri::CameraComponent>();
            oc.Type = copyCam.Type;
            oc.Primary = copyCam.Primary;
            oc.OrthoSize = copyCam.OrthoSize;
            oc.PerspectiveFOV = copyCam.PerspectiveFOV;
            oc.NearClip = copyCam.NearClip;
            oc.FarClip = copyCam.FarClip;
            if (orig.HasComponent<kizuri::TransformComponent>() && copyReg.all_of<kizuri::TransformComponent>(e)) {
                auto& ot = orig.GetComponent<kizuri::TransformComponent>();
                auto& ct = copyReg.get<kizuri::TransformComponent>(e);
                ot.Translation = ct.Translation;
                ot.Rotation = ct.Rotation;
                ot.Scale = ct.Scale;
            }
        }
        KZ_CORE_INFO("Play encerrado: ajustes de câmera preservados na cena.");
    }

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
    CreateDefaultSceneContent();
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
    if (lower.size() > 8 && lower.substr(lower.size() - 8) == ".kzprefab") {
        created = m_ActiveScene->Instantiate(path, worldPos);
        if (created) m_History.Push(CreateRef<CreateEntityCommand>(created));
        return created;
    }
    if (lower.size() > 4 && lower.substr(lower.size() - 4) == ".obj") {
        created = m_ActiveScene->CreateEntity(std::filesystem::path(path).stem().string());
        auto& mr = created.AddComponent<MeshRendererComponent>();
        mr.MeshSource = Project::MakeRelativePath(path);
        mr.MeshAsset = Mesh::FromSource(path);
    } else if ((lower.size() > 4 && (lower.substr(lower.size() - 4) == ".glb" || lower.substr(lower.size() - 4) == ".gltf"))) {
        created = m_ActiveScene->CreateEntity(std::filesystem::path(path).stem().string());
        auto& mr = created.AddComponent<MeshRendererComponent>();
        mr.MeshSource = Project::MakeRelativePath(path);
        mr.MeshAsset = Mesh::FromSource(path);
        mr.MeshMaterial = Mesh::ExtractMaterialFromGLTF(path); // material PBR do modelo
    } else {
        std::string ext = lower.size() >= 4 ? lower.substr(lower.size() - 4) : "";
        bool isImage = (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga" || ext == ".gif");
        if (!isImage) return {};
        created = m_ActiveScene->CreateEntity(std::filesystem::path(path).stem().string());
        auto& sc = created.AddComponent<SpriteRendererComponent>();
        sc.TexturePath = Project::MakeRelativePath(path);
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
    if (m_SceneLoading) {
        KZ_CORE_WARN("EditorLayer::OpenScene — já existe uma cena sendo carregada; ignorando '{0}'.", path);
        return;
    }

    // Carregamento ASSÍNCRONO: prepara o SceneSerializer e devolve na hora.
    // Cada OnUpdate processa um lote (por orçamento de tempo), então a janela
    // continua respondendo mesmo com projetos gigantes — e o usuário pode
    // fechar/desistir sem a engine travar.
    auto newScene = CreateRef<Scene>("Carregando...");
    auto loader = std::make_unique<SceneSerializer>(newScene);
    if (!loader->BeginDeserializeStepwiseFile(path)) return;

    m_PendingScene = newScene;
    m_PendingLoader = std::move(loader);
    m_PendingScenePath = path;
    m_PendingLoadProgress = 0.0f;
    m_SceneLoading = true;
    KZ_CORE_INFO("Carregando cena em segundo plano: {0}", path);
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
    const char* menuNames[] = { "Arquivo", "Editar", "Cena", "Exibir", "Janelas", "Ajuda" };
    for (int m = 0; m < 6; ++m) {
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 4.0f));
        if (ImGui::Button(menuNames[m])) ImGui::OpenPopup(("##menu_" + std::string(menuNames[m])).c_str());
        ImGui::PopStyleVar();
        ImGui::PopStyleColor();
        if (m < 5) ImGui::SameLine(0.0f, 2.0f);
    }

    // ---- Arquivo ----
    if (ImGui::BeginPopup("##menu_Arquivo")) {
        if (ImGui::MenuItem("Novo Projeto...")) {
            strncpy(m_NewProjectDirBuffer, "MeuJogo", sizeof(m_NewProjectDirBuffer));
            strncpy(m_NewProjectNameBuffer, "MeuJogo", sizeof(m_NewProjectNameBuffer));
            m_RequestOpenNewProjectPopup = true;
        }
        if (ImGui::MenuItem("Abrir Projeto...")) {
            m_RequestOpenLoadProjectPopup = true;
        }
        if (ImGui::MenuItem("Voltar ao Início", nullptr, false, m_SceneState == SceneState::Edit)) {
            m_EditorState = EditorState::Hub;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Exportar Jogo...", nullptr, false,
                            m_SceneState == SceneState::Edit && !m_ScenePath.empty())) {
            m_RequestOpenExportPopup = true;
        }
        if (ImGui::MenuItem("Definir cena como inicial", nullptr, false,
                            m_SceneState == SceneState::Edit && !m_ScenePath.empty() && (bool)Project::GetActive())) {
            auto& project = Project::GetActive();
            project->GetConfig().StartScenePath = Project::MakeRelativePath(m_ScenePath);
            project->Save();
            KZ_CORE_INFO("Cena inicial do projeto: {0}", project->GetConfig().StartScenePath);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Salvar Cena", nullptr, false, (bool)m_ActiveScene)) SaveScene();
        if (ImGui::MenuItem("Salvar Cena Como...")) SaveSceneAs();
        ImGui::Separator();
        // Fluxo normal é automático (abrir projeto carrega, Play compila). O
        // "Carregar GameModule" é fallback pra cena solta — fica em Avançado.
        if (ImGui::BeginMenu("Avançado")) {
            if (ImGui::MenuItem("Carregar GameModule...", nullptr, false, m_SceneState == SceneState::Edit)) {
                m_RequestOpenGameModulePopup = true;
            }
            ImGui::EndMenu();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Sair")) Application::Get().Close();
        ImGui::EndPopup();
    }

    // ---- Editar (undo/redo/duplicar/excluir) ----
    if (ImGui::BeginPopup("##menu_Editar")) {
        bool editing = m_SceneState == SceneState::Edit;
        if (ImGui::MenuItem("Desfazer", "Ctrl+Z", false, editing && m_History.CanUndo()))
            m_History.Undo(*m_ActiveScene);
        if (ImGui::MenuItem("Refazer", "Ctrl+Y", false, editing && m_History.CanRedo()))
            m_History.Redo(*m_ActiveScene);
        ImGui::Separator();
        if (ImGui::MenuItem("Duplicar entidade", "Ctrl+D", false, editing && m_SelectedEntity)) {
            Entity copy = m_ActiveScene->DuplicateEntity(m_SelectedEntity);
            if (copy) { m_SelectedEntity = copy; AutoSwitchViewportMode(); }
        }
        if (ImGui::MenuItem("Excluir entidade", "Del", false, editing && m_SelectedEntity)) {
            auto multi = GetMultiSelection();
            if (multi.size() > 1) {
                for (auto& e : multi) {
                    m_History.Push(CreateRef<DeleteEntityCommand>(e));
                    m_ActiveScene->DestroyEntity(e);
                }
            } else {
                Entity toDelete = m_SelectedEntity;
                m_History.Push(CreateRef<DeleteEntityCommand>(toDelete));
                m_ActiveScene->DestroyEntity(toDelete);
            }
            m_SelectedEntity = {};
            ClearMultiSelection();
            ClearMultiSelection();
        }
        ImGui::EndPopup();
    }

    // ---- Cena ----
    if (ImGui::BeginPopup("##menu_Cena")) {
        if (ImGui::MenuItem("Nova Cena", nullptr, false, m_SceneState == SceneState::Edit)) NewScene();
        if (ImGui::MenuItem("Abrir Cena...", nullptr, false, m_SceneState == SceneState::Edit)) {
            strncpy(m_ScenePathBuffer, m_ScenePath.empty() ? "cena.kzscene" : m_ScenePath.c_str(), sizeof(m_ScenePathBuffer));
            m_ScenePathBuffer[sizeof(m_ScenePathBuffer) - 1] = '\0';
            m_RequestOpenLoadPopup = true;
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Cena de Demonstração 2D...", nullptr, false, m_SceneState == SceneState::Edit))
            CreateDemoScene2D();
        if (ImGui::MenuItem("Cena de Demonstração IA...", nullptr, false, m_SceneState == SceneState::Edit))
            CreateDemoSceneAI();
        if (ImGui::MenuItem("Cena de Demonstração Rede...", nullptr, false, m_SceneState == SceneState::Edit))
            CreateDemoSceneNet();
        if (ImGui::MenuItem("Cena de Demonstração 3D...", nullptr, false, m_SceneState == SceneState::Edit))
            CreateDemoScene3D();
        if (ImGui::MenuItem("Cena DEMO COMPLETA (mini-jogo)...", nullptr, false, m_SceneState == SceneState::Edit))
            CreateDemoSceneGame();
        ImGui::EndPopup();
    }

    // ---- Exibir ----
    if (ImGui::BeginPopup("##menu_Exibir")) {
        if (ImGui::MenuItem("Viewport 2D", nullptr, m_ViewportMode == ViewportMode::Mode2D)) m_ViewportMode = ViewportMode::Mode2D;
        if (ImGui::MenuItem("Viewport 3D", nullptr, m_ViewportMode == ViewportMode::Mode3D)) m_ViewportMode = ViewportMode::Mode3D;
        ImGui::Separator();
        if (ImGui::MenuItem("Fullscreen do viewport", "F11", m_ViewportMaximized)) m_ViewportMaximized = !m_ViewportMaximized;
        ImGui::Separator();
        if (ImGui::MenuItem("Project Settings...", "Ctrl+,"))
            for (auto& p : m_Panels) if (std::string(p->GetTitle()) == "Project Settings") { p->SetVisible(true); break; }
        ImGui::EndPopup();
    }

    // ---- Janelas ----
    if (ImGui::BeginPopup("##menu_Janelas")) {
        ImGui::TextDisabled("Painéis (mostrar/ocultar)");
        ImGui::Separator();
        for (auto& panel : m_Panels) {
            bool visible = panel->IsVisible();
            if (ImGui::MenuItem(panel->GetTitle(), nullptr, visible)) panel->SetVisible(!visible);
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Project Settings...", "Ctrl+,"))
            for (auto& p : m_Panels) if (std::string(p->GetTitle()) == "Project Settings") { p->SetVisible(true); break; }
        ImGui::EndPopup();
    }

    // ---- Ajuda ----
    if (ImGui::BeginPopup("##menu_Ajuda")) {
        ImGui::TextDisabled("Kizuri Engine v%s", KIZURI_VERSION);
        ImGui::TextDisabled("C++20 · OpenGL %s · GLSL %d core",
            kizuri::GetOpenGLVersionString().c_str(), kizuri::GetGLSLVersion());
        ImGui::Separator();
        if (ImGui::MenuItem("Verificar Atualizações...")) {
            m_UpdateApiUrlBufInput = kizuri::Updater::GetApiUrl();
            if (!m_UpdateApiUrlBufInput.empty() && m_UpdateApiUrlBufInput.size() < sizeof(m_UpdateApiUrlBuf) - 1)
                strncpy(m_UpdateApiUrlBuf, m_UpdateApiUrlBufInput.c_str(), sizeof(m_UpdateApiUrlBuf) - 1);
            m_UpdateApiUrlBuf[sizeof(m_UpdateApiUrlBuf) - 1] = '\0';
            StartUpdateCheck();
        }
        if (ImGui::MenuItem("Configurar Atualizações..."))
            ImGui::OpenPopup("##update_config");
        if (ImGui::BeginPopup("##update_config")) {
            ImGui::TextDisabled("API do site (GET /api/version):");
            ImGui::SetNextItemWidth(400.0f);
            if (ImGui::InputText("##update_api_url", m_UpdateApiUrlBuf, sizeof(m_UpdateApiUrlBuf)))
                m_UpdateApiUrlBufInput = m_UpdateApiUrlBuf;
            if (ImGui::Button("Salvar", ImVec2(100.0f, 0.0f))) {
                kizuri::Updater::SetApiUrl(m_UpdateApiUrlBufInput);
                KZ_CORE_INFO("API de atualizações: {0}", kizuri::Updater::GetApiUrl());
                StartUpdateCheck();
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        ImGui::Separator();
        if (ImGui::MenuItem("Atalhos do editor"))
            ImGui::OpenPopup("##atalhos");
        if (ImGui::BeginPopup("##atalhos")) {
            ImGui::Text("F5 / Shift+F5  Play / Stop");
            ImGui::Text("W / E / R       Gizmo mover/rotacionar/escalar");
            ImGui::Text("1 / 3 / 7       Câmera: frente / direita / topo (Blender)");
            ImGui::Text("Alt + arrastar  Câmera órbita no pivô (Blender)");
            ImGui::Text("Ctrl+Z / Ctrl+Y  Desfazer / Refazer");
            ImGui::Text("Ctrl+D / Del     Duplicar / Excluir");
            ImGui::Text("F11              Fullscreen do viewport");
            ImGui::Text("Botão direito    Navegação da câmera do editor");
            ImGui::EndPopup();
        }
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
    ImGui::DockSpace(dockspaceID, dockSize, (ImGuiDockNodeFlags)((int)ImGuiDockNodeFlags_AutoHideTabBar | (int)ImGuiDockNodeFlags_NoWindowMenuButton));
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
        // Coluna direita: Inspetor em cima, Profiler + Material embaixo.
        ImGuiID dockRightBottomID = ImGui::DockBuilderSplitNode(dockRightID, ImGuiDir_Down, 0.35f, nullptr, &dockRightID);
        // Área central: Viewport em cima, Console embaixo; Game View vira uma
        // ABA ao lado do Viewport (não flutuando na tela).
        ImGuiID dockCenterTopID = dockMainID;

        ImGui::DockBuilderDockWindow("Hierarquia", dockLeftID);
        ImGui::DockBuilderDockWindow("Content Browser", dockLeftBottomID);
        ImGui::DockBuilderDockWindow("Viewport", dockCenterTopID);
        ImGui::DockBuilderDockWindow("Game View", dockCenterTopID);   // aba ao lado do viewport
        ImGui::DockBuilderDockWindow("Console", dockCenterBottomID);
        ImGui::DockBuilderDockWindow("Inspetor", dockRightID);
        ImGui::DockBuilderDockWindow("Profiler", dockRightBottomID);
        // Material Editor e Project Settings NÃO são dockados de propósito —
        // abrem como janelas flutuantes (solto na tela), cada um no seu lugar.

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

                // Content browser, modo do viewport, recentes e telinha de
                // carregamento — tudo em um lugar só.
                OnProjectOpened(project);
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
                // Content browser, modo do viewport, recentes e telinha de
                // carregamento — tudo em um lugar só.
                OnProjectOpened(project);
            }
            ImGui::CloseCurrentPopup();
        } else if (cancel) {
            ImGui::CloseCurrentPopup();
        }
        ImGui::EndPopup();
    }}

void EditorLayer::LoadRecentProjects() {
    KZ_TRACE_SCOPE("EditorLayer::LoadRecentProjects");
    m_RecentProjects.clear();
    std::ifstream in(std::filesystem::current_path() / "KizuriRecents.json");
    if (!in.is_open()) return;
    try {
        nlohmann::json root = nlohmann::json::parse(in);
        for (auto& item : root["recentProjects"]) {
            RecentProject rp;
            rp.Name = item.value("name", "");
            rp.Path = item.value("path", "");
            rp.Mode = item.value("mode", "");
            if (!rp.Path.empty()) m_RecentProjects.push_back(rp);
        }
    } catch (...) {
        KZ_CORE_WARN("KizuriRecents.json corrompido ou inválido — ignorado.");
        m_RecentProjects.clear();
    }
}

void EditorLayer::SaveRecentProjects() {
    KZ_TRACE_SCOPE("EditorLayer::SaveRecentProjects");
    nlohmann::json root;
    for (auto& rp : m_RecentProjects) {
        root["recentProjects"].push_back({ { "name", rp.Name }, { "path", rp.Path }, { "mode", rp.Mode } });
    }
    std::ofstream out(std::filesystem::current_path() / "KizuriRecents.json");
    if (out.is_open()) out << root.dump(4);
}

void EditorLayer::RememberProject(const kizuri::Ref<kizuri::Project>& project) {
    if (!project) return;
    std::string path = project->GetFilePath();
    m_RecentProjects.erase(std::remove_if(m_RecentProjects.begin(), m_RecentProjects.end(),
        [&](const RecentProject& rp) { return rp.Path == path; }), m_RecentProjects.end());

    RecentProject rp;
    rp.Name = project->GetConfig().Name;
    rp.Path = path;
    switch (project->GetConfig().DefaultMode) {
        case ProjectMode::TwoD:   rp.Mode = "2D"; break;
        case ProjectMode::ThreeD: rp.Mode = "3D"; break;
        default:                  rp.Mode = "Vazio"; break;
    }
    m_RecentProjects.insert(m_RecentProjects.begin(), rp);
    if (m_RecentProjects.size() > 8) m_RecentProjects.resize(8);
    SaveRecentProjects();
}

void EditorLayer::OnProjectOpened(const kizuri::Ref<kizuri::Project>& project) {
    if (!project) return;
    m_ContentBrowserRoot = project->GetAssetDirectory();
    m_ContentBrowserCurrentDir = m_ContentBrowserRoot;

    ProjectMode mode = project->GetConfig().DefaultMode;
    if (mode == ProjectMode::TwoD) m_ViewportMode = ViewportMode::Mode2D;
    else if (mode == ProjectMode::ThreeD) m_ViewportMode = ViewportMode::Mode3D;

    // Build settings do projeto (nome/versão/resolução do export).
    auto& cfg = project->GetConfig();
    if (!cfg.GameName.empty())
        strncpy(m_ExportGameName, cfg.GameName.c_str(), sizeof(m_ExportGameName) - 1);
    if (!cfg.Version.empty())
        strncpy(m_ExportVersion, cfg.Version.c_str(), sizeof(m_ExportVersion) - 1);
    m_ExportWidth = cfg.WindowWidth > 0 ? cfg.WindowWidth : 1280;
    m_ExportHeight = cfg.WindowHeight > 0 ? cfg.WindowHeight : 720;

    // Recria a cena com o conteúdo padrão do MODO do projeto (2D = câmera
    // ortográfica + sprites, 3D = perspectiva + cubo) — ou carrega a cena
    // inicial configurada, se houver.
    m_SelectedEntity = {};
    m_History.Clear();

    // Cena inicial do projeto: carrega de forma ASSÍNCRONA (projeto grande
    // não pode travar o editor). Enquanto carrega, mostra o conteúdo padrão
    // do modo — e a cena real substitui quando terminar.
    m_ActiveScene = CreateRef<Scene>("Nova Cena");
    m_ScenePath.clear();
    CreateDefaultSceneContent();

    std::string startScene = project->GetConfig().StartScenePath;
    if (!startScene.empty()) {
        std::string resolved = Project::ResolvePath(startScene);
        m_ScenePath = resolved;
        OpenScene(resolved); // assíncrono — a cena nova substitui m_ActiveScene quando pronta
    }

    RememberProject(project);

    // Unity-style: sem carregar DLL manualmente. Se o projeto já tem um
    // assembly COMPILADO (Source/bin), carrega na hora — os scripts aparecem
    // no dropdown "Script Nativo" já no modo edição. Projeto novo (ainda sem
    // build) fica vazio até o primeiro Play, que compila e carrega sozinho.
    // IMPORTANTE: aqui só se CARREGA a dll existente (rápido). Compilar
    // (dotnet build) de forma síncrona neste ponto travava o editor ao abrir
    // o projeto — o build é responsabilidade do fluxo do Play.
    std::string csproj, engineRoot;
    GetGameBuildInfo(csproj, engineRoot);
    if (!csproj.empty()) {
        std::string dllPath;
        if (GameExporter::FindGameModuleDll(csproj, dllPath))
            ScriptEngine::LoadModule(dllPath);
        else
            KZ_CORE_INFO("Projeto sem assembly compilado ainda (o Play vai compilar).");
    }

    // Entra com a telinha de carregamento (transição Hub -> Editor).
    m_LoadingProjectName = project->GetConfig().Name;
    m_LoadingElapsed = 0.0f;
    m_EditorState = EditorState::Loading;
}

void EditorLayer::OpenRecentProject(const std::string& path) {
    KZ_TRACE_SCOPE("EditorLayer::OpenRecentProject");
    Ref<Project> project = Project::Load(path);
    if (project) OnProjectOpened(project);
}

void EditorLayer::DrawHub() {
    KZ_TRACE_SCOPE("EditorLayer::DrawHub");
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 display = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.065f, 1.0f));
    ImGui::Begin("##KizuriHub", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* titleFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Titlebar);
    ImFont* boldFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Bold);
    ImU32 accent = IM_COL32(217, 64, 77, 255);
    ImU32 textBright = IM_COL32(229, 229, 234, 255);
    ImU32 textDim = IM_COL32(130, 130, 140, 255);

    // ---- Coluna esquerda: marca + ações ----
    const float leftCol = 420.0f;
    dl->AddLine(ImVec2(leftCol, 0.0f), ImVec2(leftCol, display.y), IM_COL32(45, 45, 52, 255), 1.0f);

    float markSize = 76.0f;
    kizuri::editor::icons::Torii(dl, ImVec2(52.0f, 56.0f), markSize, accent);
    dl->AddText(titleFont, 46.0f, ImVec2(52.0f, 150.0f), textBright, "KIZURI");
    dl->AddText(ImGui::GetFont(), 16.0f, ImVec2(54.0f, 204.0f), textDim, "Editor de jogos 2D e 3D");
    dl->AddText(ImGui::GetFont(), 13.0f, ImVec2(54.0f, 228.0f), IM_COL32(95, 95, 105, 255), KIZURI_VERSION);

    ImGui::SetCursorPos(ImVec2(52.0f, 280.0f));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(14.0f, 9.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.82f, 0.24f, 0.27f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.93f, 0.30f, 0.33f, 1.0f));
    if (ImGui::Button("Novo Projeto", ImVec2(316.0f, 0.0f))) {
        strncpy(m_NewProjectDirBuffer, "MeuJogo", sizeof(m_NewProjectDirBuffer));
        strncpy(m_NewProjectNameBuffer, "MeuJogo", sizeof(m_NewProjectNameBuffer));
        m_RequestOpenNewProjectPopup = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(52.0f, 336.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.14f, 0.14f, 0.17f, 1.0f));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.24f, 1.0f));
    if (ImGui::Button("Abrir Projeto", ImVec2(316.0f, 0.0f))) {
        m_RequestOpenLoadProjectPopup = true;
    }
    ImGui::PopStyleColor(2);

    ImGui::SetCursorPos(ImVec2(52.0f, 392.0f));
    if (ImGui::Button("Continuar sem projeto", ImVec2(316.0f, 0.0f))) {
        m_EditorState = EditorState::Editor;
    }
    ImGui::PopStyleVar();

    dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(54.0f, display.y - 40.0f), IM_COL32(85, 85, 95, 255), "© 2026 Kizuri Engine");

    // ---- Coluna direita: projetos recentes ----
    float listX = leftCol + 44.0f;
    float listW = display.x - listX - 44.0f;
    ImGui::SetCursorPos(ImVec2(listX, 48.0f));
    kizuri::editor::icons::PanelHeader("PROJETOS RECENTES", kizuri::editor::icons::Folder);

    if (m_RecentProjects.empty()) {
        ImGui::SetCursorPos(ImVec2(listX, 110.0f));
        ImGui::TextDisabled("Nenhum projeto recente ainda. Crie ou abra um projeto para começar.");
    }

    float itemY = 112.0f;
    for (const auto& rp : m_RecentProjects) {
        ImGui::SetCursorPos(ImVec2(listX, itemY));
        ImGui::PushID(rp.Path.c_str());
        ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.09f, 0.09f, 0.11f, 1.0f));
        ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.15f, 0.15f, 0.18f, 1.0f));
        bool clicked = ImGui::Button("##recent", ImVec2(listW, 58.0f));
        ImGui::PopStyleColor(2);

        ImVec2 min = ImGui::GetItemRectMin();
        dl->AddText(boldFont, boldFont->FontSize, ImVec2(min.x + 16.0f, min.y + 8.0f), textBright, rp.Name.c_str());
        dl->AddText(ImGui::GetFont(), 12.0f, ImVec2(min.x + 16.0f, min.y + 34.0f), textDim, rp.Path.c_str());
        dl->AddText(ImGui::GetFont(), 13.0f, ImVec2(min.x + listW - 48.0f, min.y + 20.0f), accent, rp.Mode.c_str());

        if (clicked) OpenRecentProject(rp.Path);
        ImGui::PopID();
        itemY += 66.0f;
        if (itemY > display.y - 60.0f) break;
    }

    // Botão fechar (o hub não tem titlebar nativa)
    ImGui::SetCursorPos(ImVec2(display.x - 52.0f, 16.0f));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.82f, 0.24f, 0.27f, 0.9f));
    if (ImGui::Button("X", ImVec2(36.0f, 30.0f))) Application::Get().Close();
    ImGui::PopStyleColor(2);

    ImGui::End();
}

void EditorLayer::DrawLoadingScreen() {
    KZ_TRACE_SCOPE("EditorLayer::DrawLoadingScreen");
    ImGuiIO& io = ImGui::GetIO();
    ImVec2 display = io.DisplaySize;

    ImGui::SetNextWindowPos(ImVec2(0.0f, 0.0f));
    ImGui::SetNextWindowSize(display);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
    ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.055f, 0.055f, 0.065f, 1.0f));
    ImGui::Begin("##KizuriLoading", nullptr,
        ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_NoNav | ImGuiWindowFlags_NoSavedSettings);
    ImGui::PopStyleVar();
    ImGui::PopStyleColor();

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImFont* titleFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Titlebar);
    ImFont* boldFont = Application::Get().GetImGuiLayer()->GetFont(KizuriFont::Bold);
    ImU32 accent = IM_COL32(217, 64, 77, 255);
    ImVec2 center(display.x * 0.5f, display.y * 0.5f);

    float markSize = 84.0f;
    kizuri::editor::icons::Torii(dl, ImVec2(center.x - markSize * 0.5f, center.y - 190.0f), markSize, accent);

    const char* title = "Carregando projeto";
    ImVec2 ts = titleFont->CalcTextSizeA(34.0f, FLT_MAX, 0.0f, title);
    dl->AddText(titleFont, 34.0f, ImVec2(center.x - ts.x * 0.5f, center.y - 70.0f), IM_COL32(229, 229, 234, 255), title);
    ImVec2 ns = boldFont->CalcTextSizeA(boldFont->FontSize, FLT_MAX, 0.0f, m_LoadingProjectName.c_str());
    dl->AddText(boldFont, boldFont->FontSize, ImVec2(center.x - ns.x * 0.5f, center.y - 22.0f), IM_COL32(150, 150, 160, 255), m_LoadingProjectName.c_str());

    // Barra de progresso: se há um carregamento assíncrono em andamento,
    // mostra o progresso REAL da cena; senão, o tempo mínimo da tela.
    float t = m_LoadingElapsed / kHubLoadingMinSeconds;
    if (m_SceneLoading) t = m_PendingLoadProgress;
    if (t > 1.0f) t = 1.0f;
    float barW = 320.0f, barH = 6.0f;
    ImVec2 barMin(center.x - barW * 0.5f, center.y + 24.0f);
    dl->AddRectFilled(barMin, ImVec2(barMin.x + barW, barMin.y + barH), IM_COL32(45, 45, 52, 255), 3.0f);
    dl->AddRectFilled(barMin, ImVec2(barMin.x + barW * t, barMin.y + barH), accent, 3.0f);

    // Spinner girando (arco) ao lado da barra
    float angle = m_LoadingElapsed * 3.0f;
    dl->PathArcTo(ImVec2(center.x + barW * 0.5f + 30.0f, barMin.y + barH * 0.5f), 10.0f, angle, angle + 4.4f, 20);
    dl->PathStroke(accent, false, 3.0f);

    ImGui::End();
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
            "Fluxo avançado (fallback): carregue manualmente um assembly (.dll) C# "
            "compilado, fora do projeto. No fluxo normal isso é automático — abrir um "
            "projeto carrega os scripts e o Play compila e recarrega sozinho.");
        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-84.0f);
        bool enterPressed = ImGui::InputText("##game_module_path", m_GameModulePathBuffer, sizeof(m_GameModulePathBuffer), ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##game_module_browse")) {
            std::string path = FileDialog::OpenFile("Assembly do Jogo", "*.dll;*.so");
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

        // --- Compilar Scripts ---
        // O jogo é um assembly C# (Kizuri.Scripting + o projeto do jogo na
        // pasta Source/). Com a checkbox abaixo, o Play compila o assembly
        // automaticamente antes de rodar (estilo Unity) e recarrega; sem ela,
        // o Play usa o que estiver carregado acima.
        ImGui::Checkbox("Compilar C# automaticamente no Play", &m_AutoCompileOnPlay);
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

void EditorLayer::GetGameBuildInfo(std::string& outCsproj, std::string& outEngineRoot) {
    namespace fs = std::filesystem;
    outCsproj.clear();
    outEngineRoot.clear();

    auto& project = Project::GetActive();
    if (project) {
        fs::path sourceDir = fs::path(project->GetProjectDirectory()) / "Source";
        std::error_code ec;
        if (fs::is_directory(sourceDir, ec)) {
            for (auto& entry : fs::directory_iterator(sourceDir, ec)) {
                if (entry.path().extension() == ".csproj") {
                    outCsproj = entry.path().string();
                    break;
                }
            }
        }
    }

    // Raiz da engine: sobe da pasta bin/ do editor até achar o checkout
    // (marcado por managed/Kizuri.Scripting/Kizuri.Scripting.csproj).
    std::string binDir = std::filesystem::current_path().string();
    const auto& args = GetCommandLineArgs();
    if (!args.empty()) {
        std::filesystem::path exePath = args[0];
        if (exePath.has_parent_path())
            binDir = std::filesystem::absolute(exePath.parent_path()).string();
    }
    fs::path dir = binDir;
    for (int i = 0; i < 8 && !dir.empty(); ++i) {
        fs::path marker = dir / "managed" / "Kizuri.Scripting" / "Kizuri.Scripting.csproj";
        std::error_code ec;
        if (fs::is_regular_file(marker, ec)) { outEngineRoot = dir.string(); break; }
        dir = dir.parent_path();
    }
}

void EditorLayer::ExportGame(const std::string& outputDir) {
    GameExportRequest req;
    req.OutputDirectory = outputDir;
    req.ScenePath = m_ScenePath;
    req.GameModulePath = ScriptEngine::IsModuleLoaded() ? ScriptEngine::GetLoadedPath() : std::string{};

    // Pasta do executável do editor (= pasta bin/ do build).
    req.EngineBinDirectory = std::filesystem::current_path().string();
    const auto& args = GetCommandLineArgs();
    if (!args.empty()) {
        std::filesystem::path exePath = args[0];
        if (exePath.has_parent_path())
            req.EngineBinDirectory = std::filesystem::absolute(exePath.parent_path()).string();
    }

    // Export self-contained: usa o csproj do jogo (Source/ do projeto ativo)
    // e publica com o runtime .NET embutido.
    if (m_ExportSelfContained) {
        GetGameBuildInfo(req.GameProjectPath, req.EngineRoot);
        if (req.GameProjectPath.empty())
            KZ_CORE_WARN("Export self-contained: nenhum .csproj em <Projeto>/Source/. "
                         "Caindo pra cópia do assembly compilado (o jogador vai precisar do .NET).");
    }

    // Build settings do projeto (nome/versão/resolução).
    req.GameName = m_ExportGameName;
    req.Version = m_ExportVersion;
    req.WindowWidth = m_ExportWidth;
    req.WindowHeight = m_ExportHeight;

    // Persiste no .kzproj ativo (se houver).
    if (Project::GetActive()) {
        auto& cfg = Project::GetActive()->GetConfig();
        cfg.GameName = m_ExportGameName;
        cfg.Version = m_ExportVersion;
        cfg.WindowWidth = m_ExportWidth;
        cfg.WindowHeight = m_ExportHeight;
        Project::GetActive()->Save();
    }

    std::string err;
    if (GameExporter::Export(req, err))
        KZ_CORE_INFO("Exportação concluída em: {0}", outputDir);
    else
        KZ_CORE_ERROR("Exportação falhou: {0}", err);
}

void EditorLayer::DrawExportModal() {
    if (m_RequestOpenExportPopup) {
        m_RequestOpenExportPopup = false;
        if (Project::GetActive()) {
            auto def = (std::filesystem::path(Project::GetActive()->GetProjectDirectory()) / "Export").string();
            strncpy(m_ExportDirBuffer, def.c_str(), sizeof(m_ExportDirBuffer) - 1);
            m_ExportDirBuffer[sizeof(m_ExportDirBuffer) - 1] = '\0';
        }
        ImGui::OpenPopup("Exportar Jogo");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Exportar Jogo", nullptr, ImGuiWindowFlags_NoResize)) {
        kizuri::editor::icons::PanelHeader("EXPORTAR JOGO", kizuri::editor::icons::Folder);
        ImGui::TextWrapped(
            "Copia KizuriGame, a engine, a cena atual (como Start.kzscene), assets "
            "referenciados e o GameModule carregado (se houver) para uma pasta pronta pra distribuir.");
        ImGui::Spacing();

        ImGui::SetNextItemWidth(-84.0f);
        ImGui::InputText("##export_dir", m_ExportDirBuffer, sizeof(m_ExportDirBuffer));
        ImGui::SameLine();
        if (ImGui::Button("Procurar...##export_browse")) {
            std::string folder = FileDialog::SelectFolder();
            if (!folder.empty()) {
                strncpy(m_ExportDirBuffer, folder.c_str(), sizeof(m_ExportDirBuffer) - 1);
                m_ExportDirBuffer[sizeof(m_ExportDirBuffer) - 1] = '\0';
            }
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Plataforma de destino:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::Combo("##export_platform", &m_ExportPlatform,
                     "Windows (.exe)\0Linux (binários)\0Android (.apk — a engine compila)\0");
        if (m_ExportPlatform == 2) {
            if (!m_AndroidToolsChecked) {
                m_AndroidToolsChecked = true;
                m_AndroidTools = kizuri::AndroidExporter::DetectTools();
                m_AndroidMissing = m_AndroidTools.Missing;
            }
            if (m_AndroidTools.Ok) {
                ImGui::TextDisabled("Android SDK/NDK + dotnet localizados. A engine compila o APK inteiro aqui.");
            } else {
                ImGui::TextWrapped(
                    "A engine compila o Android sozinha, mas faltam ferramentas no seu PC "
                    "(configuráveis via Android Studio / sdkmanager — igual Unity pede SDK+JDK):");
                ImGui::TextColored(ImVec4(1.0f, 0.5f, 0.5f, 1.0f), "%s", m_AndroidMissing.c_str());
                if (ImGui::Button("Re-detectar ferramentas")) m_AndroidToolsChecked = false;
            }
            ImGui::TextWrapped(
                "O export copia cena+assets+scripts do projeto; a engine roda cmake/NDK, "
                "dotnet publish (CoreCLR android-arm64) e monta o APK assinado.");
        }

        ImGui::Spacing();
        ImGui::TextDisabled("Build settings (janela do jogo):");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("Nome do jogo", m_ExportGameName, sizeof(m_ExportGameName));
        ImGui::SetNextItemWidth(120.0f);
        ImGui::InputText("Versão", m_ExportVersion, sizeof(m_ExportVersion));
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputInt("Largura", &m_ExportWidth);
        ImGui::SetNextItemWidth(110.0f);
        ImGui::InputInt("Altura", &m_ExportHeight);
        m_ExportWidth = glm::max(320, m_ExportWidth);
        m_ExportHeight = glm::max(240, m_ExportHeight);

        ImGui::Spacing();
        ImGui::TextDisabled("Cena: %s", m_ScenePath.c_str());
        if (ScriptEngine::IsModuleLoaded())
            ImGui::TextDisabled("GameModule: %s", ScriptEngine::GetLoadedPath().c_str());
        else
            ImGui::TextDisabled("GameModule: (nenhum carregado)");

        ImGui::Spacing();
        ImGui::Checkbox("Embutir runtime .NET (self-contained)", &m_ExportSelfContained);
        if (m_ExportSelfContained) {
            std::string publishTarget;
            auto& project = Project::GetActive();
            if (project) {
                std::filesystem::path sourceDir =
                    std::filesystem::path(project->GetProjectDirectory()) / "Source";
                std::error_code ec;
                for (auto& entry : std::filesystem::directory_iterator(sourceDir, ec)) {
                    if (entry.path().extension() == ".csproj") {
                        publishTarget = entry.path().filename().string();
                        break;
                    }
                }
            }
            ImGui::Indent();
            if (!publishTarget.empty())
                ImGui::TextDisabled("Publica de: <Projeto>/Source/%s (o jogador não precisa instalar .NET)",
                                    publishTarget.c_str());
            else
                ImGui::TextDisabled("Sem .csproj em <Projeto>/Source/ — vai copiar o assembly compilado.");
            ImGui::Unindent();
        }

        ImGui::Spacing();
        ImGui::Separator();
        ImGui::Spacing();

        bool dirOk = m_ExportDirBuffer[0] != '\0';
        if (m_ExportPlatform != 2) {
            if (ImGui::Button("Exportar", ImVec2(120.0f, 0.0f)) && dirOk) {
                ExportGame(m_ExportDirBuffer);
                ImGui::CloseCurrentPopup();
            }
        } else {
            if (ImGui::Button("Exportar (Android)", ImVec2(140.0f, 0.0f)) && dirOk &&
                m_AndroidTools.Ok && !m_AndroidRunning) {
                StartAndroidExport();
                ImGui::CloseCurrentPopup();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
}

void EditorLayer::DrawSavePrefabModal() {
    if (m_RequestOpenSavePrefabPopup) {
        m_RequestOpenSavePrefabPopup = false;
        ImGui::OpenPopup("Salvar Prefab");
    }

    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(480.0f, 0.0f), ImGuiCond_Appearing);

    if (ImGui::BeginPopupModal("Salvar Prefab", nullptr, ImGuiWindowFlags_NoResize)) {
        ImGui::TextWrapped("Caminho do arquivo .kzprefab:");
        ImGui::SetNextItemWidth(-1.0f);
        ImGui::InputText("##prefab_path", m_PrefabPathBuffer, sizeof(m_PrefabPathBuffer));

        if (ImGui::Button("Salvar", ImVec2(100.0f, 0.0f)) && m_PrefabPathBuffer[0] != '\0') {
            Entity e = m_ActiveScene->GetEntityByUUID(m_PrefabEntityUUID);
            if (e) Prefab::CreateFromEntity(e, m_PrefabPathBuffer);
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(100.0f, 0.0f)))
            ImGui::CloseCurrentPopup();
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

void EditorLayer::RevealFileInContentBrowser(const std::string& filePath) {
    m_ContentBrowserRevealPath = filePath;
    m_ContentBrowserRevealRequested = true;
}


namespace {

// Cria um arquivo de script C# novo na pasta (template da API Kizuri.Scripting).
// A engine REGISTRA a classe automaticamente no Play (Host escaneia o
// assembly do jogo) — não precisa mexer em nenhum registro manual.
void CreateNewCSharpScript(const std::filesystem::path& dir, int templateKind) {
    namespace fs = std::filesystem;
    std::error_code ec;
    if (!fs::is_directory(dir, ec)) return;

    // Nome base por template (v0.37.0: scripts prontos pra aprender e usar).
    const char* baseName = "NovoScript";
    if (templateKind == 1) baseName = "PlayerController";
    else if (templateKind == 2) baseName = "Movement2D";
    else if (templateKind == 3) baseName = "Coletavel";

    fs::path file = dir / (std::string(baseName) + ".cs");
    int n = 1;
    while (fs::exists(file, ec)) file = dir / (std::string(baseName) + std::to_string(++n) + ".cs");

    // Template escolhido pelo usuário no momento de criar (código pronto,
    // comentado em português, todo método opcional na v0.37.0).
    std::string view = R"CS(using Kizuri;
using Kizuri.Math;
)CS";
    if (templateKind == 1) view += R"CS(
// Player 3D: WASD move o personagem na direção da câmera/do mundo
// e espaço pula (Character Controller). Anexe a um cubo/capsule.
public sealed class PlayerController : Script
{
	public float velocidade = 6f;
	public float forcaPulo = 8f;

	public override void OnCreate()
	{
		Entity.AddCharacterController(velocidade, -20f);
	}

	public override void OnUpdate(float deltaSeconds)
	{
		float x = 0f, z = 0f;
		if (Input.IsKeyDown(Key.W) || Input.IsKeyDown(Key.Up))    z += 1f;
		if (Input.IsKeyDown(Key.S) || Input.IsKeyDown(Key.Down))  z -= 1f;
		if (Input.IsKeyDown(Key.A) || Input.IsKeyDown(Key.Left))  x -= 1f;
		if (Input.IsKeyDown(Key.D) || Input.IsKeyDown(Key.Right)) x += 1f;

		Entity.Translate(new Vector3(x * velocidade * deltaSeconds, 0f, z * velocidade * deltaSeconds));

		if (Input.IsKeyDown(Key.Space))
			Entity.ApplyImpulse(new Vector3(0f, forcaPulo, 0f));
	}
}
)CS";
    else if (templateKind == 2) view += R"CS(
// Movimento 2D: setas movem um Rigidbody2D (adicione Rigidbody 2D e
// Box Collider 2D no Inspetor da entidade).
public sealed class Movement2D : Script
{
	public float velocidade = 5f;

	public override void OnUpdate(float deltaSeconds)
	{
		float x = 0f;
		if (Input.IsKeyDown(Key.A) || Input.IsKeyDown(Key.Left))  x -= 1f;
		if (Input.IsKeyDown(Key.D) || Input.IsKeyDown(Key.Right)) x += 1f;

		var rb = Entity.Rigidbody2D;
		rb.SetLinearVelocity(new Vector2(x * velocidade, rb.GetLinearVelocity().Y));
	}
}
)CS";
    else if (templateKind == 3) view += R"CS(
// Coletável: some ao encostar e toca um som — exemplo de colisão/trigger.
public sealed class Coletavel : Script
{
	public float pontos = 1f;

	public override void OnCollisionBegin(Entity other)
	{
		Log.Info($"Coletado por '{other.Name}'! (+{pontos})");
		Entity.AddAudio("assets/som/coleta.wav", false, true);
		Entity.Destroy();
	}
}
)CS";
    else view += R"CS(
// Script vazio — coloque a lógica aqui. O nome da CLASSE é o que aparece
// no Inspetor (componente "Script C#") e a engine registra sozinha.
public sealed class NovoScript : Script
{
	public override void OnCreate()
	{
		Log.Info("NovoScript criado.");
	}

	public override void OnUpdate(float deltaSeconds)
	{
		// deltaSeconds = tempo do último frame em segundos.
	}
}
)CS";

    {
        std::ofstream out(file);
        out << view;
    }
    if (ec) {
        KZ_CORE_ERROR("Falha ao criar script: {0}", ec.message());
        return;
    }
    KZ_CORE_INFO("Script C# criado: {0}", file.string());
}

} // namespace

void EditorLayer::DrawContentBrowser() {
    KZ_TRACE_SCOPE("EditorLayer::DrawContentBrowser");
    BeginPanelNoMenuButton();

    // Pedido do Inspetor (botão "Gerenciador"): abre o painel se estiver
    // colapsado e navega pra pasta do arquivo revelado.
    if (m_ContentBrowserRevealRequested) {
        m_ContentBrowserRevealRequested = false;
        ImGui::SetNextWindowCollapsed(false, ImGuiCond_Always);
        if (!m_ContentBrowserRoot.empty() && !m_ContentBrowserRevealPath.empty()) {
            std::error_code ec;
            std::filesystem::path p = kizuri::Project::ResolvePath(m_ContentBrowserRevealPath);
            if (p.has_parent_path())
                p = p.parent_path();
            // Só navega se o arquivo estiver dentro da raiz de assets.
            auto rel = std::filesystem::relative(p, m_ContentBrowserRoot, ec);
            if (!ec && rel.string().find("..") == std::string::npos)
                m_ContentBrowserCurrentDir = p;
        }
    }

    ImGui::Begin("Content Browser");
    kizuri::editor::icons::PanelHeader("CONTENT BROWSER", kizuri::editor::icons::Folder);

    auto& project = Project::GetActive();
    if (!project) {
        ImGui::TextDisabled("Nenhum projeto aberto. Use Arquivo > Novo Projeto ou Abrir Projeto para começar.");
        ImGui::End();
        return;
    }

    if (m_ContentBrowserRoot.empty()) {
        ImGui::TextDisabled("Nenhum projeto aberto.");
        ImGui::End();
        return;
    }

    // Atalho de pasta: raiz do projeto, pasta de conteúdo (assets) e
    // Source/ (scripts) — o Content Browser abre na pasta de conteúdo por
    // padrão; com um clique você troca de pasta de trabalho.
    {
        auto proj = Project::GetActive(); // Ref<Project> (não-pointeiro)
        ImGui::PushID("cb_shortcuts");
        std::string label = "📁 " + (m_ContentBrowserCurrentDir == m_ContentBrowserRoot
            ? std::string("Conteúdo (assets)") : std::string("Pasta atual"));
        if (ImGui::Button(label.c_str())) ImGui::OpenPopup("cb_shortcuts_popup");
        if (ImGui::BeginPopup("cb_shortcuts_popup")) {
            bool clickRoot = false, clickAsset = false, clickSource = false;
            if (proj) {
                ImGui::MenuItem("Raiz do projeto", nullptr, &clickRoot);
                ImGui::MenuItem("Conteúdo (assets)", nullptr, &clickAsset);
                if (std::filesystem::is_directory(std::filesystem::path(proj->GetProjectDirectory()) / "Source", std::error_code{}))
                    ImGui::MenuItem("Source (scripts)", nullptr, &clickSource);
            }
            ImGui::MenuItem("Subir um nível", nullptr, &clickRoot);
            if (clickRoot && proj)
                m_ContentBrowserCurrentDir = std::filesystem::path(proj->GetProjectDirectory());
            if (clickAsset && proj)
                m_ContentBrowserCurrentDir = std::filesystem::path(proj->GetAssetDirectory());
            if (clickSource && proj)
                m_ContentBrowserCurrentDir = std::filesystem::path(proj->GetProjectDirectory()) / "Source";
            ImGui::EndPopup();
        }
        ImGui::SameLine();
        ImGui::TextDisabled("%s", m_ContentBrowserCurrentDir.string().c_str());
        ImGui::PopID();
        ImGui::Separator();
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
        if (ImGui::MenuItem("Criar Script C#...")) {
            m_ScriptTemplateDir = m_ContentBrowserCurrentDir;
            m_RequestScriptTemplate = true;
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

    // Orçamento de thumbnails novos por frame: 8 decodificações no máximo.
    // Pasta com milhares de imagens preenche as miniaturas aos poucos (alguns
    // frames) sem nunca congelar a janela.
    m_ThumbBudget = 8;

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
        if (!isDir) {
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            if (ext == ".kzscene")       iconColor = IM_COL32(217, 180, 100, 255);
            else if (ext == ".glb" || ext == ".gltf" || ext == ".obj") iconColor = IM_COL32(110, 210, 120, 255);
            else if (ext == ".hdr")      iconColor = IM_COL32(90, 170, 230, 255);
            else if (ext == ".kzprefab") iconColor = IM_COL32(100, 200, 190, 255);
            else if (ext == ".wav" || ext == ".mp3" || ext == ".ogg" || ext == ".flac") iconColor = IM_COL32(190, 130, 220, 255);
            else if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") iconColor = IM_COL32(230, 120, 120, 255);
            else if (ext == ".cs")     iconColor = IM_COL32(90, 170, 220, 255);
        }
        if (isDir) {
            kizuri::editor::icons::Folder(dl, ImVec2(cursor.x + thumbSize * 0.15f, cursor.y + thumbSize * 0.15f), thumbSize * 0.7f, iconColor);
        } else {
            std::string ext = entry.path().extension().string();
            for (auto& c : ext) c = (char)tolower((unsigned char)c);
            bool isImage = ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
                           ext == ".bmp" || ext == ".tga" || ext == ".hdr";
            if (isImage) {
                // Preview real da imagem (cache por caminho — só carrega na 1ª
                // vez, e limitado a um orçamento por frame pra pasta gigante
                // não travar o editor; o resto vira placeholder até o frame
                // seguinte).
                auto thumb = GetThumbnail(entry.path().string());
                if (thumb) {
                    ImGui::SetCursorScreenPos(cursor);
                    ImGui::Image((ImTextureID)(uint64_t)thumb->GetRendererID(),
                                 ImVec2(thumbSize, thumbSize), ImVec2(0, 1), ImVec2(1, 0));
                } else {
                    dl->AddRect(ImVec2(cursor.x + thumbSize * 0.2f, cursor.y + thumbSize * 0.1f),
                                ImVec2(cursor.x + thumbSize * 0.8f, cursor.y + thumbSize * 0.9f), iconColor, 2.0f, 0, 2.0f);
                }
            } else {
                dl->AddRect(ImVec2(cursor.x + thumbSize * 0.2f, cursor.y + thumbSize * 0.1f),
                            ImVec2(cursor.x + thumbSize * 0.8f, cursor.y + thumbSize * 0.9f), iconColor, 2.0f, 0, 2.0f);
            }
        }

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
            if (!isDir && ImGui::MenuItem("Renomear")) {
                m_RenameTarget = entry.path();
                strncpy(m_RenameBuffer, name.c_str(), sizeof(m_RenameBuffer) - 1);
                m_RenameBuffer[sizeof(m_RenameBuffer) - 1] = '\0';
                m_RequestRenamePopup = true;
            }
            if (ImGui::MenuItem("Excluir")) {
                std::error_code delEc;
                std::filesystem::remove_all(entry.path(), delEc);
            }
            ImGui::EndPopup();
        }

        ImGui::PopID();
    }

    ImGui::EndTable();

    // Modal de renomear (Arquivo > Content Browser).
    if (m_RequestRenamePopup) {
        m_RequestRenamePopup = false;
        ImGui::OpenPopup("Renomear");
    }
    if (ImGui::BeginPopupModal("Renomear", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Novo nome do arquivo:");
        ImGui::SetNextItemWidth(320.0f);
        ImGui::InputText("##rename", m_RenameBuffer, sizeof(m_RenameBuffer));
        bool ok = ImGui::IsKeyPressed(ImGuiKey_Enter) || ImGui::Button("OK", ImVec2(80.0f, 0.0f));
        if (ok) {
            std::string newName = m_RenameBuffer;
            if (!newName.empty() && !m_RenameTarget.empty()) {
                std::error_code re;
                std::filesystem::rename(m_RenameTarget, m_RenameTarget.parent_path() / newName, re);
                if (re) KZ_CORE_ERROR("Falha ao renomear: {0}", re.message());
            }
            ImGui::CloseCurrentPopup();
        }
        ImGui::SameLine();
        if (ImGui::Button("Cancelar", ImVec2(80.0f, 0.0f))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }

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

    // Busca por nome (filtra): mostra uma lista plana das entidades que casam.
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
    ImGui::SetNextItemWidth(-1.0f);
    ImGui::InputTextWithHint("##hierarchy_search", "Buscar entidade (nome)...",
                             m_HierarchySearchBuffer, sizeof(m_HierarchySearchBuffer));
    ImGui::PopStyleVar();

    if (m_HierarchySearchBuffer[0] != '\0') {
        std::string query = m_HierarchySearchBuffer;
        for (auto& c : query) c = (char)tolower((unsigned char)c);
        m_ActiveScene->GetRegistry().view<TagComponent>().each(
            [&](auto entityHandle, TagComponent& tag) {
                std::string name = tag.Tag;
                for (auto& c : name) c = (char)tolower((unsigned char)c);
                if (name.find(query) == std::string::npos) return;
                Entity entity{ entityHandle, m_ActiveScene.get() };
                if (ImGui::Selectable(tag.Tag.c_str(), m_SelectedEntity == entity))
                    m_SelectedEntity = entity;
            });
    } else {
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

bool EditorLayer::IsEntityMultiSelected(Entity entity) const {
    return m_MultiSelection.find(entity.GetUUID()) != m_MultiSelection.end();
}

std::vector<Entity> EditorLayer::GetMultiSelection() const {
    std::vector<Entity> out;
    if (!m_ActiveScene) return out;
    auto view = m_ActiveScene->GetRegistry().view<TagComponent>();
    for (auto e : view) {
        Entity entity{ e, m_ActiveScene.get() };
        if (m_MultiSelection.find(entity.GetUUID()) != m_MultiSelection.end())
            out.push_back(entity);
    }
    return out;
}

void EditorLayer::ClearMultiSelection() { m_MultiSelection.clear(); }

void EditorLayer::DrawEntityNode(Entity entity, Entity& outEntityToDelete, bool editable) {
    auto& tag = entity.GetComponent<TagComponent>().Tag;
    auto children = entity.GetChildren();

    ImGuiTreeNodeFlags flags = (IsEntityMultiSelected(entity) ? ImGuiTreeNodeFlags_Selected : 0)
        | ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
    bool isLeaf = children.empty();
    if (isLeaf) flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;

    bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());
    // Só o modo edição seleciona por clique — no Play a árvore é leitura.
    // Ctrl+clique alterna a multi-seleção.
    if (editable && ImGui::IsItemClicked()) {
        bool ctrl = ImGui::GetIO().KeyCtrl;
        if (ctrl) {
            auto it = m_MultiSelection.find(entity.GetUUID());
            if (it != m_MultiSelection.end()) m_MultiSelection.erase(it);
            else m_MultiSelection.insert(entity.GetUUID());
        } else {
            m_MultiSelection.clear();
        }
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
            if (ImGui::MenuItem("Salvar Prefab...")) {
                std::string defaultName = entity.GetName() + ".kzprefab";
                if (Project::GetActive())
                    defaultName = (std::filesystem::path(Project::GetActive()->GetAssetDirectory()) / defaultName).string();
                std::string path = FileDialog::SaveFile("Prefab Kizuri", "*.kzprefab", "kzprefab");
                if (path.empty()) {
                    strncpy(m_PrefabPathBuffer, defaultName.c_str(), sizeof(m_PrefabPathBuffer) - 1);
                    m_PrefabPathBuffer[sizeof(m_PrefabPathBuffer) - 1] = '\0';
                    m_PrefabEntityUUID = entity.GetUUID();
                    m_RequestOpenSavePrefabPopup = true;
                } else {
                    Prefab::CreateFromEntity(entity, path);
                }
            }
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
        auto multi = GetMultiSelection();
        if (multi.size() > 1) {
            // Multi-seleção: não edita componente individual — mostra resumo.
            ImGui::TextDisabled("%zu entidades selecionadas", multi.size());
            ImGui::SameLine();
            if (ImGui::SmallButton("Excluir seleção")) {
                for (auto& e : multi) {
                    m_History.Push(CreateRef<DeleteEntityCommand>(e));
                    m_ActiveScene->DestroyEntity(e);
                }
                m_SelectedEntity = {};
                ClearMultiSelection();
            }
            ImGui::Separator();
        } else {
            ClearMultiSelection();
        }

        auto& tagc = m_SelectedEntity.GetComponent<TagComponent>();
        auto& tag = tagc.Tag;
        char buffer[256];
        strncpy(buffer, tag.c_str(), sizeof(buffer) - 1);
        buffer[sizeof(buffer) - 1] = '\0';
        if (ImGui::InputText("Nome", buffer, sizeof(buffer))) tag = std::string(buffer);

        // Ativo/inativo (estilo GameObject.SetActive) — inativa não desenha
        // nem atualiza (ela e os filhos herdam).
        auto& idc = m_SelectedEntity.GetComponent<IDComponent>();
        ImGui::Checkbox("Ativo", &idc.Active);

        // Tags & Layers: camada de colisão + máscara (bits = camadas que colidem).
        ImGui::SetNextItemWidth(70.0f);
        ImGui::InputInt("Camada", &tagc.Layer);
        ImGui::SetNextItemWidth(150.0f);
        uint32_t mask = tagc.CollisionMask;
        ImGui::InputScalar("Colide com (máscara)", ImGuiDataType_U32, &mask, nullptr, nullptr, "%08X", ImGuiInputTextFlags_CharsHexadecimal);
        tagc.CollisionMask = mask;
        ImGui::TextDisabled("Bit N = camada N. Ex.: 0xFFFFFFFF = todas; 0x0003 = camadas 0 e 1.");

        if (m_SelectedEntity.HasComponent<TransformComponent>()) {
            auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
            if (ImGui::TreeNodeEx("Transform", ImGuiTreeNodeFlags_DefaultOpen)) {
                ImGui::DragFloat3("Posição", &tc.Translation.x, 0.1f);
                ImGui::DragFloat3("Rotação", &tc.Rotation.x, 0.1f);
                ImGui::DragFloat3("Escala", &tc.Scale.x, 0.1f);
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
                std::string browsedTexture;
                if (FileBrowseButton("Textura (sprite)", "*.png;*.jpg;*.jpeg;*.bmp;*.tga", browsedTexture)) {
                    sc.TexturePath = browsedTexture;
                    sc.Texture = sc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(sc.TexturePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(sc.TexturePath);
                if (sc.Texture) {
                    uint32_t texID = sc.Texture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
                    ImGui::SameLine();
                    ImGui::TextDisabled("%ux%u", sc.Texture->GetWidth(), sc.Texture->GetHeight());
                }
                ImGui::ColorEdit4("Cor", &sc.Color.x);
                ImGui::DragFloat("Tiling", &sc.TilingFactor, 0.1f);
                ImGui::Checkbox("Inverter X", &sc.FlipX);
                ImGui::SameLine();
                ImGui::Checkbox("Inverter Y", &sc.FlipY);
                ImGui::DragInt("Camada de ordenação", &sc.SortingLayer, 1);
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
                ImGui::DragInt("Camada de ordenação", &cr.SortingLayer, 1);
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

        // ---- Script C# (NativeScript) — o componente que roda código do
        // jogo. A engine registra as classes automaticamente quando o
        // jogo compila (Play) — aqui só escolhe qual classe roda.
        if (m_SelectedEntity.HasComponent<NativeScriptComponent>()) {
            auto& nsc = m_SelectedEntity.GetComponent<NativeScriptComponent>();
            if (DrawComponentHeader("Script C#", &removeThis)) {
                auto classNames = ScriptEngine::GetRegistry().GetClassNames();
                if (classNames.empty()) {
                    ImGui::TextDisabled("Nenhum script compilado ainda.");
                    ImGui::TextDisabled("Dê Play uma vez (compilação automática do projeto) e as classes aparecem aqui.");
                } else {
                    int current = 0;
                    std::vector<const char*> items;
                    items.reserve(classNames.size() + 1);
                    items.push_back("(nenhuma)");
                    for (auto& cn : classNames) items.push_back(cn.c_str());
                    for (size_t i = 1; i < items.size(); ++i)
                        if (nsc.ClassName == items[i]) { current = (int)i; break; }
                    int chosen = current;
                    if (ImGui::Combo("Classe", &chosen, items.data(), (int)items.size())) {
                        nsc.ClassName = (chosen == 0) ? std::string() : classNames[(size_t)chosen - 1];
                        nsc.DestroyInstance();
                    }
                    if (!nsc.ClassName.empty())
                        ImGui::TextDisabled("Roda '%s' quando der Play.", nsc.ClassName.c_str());
                }
                ImGui::Spacing();
                if (ImGui::Button("+ Criar script novo...")) {
                    std::string csproj, engineRoot;
                    GetGameBuildInfo(csproj, engineRoot);
                    std::filesystem::path dir = std::filesystem::path(csproj).parent_path();
                    std::error_code ec;
                    if (dir.empty() || !std::filesystem::is_directory(dir, ec))
                        dir = m_ContentBrowserCurrentDir;
                    m_ScriptTemplateDir = dir;
                    m_RequestScriptTemplate = true;
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<NativeScriptComponent>();
        }

        if (m_SelectedEntity.HasComponent<CameraFollowComponent>()) {
            auto& cf = m_SelectedEntity.GetComponent<CameraFollowComponent>();
            if (DrawComponentHeader("Camera Follow", &removeThis)) {
                char targetBuf[256];
                strncpy(targetBuf, cf.TargetName.c_str(), sizeof(targetBuf) - 1);
                targetBuf[sizeof(targetBuf) - 1] = '\0';
                if (ImGui::InputText("Alvo (nome)", targetBuf, sizeof(targetBuf)))
                    cf.TargetName = std::string(targetBuf);
                ImGui::DragFloat3("Offset", &cf.Offset.x, 0.05f);
                ImGui::DragFloat("Suavidade", &cf.Smoothness, 0.1f, 0.0f, 60.0f);
                ImGui::Checkbox("Gira com o alvo", &cf.FollowRotation);
                ImGui::Checkbox("Offset em espaço mundo", &cf.UseWorldOffset);
                ImGui::TextDisabled("A câmera segue a entidade com esse nome no Play.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<CameraFollowComponent>();
        }

        if (m_SelectedEntity.HasComponent<UICanvasComponent>()) {
            auto& uc = m_SelectedEntity.GetComponent<UICanvasComponent>();
            if (DrawComponentHeader("UI Canvas", &removeThis)) {
                ImGui::DragFloat("Meia-altura (Ortho)", &uc.OrthoSize, 0.1f, 1.0f, 100.0f);
                ImGui::TextDisabled("Renderiza os descendentes com UIRect em espaço de tela (0,0 = centro).");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<UICanvasComponent>();
        }

        if (m_SelectedEntity.HasComponent<UIRectComponent>()) {
            auto& ur = m_SelectedEntity.GetComponent<UIRectComponent>();
            if (DrawComponentHeader("UI Rect", &removeThis)) {
                ImGui::DragFloat2("Posição (centro)", &ur.Position.x, 0.05f);
                ImGui::DragFloat2("Tamanho", &ur.Size.x, 0.05f, 0.0f, 0.0f);
                ImGui::ColorEdit4("Cor de fundo", &ur.Color.x);
                ImGui::TextDisabled("Precisa ser descendente de uma entidade com UI Canvas.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<UIRectComponent>();
        }

        if (m_SelectedEntity.HasComponent<UIButtonComponent>()) {
            auto& ub = m_SelectedEntity.GetComponent<UIButtonComponent>();
            if (DrawComponentHeader("UI Botão", &removeThis)) {
                ImGui::TextDisabled("Hovered: %s | Pressed: %s | Foi clicado: %s",
                                    ub.Hovered ? "sim" : "não",
                                    ub.Pressed ? "sim" : "não",
                                    ub.WasClicked ? "sim" : "não");
                ImGui::TextDisabled("Estado em tempo real (só muda no Play). Cheque via C#: UIButtonWasClicked().");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<UIButtonComponent>();
        }

        if (m_SelectedEntity.HasComponent<MeshRendererComponent>()) {
            auto& mr = m_SelectedEntity.GetComponent<MeshRendererComponent>();
            if (DrawComponentHeader("Mesh Renderer", &removeThis)) {
                // Fonte da mesh: combobox com os builtins + campo livre pra arquivo.
                const char* builtins[] = { "builtin:cube", "builtin:plane", "builtin:sphere",
                                           "builtin:cylinder", "builtin:cone", "builtin:capsule", "builtin:torus" };
                int currentBuiltin = -1;
                for (int i = 0; i < 7; ++i) if (mr.MeshSource == builtins[i]) currentBuiltin = i;
                if (ImGui::Combo("Mesh pronta", &currentBuiltin, builtins, 7)) {
                    if (currentBuiltin >= 0) {
                        mr.MeshSource = builtins[currentBuiltin];
                        mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                    }
                }
                char meshBuf[512];
                strncpy(meshBuf, mr.MeshSource.c_str(), sizeof(meshBuf) - 1);
                meshBuf[sizeof(meshBuf) - 1] = '\0';
                if (ImGui::InputText("Malha (.obj/.glb/.gltf)", meshBuf, sizeof(meshBuf))) {
                    mr.MeshSource = meshBuf;
                    if (!mr.MeshSource.empty()) {
                        mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                        // Modelo glTF importado traz material PBR próprio — aplica.
                        if (mr.MeshSource.find(".glb") != std::string::npos || mr.MeshSource.find(".gltf") != std::string::npos)
                            mr.MeshMaterial = kizuri::Mesh::ExtractMaterialFromGLTF(Project::ResolvePath(mr.MeshSource));
                    }
                }
                std::string droppedMesh;
                if (AcceptAssetDrop(droppedMesh)) {
                    mr.MeshSource = kizuri::Project::MakeRelativePath(droppedMesh);
                    mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                    if (mr.MeshSource.find(".glb") != std::string::npos || mr.MeshSource.find(".gltf") != std::string::npos)
                        mr.MeshMaterial = kizuri::Mesh::ExtractMaterialFromGLTF(Project::ResolvePath(mr.MeshSource));
                }
                std::string browsedMesh;
                if (FileBrowseButton("Malha 3D", "*.glb;*.gltf;*.obj", browsedMesh)) {
                    mr.MeshSource = browsedMesh;
                    mr.MeshAsset = kizuri::Mesh::FromSource(mr.MeshSource);
                    if (mr.MeshSource.find(".glb") != std::string::npos || mr.MeshSource.find(".gltf") != std::string::npos)
                        mr.MeshMaterial = kizuri::Mesh::ExtractMaterialFromGLTF(Project::ResolvePath(mr.MeshSource));
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(mr.MeshSource);
                // Material NÃO é editado aqui — o painel "Material Editor"
                // (menu Exibir, janela flutuante) tem preview + todos os
                // campos PBR. O Inspetor cuida da malha; o material fica
                // num lugar só.
                if (ImGui::Button("Abrir Material Editor")) {
                    for (auto& p : m_Panels)
                        if (std::string(p->GetTitle()) == "Material Editor") { p->SetVisible(true); break; }
                }
                ImGui::SameLine();
                ImGui::TextDisabled("Preview + campos PBR na janela dedicada.");
                if (ImGui::Button("Assar Lightmap (AO + ambience)")) {
                    m_ActiveScene->BakeLightmap(m_SelectedEntity);
                    // Salva a lightmap junto do projeto (assets/).
                    if (mr.LightmapTexture) {
                        std::string path = kizuri::Project::GetActive()->GetAssetDirectory() + "/Lightmaps/" + m_SelectedEntity.GetName() + "_lightmap.png";
                        std::error_code ec;
                        std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
                        if (kizuri::Texture2D::SaveToFile(mr.LightmapTexture, path))
                            mr.LightmapPath = kizuri::Project::MakeRelativePath(path);
                    }
                }
                if (mr.LightmapTexture) {
                    ImGui::SameLine();
                    if (ImGui::Button("Remover lightmap")) { mr.LightmapTexture = nullptr; mr.LightmapPath.clear(); }
                    uint32_t texID = mr.LightmapTexture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(96.0f, 96.0f), ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<MeshRendererComponent>();
        }

        // LOD (Level of Detail): malhas por distância. Só tem efeito junto de
        // um MeshRenderer.
        if (m_SelectedEntity.HasComponent<LODComponent>()) {
            auto& lod = m_SelectedEntity.GetComponent<LODComponent>();
            if (DrawComponentHeader("LOD (níveis de detalhe)", &removeThis)) {
                ImGui::DragFloat("Multiplicador de distância", &lod.DistanceMultiplier, 0.01f, 0.1f, 10.0f);
                ImGui::TextDisabled("Distâncias crescentes; índice 0 = mais detalhe.");
                int toRemove = -1;
                for (int i = 0; i < (int)lod.Levels.size(); ++i) {
                    auto& l = lod.Levels[i];
                    char buf[512];
                    strncpy(buf, l.MeshSource.c_str(), sizeof(buf) - 1); buf[sizeof(buf) - 1] = '\0';
                    ImGui::PushID(i);
                    if (ImGui::InputText("Malha", buf, sizeof(buf))) {
                        l.MeshSource = buf;
                        l.MeshAsset = l.MeshSource.empty() ? nullptr : Mesh::FromSource(l.MeshSource);
                    }
                    ImGui::SameLine();
                    if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(l.MeshSource);
                    if (ImGui::DragFloat("A partir de (distância)", &l.Distance, 1.0f, 0.0f, 10000.0f))
                        std::sort(lod.Levels.begin(), lod.Levels.end(),
                                  [](auto& a, auto& b) { return a.Distance < b.Distance; });
                    ImGui::SameLine();
                    if (ImGui::SmallButton("X")) toRemove = i;
                    ImGui::PopID();
                    ImGui::Separator();
                }
                if (toRemove >= 0 && toRemove < (int)lod.Levels.size())
                    lod.Levels.erase(lod.Levels.begin() + toRemove);
                if (ImGui::Button("+ Adicionar nível"))
                    lod.Levels.push_back({ "builtin:cube", 100.0f, nullptr });
                ImGui::Separator();
                if (ImGui::Button("Gerar LOD automático (builtins)")) {
                    auto& mr = m_SelectedEntity.GetComponent<MeshRendererComponent>();
                    lod.Levels.clear();
                    // Nível 0 = a malha do MeshRenderer (maior detalhe).
                    lod.Levels.push_back({ mr.MeshSource, 30.0f, mr.MeshAsset });
                    for (int lvl = 1; lvl <= 2; ++lvl) {
                        auto m = kizuri::Mesh::CreateLODMesh(mr.MeshSource, lvl);
                        lod.Levels.push_back({ mr.MeshSource, 60.0f + (lvl - 1) * 80.0f, m });
                    }
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<LODComponent>();
        }

        if (m_SelectedEntity.HasComponent<TimelineComponent>()) {
            auto& tl = m_SelectedEntity.GetComponent<TimelineComponent>();
            if (DrawComponentHeader("Timeline (cutscene)", &removeThis)) {
                if (ImGui::Button(tl.Playing ? "Pausar" : "Tocar")) tl.Playing = !tl.Playing;
                ImGui::SameLine();
                ImGui::Checkbox("Loop", &tl.Loop);
                ImGui::SameLine();
                ImGui::SetNextItemWidth(90.0f);
                ImGui::DragFloat("Velocidade", &tl.Speed, 0.01f, 0.0f, 8.0f);
                float dur = tl.Duration();
                if (dur > 0.0f)
                    ImGui::SliderFloat("Tempo", &tl.Time, 0.0f, dur);
                // Timeline VISUAL (pilar AAA v0.34): linha do tempo com os
                // keyframes arrastáveis (Tempo editável) + posição/rotação.
                int toRemove = -1;
                if (!tl.Keyframes.empty()) {
                    float maxT = tl.Duration();
                    ImGui::TextUnformatted("Linha do tempo (arraste os tempos):");
                    for (int i = 0; i < (int)tl.Keyframes.size(); ++i) {
                        auto& k = tl.Keyframes[i];
                        ImGui::PushID(i);
                        ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);
                        if (ImGui::BeginChild(ImGui::GetID((void*)(uintptr_t)i), ImVec2(0, 0),
                                              ImGuiChildFlags_AlwaysUseWindowPadding, ImGuiWindowFlags_AlwaysVerticalScrollbar)) {
                        }
                        ImGui::PopStyleVar();
                        // faixa + marcador do keyframe
                        ImGui::BeginChild(ImGui::GetID((void*)(uintptr_t)(i + 1000)), ImVec2(0, 26));
                        ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.16f, 0.2f, 1.0f));
                        ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.82f, 0.24f, 0.27f, 1.0f));
                        float t = k.Time;
                        if (ImGui::SliderFloat("##t", &t, 0.0f, glm::max(maxT, 0.1f), "")) k.Time = t;
                        ImGui::PopStyleColor(2);
                        ImGui::EndChild();
                        ImGui::SameLine();
                        ImGui::SetNextItemWidth(70.0f);
                        ImGui::DragFloat("t", &k.Time, 0.05f, 0.0f, 999.0f, "%.2f");
                        ImGui::SameLine();
                        ImGui::DragFloat3("P", &k.Position.x, 0.05f);
                        ImGui::SameLine();
                        ImGui::DragFloat3("R", &k.Rotation.x, 1.0f);
                        ImGui::SameLine();
                        if (ImGui::SmallButton("X")) toRemove = i;
                        ImGui::PopID();
                    }
                    if (toRemove >= 0 && toRemove < (int)tl.Keyframes.size())
                        tl.Keyframes.erase(tl.Keyframes.begin() + toRemove);
                } else {
                    ImGui::TextDisabled("Sem keyframes — adicione abaixo (posição atual da entidade).");
                }
                if (toRemove >= 0 && toRemove < (int)tl.Keyframes.size())
                    tl.Keyframes.erase(tl.Keyframes.begin() + toRemove);
                if (ImGui::Button("+ Keyframe (posição atual)")) {
                    auto& tc = m_SelectedEntity.GetComponent<TransformComponent>();
                    TimelineComponent::Keyframe k;
                    k.Time = tl.Time + 1.0f;
                    k.Position = tc.Translation;
                    tl.Keyframes.push_back(k);
                    std::sort(tl.Keyframes.begin(), tl.Keyframes.end(),
                              [](auto& a, auto& b) { return a.Time < b.Time; });
                }
                ImGui::TextDisabled("Interpola posição/rotação/escala entre keyframes.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<TimelineComponent>();
        }

        if (m_SelectedEntity.HasComponent<CharacterControllerComponent>()) {
            auto& cc = m_SelectedEntity.GetComponent<CharacterControllerComponent>();
            if (DrawComponentHeader("Character Controller", &removeThis)) {
                ImGui::DragFloat("Velocidade", &cc.Speed, 0.1f, 0.0f, 50.0f);
                ImGui::DragFloat("Gravidade", &cc.Gravity, 0.5f, -80.0f, 0.0f);
                ImGui::DragFloat("Raio", &cc.Radius, 0.01f, 0.1f, 2.0f);
                ImGui::DragFloat("Altura", &cc.Height, 0.05f, 0.5f, 5.0f);
                ImGui::DragFloat("Passo", &cc.StepOffset, 0.01f, 0.0f, 1.0f);
                ImGui::TextDisabled("Movimento: script usa MoveCharacter(x, z). Chão por raycast.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<CharacterControllerComponent>();
        }

        if (m_SelectedEntity.HasComponent<TerrainComponent>()) {
            auto& t = m_SelectedEntity.GetComponent<TerrainComponent>();
            if (DrawComponentHeader("Terreno (heightmap)", &removeThis)) {
                bool changed = false;
                changed |= ImGui::SliderInt("Segmentos", (int*)&t.Segments, 16, 200);
                changed |= ImGui::DragFloat("Tamanho", &t.Size, 1.0f, 10.0f, 1000.0f);
                changed |= ImGui::DragFloat("Elevação", &t.HeightScale, 0.1f, 0.0f, 50.0f);
                changed |= ImGui::DragInt("Semente", (int*)&t.Seed, 1);
                if (changed || ImGui::Button("Regenerar terreno")) {
                    if (changed) t.Heightmap.clear(); // mudou a geometria base — descarta a escultura
                    t.Regenerate();
                    if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
                        m_SelectedEntity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
                }

                ImGui::Separator();
                ImGui::Checkbox("Modo escultura (pincel no viewport)", &m_TerrainSculpting);
                ImGui::DragFloat("Raio do pincel", &m_TerrainBrushRadius, 0.1f, 0.5f, 30.0f);
                ImGui::DragFloat("Intensidade", &m_TerrainBrushStrength, 0.05f, 0.01f, 5.0f);
                ImGui::TextDisabled("Esquerdo = levanta · Shift+esquerdo = afunda.");
                if (!t.Heightmap.empty() && ImGui::Button("Limpar escultura (volta ao fbm)")) {
                    t.Heightmap.clear();
                    t.Regenerate();
                    if (m_SelectedEntity.HasComponent<MeshRendererComponent>())
                        m_SelectedEntity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<TerrainComponent>();
        }

        if (m_SelectedEntity.HasComponent<AnimatorComponent>()) {
            auto& ac = m_SelectedEntity.GetComponent<AnimatorComponent>();
            if (DrawComponentHeader("Animador (skinning)", &removeThis)) {
                // Skin carregada sob demanda (path do .glb/.gltf).
                if (!ac.Skin && !ac.MeshPath.empty())
                    ac.Skin = kizuri::SkinData::CreateFromGLTF(Project::ResolvePath(ac.MeshPath));

                char animPath[512];
                strncpy(animPath, ac.MeshPath.c_str(), sizeof(animPath) - 1);
                animPath[sizeof(animPath) - 1] = '\0';
                if (ImGui::InputText("Malha animada (.glb/.gltf)", animPath, sizeof(animPath))) {
                    ac.MeshPath = animPath;
                    ac.Skin = ac.MeshPath.empty() ? nullptr : kizuri::SkinData::CreateFromGLTF(Project::ResolvePath(ac.MeshPath));
                    if (!ac.Skin || ac.Skin->Clips.empty()) ac.ClipName.clear();
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(ac.MeshPath);

                if (ac.Skin && !ac.Skin->Clips.empty()) {
                    // Seletor de clip.
                    int currentClip = ac.Skin->GetClipIndex(ac.ClipName);
                    const char* preview = (currentClip >= 0) ? ac.Skin->Clips[(size_t)currentClip].Name.c_str() : "(pose de repouso)";
                    if (ImGui::BeginCombo("Animação", preview)) {
                        for (int i = 0; i < (int)ac.Skin->Clips.size(); ++i) {
                            bool selected = (i == currentClip);
                            if (ImGui::Selectable(ac.Skin->Clips[(size_t)i].Name.c_str(), selected)) {
                                ac.Play(ac.Skin->Clips[(size_t)i].Name);
                                currentClip = i;
                            }
                        }
                        ImGui::EndCombo();
                    }

                    // Transporte de preview (roda em modo edição, no viewport).
                    ImGui::Checkbox("Tocando", &ac.Playing);
                    ImGui::SameLine();
                    ImGui::Checkbox("Em loop", &ac.Loop);
                    ImGui::DragFloat("Velocidade", &ac.Speed, 0.05f, 0.0f, 10.0f);
                    float dur = ac.Skin->GetClipDuration(ac.ClipName);
                    ImGui::DragFloat("Tempo", &ac.Time, 0.01f, 0.0f, dur > 0.0f ? dur : 1.0f);
                    ImGui::TextDisabled("Juntas: %d | Clipes: %d", (int)ac.Skin->Joints.size(), (int)ac.Skin->Clips.size());
                } else {
                    ImGui::TextDisabled("Aponte o caminho de um .glb/.gltf com skin + animações.");
                }
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<AnimatorComponent>();
        }

        if (m_SelectedEntity.HasComponent<AnimatorStateMachineComponent>()) {
            auto& sm = m_SelectedEntity.GetComponent<AnimatorStateMachineComponent>();
            bool removeSM = false;
            if (DrawComponentHeader("Máquina de Estados (animação)", &removeSM)) {
                if (sm.CurrentState >= 0 && sm.CurrentState < (int)sm.States.size())
                    ImGui::TextDisabled("Estado atual: %s (blend: %.0f%%)",
                                        sm.States[(size_t)sm.CurrentState].Name.c_str(),
                                        sm.m_TransitionDuration > 0.0f
                                            ? 100.0f * glm::clamp(sm.m_TransitionTime / sm.m_TransitionDuration, 0.0f, 1.0f)
                                            : 100.0f);
                else
                    ImGui::TextDisabled("Nenhum estado selecionado.");

                int removeIdx = -1;
                for (int i = 0; i < (int)sm.States.size(); ++i) {
                    auto& st = sm.States[(size_t)i];
                    std::string label = "Estado " + std::to_string(i + 1);
                    if (ImGui::TreeNodeEx(label.c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
                        char nameBuf[128], clipBuf[128];
                        strncpy(nameBuf, st.Name.c_str(), sizeof(nameBuf) - 1); nameBuf[sizeof(nameBuf) - 1] = '\0';
                        strncpy(clipBuf, st.Clip.c_str(), sizeof(clipBuf) - 1); clipBuf[sizeof(clipBuf) - 1] = '\0';
                        ImGui::InputText("Nome", nameBuf, sizeof(nameBuf));
                        st.Name = nameBuf;
                        ImGui::InputText("Clip", clipBuf, sizeof(clipBuf));
                        st.Clip = clipBuf;
                        ImGui::DragFloat("Velocidade", &st.Speed, 0.05f, 0.0f, 10.0f);
                        ImGui::Checkbox("Loop", &st.Loop);
                        bool isCurrent = (i == sm.CurrentState);
                        if (ImGui::Button(isCurrent ? "Tocando..." : "Tocar")) {
                            sm.SetState(st.Name);
                            if (m_SelectedEntity.HasComponent<AnimatorComponent>()) {
                                auto& ac = m_SelectedEntity.GetComponent<AnimatorComponent>();
                                if (ac.Skin) ac.Play(st.Clip);
                            }
                        }
                        ImGui::SameLine();
                        if (ImGui::Button("Remover")) removeIdx = i;
                        ImGui::TreePop();
                    }
                }
                if (ImGui::Button("+ Adicionar Estado")) {
                    sm.States.push_back({ "idle", "", 1.0f, true });
                    if (sm.CurrentState < 0) sm.CurrentState = 0;
                }
                if (removeIdx >= 0 && removeIdx < (int)sm.States.size()) {
                    sm.States.erase(sm.States.begin() + removeIdx);
                    sm.CurrentState = sm.States.empty() ? -1 : 0;
                }

                ImGui::Separator();
                int removeTr = -1;
                for (int i = 0; i < (int)sm.Transitions.size(); ++i) {
                    auto& tr = sm.Transitions[(size_t)i];
                    std::string from = tr.From >= 0 && tr.From < (int)sm.States.size() ? sm.States[(size_t)tr.From].Name : "(qualquer)";
                    std::string to = tr.To >= 0 && tr.To < (int)sm.States.size() ? sm.States[(size_t)tr.To].Name : "?";
                    if (ImGui::TreeNodeEx(("Transição " + std::to_string(i + 1) + "  [" + from + " -> " + to + "]").c_str())) {
                        int fromIdx = tr.From, toIdx = tr.To;
                        ImGui::SliderInt("De", &fromIdx, -1, (int)sm.States.size() - 1);
                        ImGui::SliderInt("Para", &toIdx, 0, (int)sm.States.size() - 1);
                        ImGui::DragFloat("Crossfade (s)", &tr.BlendTime, 0.01f, 0.01f, 5.0f);
                        tr.From = glm::clamp(fromIdx, -1, (int)sm.States.size() - 1);
                        tr.To = glm::clamp(toIdx, 0, glm::max((int)sm.States.size() - 1, 0));
                        if (ImGui::Button("Remover")) removeTr = i;
                        ImGui::TreePop();
                    }
                }
                if (ImGui::Button("+ Adicionar Transição"))
                    sm.Transitions.push_back({ -1, 0, 0.3f });
                if (removeTr >= 0 && removeTr < (int)sm.Transitions.size())
                    sm.Transitions.erase(sm.Transitions.begin() + removeTr);

                ImGui::TextDisabled("API: Entity.PlayAnimationState(\"correr\") faz o crossfade.");

                ImGui::TreePop();
            }
            if (removeSM) m_SelectedEntity.RemoveComponent<AnimatorStateMachineComponent>();
        }

        if (m_SelectedEntity.HasComponent<AnimationBlendComponent>()) {
            auto& ab = m_SelectedEntity.GetComponent<AnimationBlendComponent>();
            if (DrawComponentHeader("Blend de Animação", &removeThis)) {
                char aBuf[128], bBuf[128];
                strncpy(aBuf, ab.ClipA.c_str(), sizeof(aBuf) - 1); aBuf[sizeof(aBuf) - 1] = '\0';
                strncpy(bBuf, ab.ClipB.c_str(), sizeof(bBuf) - 1); bBuf[sizeof(bBuf) - 1] = '\0';
                ImGui::InputText("Clip A (peso 0)", aBuf, sizeof(aBuf));
                ImGui::InputText("Clip B (peso 1)", bBuf, sizeof(bBuf));
                ab.ClipA = aBuf;
                ab.ClipB = bBuf;
                ImGui::DragFloat("Peso (0=A, 1=B)", &ab.BlendWeight, 0.01f, 0.0f, 1.0f);
                ImGui::Checkbox("Usar blend", &ab.UseBlend);
                ImGui::TextDisabled("Mistura TRS (slerp de rotação) nos mesmos ossos da skin.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<AnimationBlendComponent>();
        }

        if (m_SelectedEntity.HasComponent<TwoBoneIKComponent>()) {
            auto& ik = m_SelectedEntity.GetComponent<TwoBoneIKComponent>();
            if (DrawComponentHeader("IK de Dois Ossos", &removeThis)) {
                char rBuf[128], mBuf[128], tBuf[128];
                strncpy(rBuf, ik.RootBone.c_str(), sizeof(rBuf) - 1); rBuf[sizeof(rBuf) - 1] = '\0';
                strncpy(mBuf, ik.MidBone.c_str(), sizeof(mBuf) - 1); mBuf[sizeof(mBuf) - 1] = '\0';
                strncpy(tBuf, ik.TipBone.c_str(), sizeof(tBuf) - 1); tBuf[sizeof(tBuf) - 1] = '\0';
                ImGui::InputText("Osso raiz", rBuf, sizeof(rBuf));
                ImGui::InputText("Osso do meio", mBuf, sizeof(mBuf));
                ImGui::InputText("Osso da ponta", tBuf, sizeof(tBuf));
                ik.RootBone = rBuf;
                ik.MidBone = mBuf;
                ik.TipBone = tBuf;
                ImGui::DragFloat3("Alvo (mundo)", &ik.Target.x, 0.1f);
                ImGui::DragFloat("Peso", &ik.Weight, 0.01f, 0.0f, 1.0f);
                ImGui::TextDisabled("Alvo animável por script (SetIKTarget).");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<TwoBoneIKComponent>();
        }

        // ---- IA e Navegação (pilar AAA v0.34) ----
        if (m_SelectedEntity.HasComponent<NavGridComponent>()) {
            auto& ng = m_SelectedEntity.GetComponent<NavGridComponent>();
            if (DrawComponentHeader("NavGrid (navegação)", &removeThis)) {
                (void)ImGui::DragFloat3("Origem", &ng.Origin.x, 0.5f);
                (void)ImGui::DragInt("Células em X", (int*)&ng.Width, 1, 4, 256);
                (void)ImGui::DragInt("Células em Z", (int*)&ng.Depth, 1, 4, 256);
                (void)ImGui::DragFloat("Tamanho da célula", &ng.CellSize, 0.1f, 0.1f, 10.0f);
                ImGui::Checkbox("Rasterizar obstáculos no Play", &ng.AutoBuild);
                if (ImGui::Button("Reconstruir grade agora")) {
                    m_ActiveScene->RebuildNavGrid(m_SelectedEntity);
                }
                if (ng.Grid)
                    ImGui::TextDisabled("Grade: %dx%d células", ng.Grid->GetWidth(), ng.Grid->GetDepth());
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<NavGridComponent>();
        }

        if (m_SelectedEntity.HasComponent<NavObstacleComponent>()) {
            auto& no = m_SelectedEntity.GetComponent<NavObstacleComponent>();
            if (DrawComponentHeader("Nav Obstáculo", &removeThis)) {
                ImGui::DragFloat3("Meia-extensão", &no.HalfExtents.x, 0.1f);
                ImGui::TextDisabled("(0,0,0) = usa a escala do transform.");
                ImGui::TextDisabled("Só bloqueia a navegação — não a física.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<NavObstacleComponent>();
        }

        if (m_SelectedEntity.HasComponent<NavAgentComponent>()) {
            auto& na = m_SelectedEntity.GetComponent<NavAgentComponent>();
            if (DrawComponentHeader("Nav Agent", &removeThis)) {
                ImGui::DragFloat("Velocidade", &na.Speed, 0.1f, 0.0f, 30.0f);
                ImGui::DragFloat("Giro", &na.TurnSpeed, 0.1f, 0.1f, 30.0f);
                ImGui::DragFloat("Distância de parada", &na.StopDistance, 0.05f, 0.05f, 5.0f);
                ImGui::Checkbox("Girar pro movimento", &na.FaceMovement);
                ImGui::Checkbox("Ativo", &na.Enabled);
                if (na.HasDestination) {
                    ImGui::TextDisabled("Destino: (%.1f, %.1f, %.1f)",
                                        na.Destination.x, na.Destination.y, na.Destination.z);
                    if (ImGui::Button("Parar")) m_ActiveScene->StopNavAgent(m_SelectedEntity);
                }
                ImGui::TextDisabled("Use o script: Scene.SetNavDestination(entidade, pos).");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<NavAgentComponent>();
        }

        if (m_SelectedEntity.HasComponent<EnemyAIComponent>()) {
            auto& ai = m_SelectedEntity.GetComponent<EnemyAIComponent>();
            if (DrawComponentHeader("Inimigo IA", &removeThis)) {
                const char* states[] = { "Patrulha", "Persegue", "Ataca" };
                int s = (int)ai.InitialState;
                if (ImGui::Combo("Estado inicial", &s, states, 3)) ai.InitialState = (EnemyAIComponent::State)s;
                ImGui::DragFloat("Alcance de visão", &ai.SightRange, 0.5f, 1.0f, 100.0f);
                ImGui::DragFloat("Alcance de perder", &ai.LoseRange, 0.5f, 1.0f, 120.0f);
                ImGui::DragFloat("Alcance de ataque", &ai.ChaseRange, 0.1f, 0.5f, 30.0f);
                ImGui::DragFloat("Cooldown de ataque (s)", &ai.AttackCooldown, 0.1f, 0.1f, 10.0f);
                ImGui::DragFloat("Dano por ataque", &ai.AttackDamage, 0.5f, 0.0f, 100.0f);
                ImGui::DragFloat("Espera na patrulha (s)", &ai.PatrolWait, 0.1f, 0.0f, 10.0f);
                char tagBuf[64];
                strncpy(tagBuf, ai.TargetTag.c_str(), sizeof(tagBuf) - 1);
                tagBuf[sizeof(tagBuf) - 1] = '\0';
                if (ImGui::InputText("Tag do alvo", tagBuf, sizeof(tagBuf))) ai.TargetTag = tagBuf;

                ImGui::Separator();
                ImGui::TextDisabled("Pontos de patrulha (%.0f,%.0f,%.0f ...)", 
                                    ai.PatrolPoints.empty() ? 0.0f : ai.PatrolPoints[0].x,
                                    ai.PatrolPoints.empty() ? 0.0f : ai.PatrolPoints[0].y,
                                    ai.PatrolPoints.empty() ? 0.0f : ai.PatrolPoints[0].z);
                if (ImGui::Button("+ Ponto aqui (posição da entidade)")) {
                    ai.PatrolPoints.push_back(m_SelectedEntity.GetComponent<TransformComponent>().Translation);
                }
                ImGui::SameLine();
                if (!ai.PatrolPoints.empty() && ImGui::Button("Remover último"))
                    ai.PatrolPoints.pop_back();
                ImGui::TextDisabled("O inimigo precisa de um NavAgent (mesma entidade ou filho) e de um NavGrid na cena.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<EnemyAIComponent>();
        }

        if (m_SelectedEntity.HasComponent<FoliageComponent>()) {
            auto& fc = m_SelectedEntity.GetComponent<FoliageComponent>();
            if (DrawComponentHeader("Foliage (vegetação)", &removeThis)) {
                bool regen = false;
                char fMesh[256];
                strncpy(fMesh, fc.MeshSource.c_str(), sizeof(fMesh) - 1); fMesh[sizeof(fMesh) - 1] = '\0';
                if (ImGui::InputText("Malha (instâncias)", fMesh, sizeof(fMesh))) {
                    fc.MeshSource = fMesh;
                    fc.MeshAsset = fc.MeshSource.empty() ? nullptr : kizuri::Mesh::FromSource(fc.MeshSource);
                    regen = true;
                }
                regen |= ImGui::DragFloat2("Área (XZ)", &fc.AreaSize.x, 0.5f, 1.0f, 200.0f);
                regen |= ImGui::DragFloat("Altura (tronco)", &fc.HeightScale, 0.05f, 0.1f, 10.0f);
                regen |= ImGui::DragInt("Quantidade", (int*)&fc.Count, 1, 1, 5000);
                regen |= ImGui::DragFloat("Escala mínima", &fc.ScaleMin, 0.05f, 0.1f, 10.0f);
                regen |= ImGui::DragFloat("Escala máxima", &fc.ScaleMax, 0.05f, 0.1f, 10.0f);
                regen |= ImGui::DragInt("Semente", (int*)&fc.Seed, 1);
                regen |= ImGui::Checkbox("Vazio no centro", &fc.AvoidCenter);
                ImGui::ColorEdit4("Cor", &fc.Color.x);
                if (ImGui::Button("Regenerar vegetação") || regen) fc.Regenerate();
                ImGui::TextDisabled("Instâncias: %zu · 1 draw call · a malha vem do campo acima.", fc.Instances.size());
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<FoliageComponent>();
        }

        if (m_SelectedEntity.HasComponent<OccluderComponent>()) {
            auto& oc = m_SelectedEntity.GetComponent<OccluderComponent>();
            if (DrawComponentHeader("Occluder", &removeThis)) {
                ImGui::DragFloat3("Meia-extensão", &oc.HalfExtents.x, 0.1f);
                ImGui::DragFloat("Distância máxima de oclusão", &oc.MaxOcclusionDistance, 1.0f, 1.0f, 1000.0f);
                ImGui::TextDisabled("(0,0,0) = usa a escala. Objetos inteiramente atrás não são desenhados.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<OccluderComponent>();
        }

        if (m_SelectedEntity.HasComponent<DecalComponent>()) {
            auto& dc = m_SelectedEntity.GetComponent<DecalComponent>();
            if (DrawComponentHeader("Decal (textura projetada)", &removeThis)) {
                char decBuf[512];
                strncpy(decBuf, dc.TexturePath.c_str(), sizeof(decBuf) - 1);
                decBuf[sizeof(decBuf) - 1] = '\0';
                if (ImGui::InputText("Textura", decBuf, sizeof(decBuf))) {
                    dc.TexturePath = decBuf;
                    dc.Texture = dc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(dc.TexturePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(dc.TexturePath);
                ImGui::ColorEdit4("Cor", &dc.Color.x);
                ImGui::DragInt("Camada de ordenação", &dc.SortingLayer, 1);
                if (dc.Texture) {
                    uint32_t texID = dc.Texture->GetRendererID();
                    ImGui::Image((ImTextureID)(uint64_t)texID, ImVec2(64.0f, 64.0f), ImVec2(0, 1), ImVec2(1, 0));
                }
                ImGui::TextDisabled("Escala do transform = tamanho; projetado ao longo do Z local.");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<DecalComponent>();
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
                ImGui::DragInt("Camada de ordenação", &tc.SortingLayer, 1);
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
                std::string browsedSheet;
                if (FileBrowseButton("Sprite Sheet", "*.png;*.jpg;*.jpeg;*.bmp;*.tga", browsedSheet)) {
                    sac.SheetPath = browsedSheet;
                    sac.SheetTexture = sac.SheetPath.empty() ? nullptr : kizuri::Texture2D::Create(sac.SheetPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(sac.SheetPath);
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
                ImGui::DragInt("Camada de ordenação", &sac.SortingLayer, 1);
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
                std::string browsedAtlas;
                if (FileBrowseButton("Atlas de Tiles", "*.png;*.jpg;*.jpeg;*.bmp;*.tga", browsedAtlas)) {
                    tmc.AtlasPath = browsedAtlas;
                    tmc.AtlasTexture = tmc.AtlasPath.empty() ? nullptr : kizuri::Texture2D::Create(tmc.AtlasPath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(tmc.AtlasPath);
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
                ImGui::DragInt("Camada de ordenação", &tmc.SortingLayer, 1);
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
                if (lc.Type != LightType::Directional) {
                    ImGui::DragFloat("Alcance", &lc.Range, 0.1f, 0.1f, FLT_MAX);
                    ImGui::Checkbox("Projeta sombra", &lc.CastsShadow);
                }
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
                char pTexBuf[512];
                strncpy(pTexBuf, pc.TexturePath.c_str(), sizeof(pTexBuf) - 1);
                pTexBuf[sizeof(pTexBuf) - 1] = '\0';
                if (ImGui::InputText("Textura (vazio = degradê)", pTexBuf, sizeof(pTexBuf))) {
                    pc.TexturePath = pTexBuf;
                    pc.Texture = pc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(pc.TexturePath);
                }
                std::string browsedParticleTex;
                if (FileBrowseButton("Textura de partícula", "*.png;*.jpg;*.jpeg;*.bmp;*.tga", browsedParticleTex)) {
                    pc.TexturePath = browsedParticleTex;
                    pc.Texture = pc.TexturePath.empty() ? nullptr : kizuri::Texture2D::Create(pc.TexturePath);
                }
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(pc.TexturePath);
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
                std::string browsedClip;
                if (FileBrowseButton("Áudio", "*.wav;*.mp3;*.ogg;*.flac", browsedClip))
                    ac.ClipPath = browsedClip;
                ImGui::SameLine();
                if (ImGui::Button("Gerenciador")) RevealFileInContentBrowser(ac.ClipPath);
                ImGui::Checkbox("Em loop", &ac.Loop);
                ImGui::SameLine();
                ImGui::Checkbox("Reproduzir ao iniciar", &ac.PlayOnStart);
                ImGui::Checkbox("Áudio espacial (3D)", &ac.Spatial);
                ImGui::Checkbox("Reverb", &ac.Reverb);
                ImGui::DragFloat("Volume", &ac.Volume, 0.01f, 0.0f, 2.0f);
                const char* groupNames[] = { "SFX", "Música", "UI" };
                ImGui::Combo("Grupo (mixer)", &ac.Group, groupNames, 3);
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
                ImGui::DragFloat("Escala de gravidade", &rb.GravityScale, 0.05f, -5.0f, 5.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<Rigidbody2DComponent>();
        }

        if (m_SelectedEntity.HasComponent<Rigidbody3DComponent>()) {
            auto& rb3 = m_SelectedEntity.GetComponent<Rigidbody3DComponent>();
            if (DrawComponentHeader("Rigidbody 3D", &removeThis)) {
                const char* types[] = { "Estático", "Dinâmico", "Cinemático" };
                int current = (int)rb3.Type;
                if (ImGui::Combo("Tipo", &current, types, IM_ARRAYSIZE(types)))
                    rb3.Type = (Rigidbody3DComponent::BodyType)current;
                ImGui::DragFloat("Massa", &rb3.Mass, 0.1f, 0.01f, 1000.0f);
                ImGui::DragFloat("Escala de gravidade", &rb3.GravityScale, 0.05f, -5.0f, 5.0f);
                ImGui::DragFloat("Amortecimento linear", &rb3.LinearDamping, 0.01f, 0.0f, 5.0f);
                ImGui::DragFloat("Amortecimento angular", &rb3.AngularDamping, 0.01f, 0.0f, 5.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<Rigidbody3DComponent>();
        }

        if (m_SelectedEntity.HasComponent<BoxCollider3DComponent>()) {
            auto& bc3 = m_SelectedEntity.GetComponent<BoxCollider3DComponent>();
            if (DrawComponentHeader("Box Collider 3D", &removeThis)) {
                ImGui::DragFloat3("Meia-extensão", &bc3.HalfExtents.x, 0.05f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<BoxCollider3DComponent>();
        }

        if (m_SelectedEntity.HasComponent<SphereCollider3DComponent>()) {
            auto& sc3 = m_SelectedEntity.GetComponent<SphereCollider3DComponent>();
            if (DrawComponentHeader("Sphere Collider 3D", &removeThis)) {
                ImGui::DragFloat("Raio", &sc3.Radius, 0.05f, 0.01f, 100.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<SphereCollider3DComponent>();
        }

        if (m_SelectedEntity.HasComponent<MeshColliderComponent>()) {
            auto& mc3 = m_SelectedEntity.GetComponent<MeshColliderComponent>();
            if (DrawComponentHeader("Mesh Collider 3D (convexo)", &removeThis)) {
                char mcBuf[512];
                strncpy(mcBuf, mc3.MeshPath.c_str(), sizeof(mcBuf) - 1);
                mcBuf[sizeof(mcBuf) - 1] = '\0';
                if (ImGui::InputText("Malha (vazio = do MeshRenderer)", mcBuf, sizeof(mcBuf)))
                    mc3.MeshPath = mcBuf;
                ImGui::DragInt("Pontos máximos (amostragem)", (int*)&mc3.MaxPoints, 1, 8, 512);
                ImGui::TextDisabled("Envoltório convexo da geometria (Bullet).");
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<MeshColliderComponent>();
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

        if (m_SelectedEntity.HasComponent<CircleCollider2DComponent>()) {
            auto& cc = m_SelectedEntity.GetComponent<CircleCollider2DComponent>();
            if (DrawComponentHeader("Circle Collider 2D", &removeThis)) {
                ImGui::DragFloat2("Offset", &cc.Offset.x, 0.05f);
                ImGui::DragFloat("Raio", &cc.Radius, 0.05f, 0.01f, FLT_MAX);
                ImGui::DragFloat("Densidade", &cc.Density, 0.05f, 0.0f, FLT_MAX);
                ImGui::DragFloat("Fricção", &cc.Friction, 0.02f, 0.0f, 1.0f);
                ImGui::DragFloat("Restituição", &cc.Restitution, 0.02f, 0.0f, 1.0f);
                ImGui::TreePop();
            }
            if (removeThis) m_SelectedEntity.RemoveComponent<CircleCollider2DComponent>();
        }

        if (m_SelectedEntity.HasComponent<NativeScriptComponent>()) {
            auto& nsc = m_SelectedEntity.GetComponent<NativeScriptComponent>();
            if (DrawComponentHeader("Script Nativo", &removeThis)) {
                auto classNames = ScriptEngine::GetRegistry().GetClassNames();
                if (classNames.empty()) {
                    ImGui::TextDisabled("Nenhum script registrado. Abra um projeto (ou aperte Play — a engine compila e carrega sozinha).");
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
        if (!m_SelectedEntity.HasComponent<CameraComponent>() && ImGui::MenuItem("Camera")) {
            // Câmera nova nasce no modo do viewport atual (o default do
            // componente é 2D — o testador reclamou que "câmera nova sempre
            // nasce 2D mesmo em cena 3D").
            auto& newCam = m_SelectedEntity.AddComponent<CameraComponent>();
            if (m_ViewportMode == ViewportMode::Mode3D) {
                newCam.Type = CameraComponent::ProjectionType::Perspective3D;
                newCam.Primary = true;
            }
        }
        if (!m_SelectedEntity.HasComponent<CameraFollowComponent>() && ImGui::MenuItem("Camera Follow (segue alvo)"))
            m_SelectedEntity.AddComponent<CameraFollowComponent>();
        if (!m_SelectedEntity.HasComponent<MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer")) {
            m_SelectedEntity.AddComponent<MeshRendererComponent>();
        }
        if (!m_SelectedEntity.HasComponent<LODComponent>() && ImGui::MenuItem("LOD (níveis de detalhe)")) {
            auto& lod = m_SelectedEntity.AddComponent<LODComponent>();
            lod.Levels.push_back({ "builtin:cube", 100.0f, nullptr });
        }
        if (!m_SelectedEntity.HasComponent<CharacterControllerComponent>() && ImGui::MenuItem("Character Controller"))
            m_SelectedEntity.AddComponent<CharacterControllerComponent>();
        if (!m_SelectedEntity.HasComponent<TimelineComponent>() && ImGui::MenuItem("Timeline (cutscene)"))
            m_SelectedEntity.AddComponent<TimelineComponent>();
        if (!m_SelectedEntity.HasComponent<TerrainComponent>() && ImGui::MenuItem("Terreno (heightmap)")) {
            auto& t = m_SelectedEntity.AddComponent<TerrainComponent>();
            if (!m_SelectedEntity.HasComponent<MeshRendererComponent>())
                m_SelectedEntity.AddComponent<MeshRendererComponent>();
            if (!m_SelectedEntity.HasComponent<Rigidbody3DComponent>()) {
                // Colisor estático (heightfield) — personagens andam no terreno.
                auto& rb = m_SelectedEntity.AddComponent<Rigidbody3DComponent>();
                rb.Type = Rigidbody3DComponent::BodyType::Static;
            }
            t.Regenerate();
            m_SelectedEntity.GetComponent<MeshRendererComponent>().MeshAsset = t.GeneratedMesh;
        }
        if (!m_SelectedEntity.HasComponent<LightComponent>() && ImGui::MenuItem("Light"))
            m_SelectedEntity.AddComponent<LightComponent>();
        if (!m_SelectedEntity.HasComponent<AnimatorComponent>() && ImGui::MenuItem("Animador (skinning)"))
            m_SelectedEntity.AddComponent<AnimatorComponent>();
        if (!m_SelectedEntity.HasComponent<AnimatorStateMachineComponent>() && ImGui::MenuItem("Máquina de Estados (animação)"))
            m_SelectedEntity.AddComponent<AnimatorStateMachineComponent>();
        if (!m_SelectedEntity.HasComponent<AnimationBlendComponent>() && ImGui::MenuItem("Blend de Animação (2 clips)"))
            m_SelectedEntity.AddComponent<AnimationBlendComponent>();
        if (!m_SelectedEntity.HasComponent<TwoBoneIKComponent>() && ImGui::MenuItem("IK de Dois Ossos")) {
            auto& ik = m_SelectedEntity.AddComponent<TwoBoneIKComponent>();
            ik.RootBone = "root";
            ik.MidBone = "bone1";
            ik.TipBone = "bone2";
        }
        if (!m_SelectedEntity.HasComponent<TextComponent>() && ImGui::MenuItem("Texto (HUD)"))
            m_SelectedEntity.AddComponent<TextComponent>();
        if (!m_SelectedEntity.HasComponent<SpriteAnimationComponent>() && ImGui::MenuItem("Animação de Sprite"))
            m_SelectedEntity.AddComponent<SpriteAnimationComponent>();
        if (!m_SelectedEntity.HasComponent<DecalComponent>() && ImGui::MenuItem("Decal (textura projetada)"))
            m_SelectedEntity.AddComponent<DecalComponent>();
        if (!m_SelectedEntity.HasComponent<OccluderComponent>() && ImGui::MenuItem("Occluder (bloqueia visão)"))
            m_SelectedEntity.AddComponent<OccluderComponent>();
        if (!m_SelectedEntity.HasComponent<FoliageComponent>() && ImGui::MenuItem("Foliage (vegetação)"))
            m_SelectedEntity.AddComponent<FoliageComponent>();
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
        if (!m_SelectedEntity.HasComponent<CircleCollider2DComponent>() && ImGui::MenuItem("Circle Collider 2D"))
            m_SelectedEntity.AddComponent<CircleCollider2DComponent>();
        if (!m_SelectedEntity.HasComponent<NativeScriptComponent>() && ImGui::MenuItem("Script C#"))
            m_SelectedEntity.AddComponent<NativeScriptComponent>();
        ImGui::Separator();
        // Física 3D (pilar AAA v0.34 — grupo completo no menu).
        if (!m_SelectedEntity.HasComponent<Rigidbody3DComponent>() && ImGui::MenuItem("Rigidbody 3D"))
            m_SelectedEntity.AddComponent<Rigidbody3DComponent>();
        if (!m_SelectedEntity.HasComponent<BoxCollider3DComponent>() && ImGui::MenuItem("Box Collider 3D"))
            m_SelectedEntity.AddComponent<BoxCollider3DComponent>();
        if (!m_SelectedEntity.HasComponent<SphereCollider3DComponent>() && ImGui::MenuItem("Sphere Collider 3D"))
            m_SelectedEntity.AddComponent<SphereCollider3DComponent>();
        if (!m_SelectedEntity.HasComponent<MeshColliderComponent>() && ImGui::MenuItem("Mesh Collider 3D (convexo)"))
            m_SelectedEntity.AddComponent<MeshColliderComponent>();
        ImGui::Separator();
        // IA e Navegação (pilar AAA v0.34).
        if (!m_SelectedEntity.HasComponent<NavGridComponent>() && ImGui::MenuItem("NavGrid (grade de navegação)"))
            m_SelectedEntity.AddComponent<NavGridComponent>();
        if (!m_SelectedEntity.HasComponent<NavObstacleComponent>() && ImGui::MenuItem("Nav Obstáculo"))
            m_SelectedEntity.AddComponent<NavObstacleComponent>();
        if (!m_SelectedEntity.HasComponent<NavAgentComponent>() && ImGui::MenuItem("Nav Agent (segue caminho)"))
            m_SelectedEntity.AddComponent<NavAgentComponent>();
        if (!m_SelectedEntity.HasComponent<EnemyAIComponent>() && ImGui::MenuItem("Inimigo IA (patrulha/persegue)"))
            m_SelectedEntity.AddComponent<EnemyAIComponent>();
        ImGui::Separator();
        if (!m_SelectedEntity.HasComponent<UICanvasComponent>() && ImGui::MenuItem("UI Canvas"))
            m_SelectedEntity.AddComponent<UICanvasComponent>();
        if (!m_SelectedEntity.HasComponent<UIRectComponent>() && ImGui::MenuItem("UI Rect"))
            m_SelectedEntity.AddComponent<UIRectComponent>();
        if (!m_SelectedEntity.HasComponent<UIButtonComponent>() && ImGui::MenuItem("UI Botão")) {
            m_SelectedEntity.AddComponent<UIButtonComponent>();
            if (!m_SelectedEntity.HasComponent<UIRectComponent>())
                m_SelectedEntity.AddComponent<UIRectComponent>();
        }
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

    // Hub / telinha de carregamento: o editor completo não é desenhado —
    // a seleção de projeto (ou o loading) cobre tudo, e os popups de
    // projeto continuam funcionando por cima.
    if (m_EditorState != EditorState::Editor) {
        if (m_EditorState == EditorState::Hub) DrawHub();
        else DrawLoadingScreen();
        DrawProjectModals();
        return;
    }

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

    // F5 = Play, Shift+F5 = Stop — o mesmo botão do toolbar do viewport.
    // Fora do Play o F5 começa (e não conflita com digitação), durante o
    // Play o F5 também para (mata a cópia em execução). Igual aos atalhos
    // acima, usa "acabou de ser pressionado" pra não reiniciar a cada frame.
    {
        bool f5Down = kizuri::Input::IsKeyPressed(kizuri::Key::F5);
        bool f5JustPressed = f5Down && !m_PrevF5KeyDown;
        m_PrevF5KeyDown = f5Down;
        if (f5JustPressed && !io.WantTextInput) {
            if (m_SceneState == SceneState::Play) OnSceneStop();
            else OnScenePlay();
        }
    }

    // Ferramentas de edição: Del = apagar a entidade selecionada (com undo,
    // mesmo comando do menu de contexto da Hierarquia); Ctrl+D = duplicar
    // (com a subárvore) e já selecionar a cópia. Edge-detect no D pra não
    // duplicar a cada frame enquanto o Ctrl+D estiver segurado.
    if (m_SceneState == SceneState::Edit && m_SelectedEntity && !io.WantTextInput) {
        bool delDown = kizuri::Input::IsKeyPressed(kizuri::Key::Delete);
        if (delDown) {
            Entity toDelete = m_SelectedEntity;
            m_SelectedEntity = {};
            ClearMultiSelection();
            m_History.Push(CreateRef<DeleteEntityCommand>(toDelete));
            m_ActiveScene->DestroyEntity(toDelete);
        }
        bool dDown = kizuri::Input::IsKeyPressed(kizuri::Key::D);
        bool dJustPressed = dDown && !m_PrevDKeyDown;
        m_PrevDKeyDown = dDown;
        if (io.KeyCtrl && dJustPressed) {
            Entity copy = m_ActiveScene->DuplicateEntity(m_SelectedEntity);
            if (copy) {
                m_SelectedEntity = copy;
                AutoSwitchViewportMode();
            }
        }
    }
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — atalhos undo/redo + F5 ok");

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
    DrawExportModal();
    DrawSavePrefabModal();
    DrawUpdateModals();
    DrawAndroidExportModals();
    DrawScriptTemplateModal();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — modais ok");
    if (!m_ViewportMaximized) {
        DrawSceneHierarchy();
        DrawInspector();
        DrawConsole();
        DrawContentBrowser();
        // Painéis dockáveis (Profiler, Game View, Material Editor, Animator,
        // Project Settings) — cada um é uma janela ImGui própria que entra no
        // mesmo dockspace; só os visíveis (menu Janelas) são desenhados.
        if (m_PanelContext) {
            for (auto& panel : m_Panels)
                if (panel->IsVisible()) panel->OnImGuiRender();
        }
    } else {
        KZ_CORE_TRACE("EditorLayer::OnImGuiRender — painéis ocultos (viewport maximizado)");
    }

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

    // Os bounds do viewport têm que ser a POSIÇÃO REAL onde o framebuffer é
    // desenhado — a do cursor logo antes do ImGui::Image (abaixo do título da
    // janela, do PanelHeader e da toolbar). Usar GetWindowContentRegionMin()
    // aqui contava a toolbar como parte do viewport: o conteúdo renderizado
    // ficava visualmente mais baixo do que o gizmo/UI/picking supunham —
    // gizmo "em cima do objeto", clique registrando mais abaixo e seleção
    // errada (o famoso deslocamento vertical do v0.29+).
    ImVec2 viewportPos = ImGui::GetCursorScreenPos();
    m_ViewportBounds[0] = { viewportPos.x, viewportPos.y };
    m_ViewportBounds[1] = { viewportPos.x + panelSize.x, viewportPos.y + panelSize.y };

    uint32_t textureID = m_Framebuffer->GetColorAttachmentRendererID();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — chamando ImGui::Image (textureID={0}, {1}x{2})", textureID, panelSize.x, panelSize.y);
    // O framebuffer é preenchido de baixo para cima (origem OpenGL), então
    // invertemos as UVs verticalmente para a imagem aparecer com a
    // orientação correta dentro do ImGui.
    ImGui::Image((ImTextureID)(uint64_t)textureID, panelSize, ImVec2(0, 1), ImVec2(1, 0));
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — ImGui::Image ok");

    // Overlay de estatísticas (Profiler do viewport): FPS, tempo de frame,
    // draw calls e triângulos do frame anterior. Desligável em
    // Configurações > Editor.
    if (m_ShowStats) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        float fps = m_FpsSmoothed;
        float ms = fps > 0.0f ? 1000.0f / fps : 0.0f;
        const kizuri::GraphicsSettings& gs = kizuri::Renderer3D::GetGraphicsSettings();
        char buf[256];
        snprintf(buf, sizeof(buf),
                 "%.0f FPS  (%.1f ms)\nDraw calls: %u\nTriângulos: %u\nRes: %.0fx%.0f  ·  GLSL %d",
                 fps, ms,
                 kizuri::RenderCommand::GetFrameDrawCalls(),
                 kizuri::RenderCommand::GetFrameTriangles(),
                 panelSize.x, panelSize.y,
                 kizuri::GetGLSLVersion());
        // Fundo semi-transparente pra legibilidade sobre a cena.
        ImVec2 textSize = ImGui::CalcTextSize(buf);
        float pad = 6.0f;
        dl->AddRectFilled(ImVec2(pos.x + pad, pos.y + pad),
                          ImVec2(pos.x + pad + textSize.x + pad * 2.0f,
                                 pos.y + pad + textSize.y + pad * 2.0f),
                          IM_COL32(10, 14, 20, 150), 6.0f);
        dl->AddText(ImVec2(pos.x + pad * 2.0f, pos.y + pad * 2.0f),
                    IM_COL32(220, 230, 245, 235), buf);
        (void)gs;
    }

    // Janela de diagnóstico do TEXTO: atlas da fonte em tempo real + estado
    // do blending + status do bake. Ligada em Configurações > Editor >
    // "Diagnóstico de Texto" — pra investigar "texto em retângulo branco".
    if (m_ShowTextDiag) {
        ImGui::SetNextWindowSize(ImVec2(560, 380), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(60, 60), ImGuiCond_FirstUseEver);
        if (ImGui::Begin("Diagnóstico de Texto", &m_ShowTextDiag)) {
            ImGui::TextUnformatted(kizuri::TextRenderer::GetDiagnostics().c_str());
            ImGui::Text("GL_BLEND agora: %s", glIsEnabled(GL_BLEND) ? "LIGADO" : "DESLIGADO");
            ImGui::Text("(o texto força blending no draw; ver TextRenderer::DrawString)");
            ImGui::Separator();
            auto atlas = kizuri::TextRenderer::GetAtlasTexture();
            if (atlas) {
                ImGui::Text("Atlas (cada letra = glifo branco sobre transparente):");
                ImGui::Image((ImTextureID)(uint64_t)atlas->GetRendererID(), ImVec2(512, 512));
            } else {
                ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "Atlas NÃO gerado — procure no log por 'TextRenderer:'.");
            }
            ImGui::Spacing();
            ImGui::TextDisabled("Letras visíveis no atlas = bake OK. Se o texto na cena sair branco\n"
                                "com o atlas cheio, é estado de blending/ordem de passe.");
        }
        ImGui::End();
    }

    // Faixa de DIAGNÓSTICO (vermelha): mostra na tela a última falha de
    // driver/shader/FBO — sem precisar caçar o KizuriEngine.log. Some sozinha
    // quando um frame passa limpo.
    const std::string& diag = kizuri::GetShaderDiagnostic();
    if (!diag.empty()) {
        ImDrawList* dl = ImGui::GetWindowDrawList();
        ImVec2 pos = ImGui::GetCursorScreenPos();
        ImVec2 textSize = ImGui::CalcTextSize(diag.c_str());
        float pad = 8.0f;
        dl->AddRectFilled(ImVec2(pos.x + pad, pos.y + 8.0f),
                          ImVec2(pos.x + pad + textSize.x + pad * 2.0f,
                                 pos.y + 8.0f + textSize.y + pad * 2.0f),
                          IM_COL32(180, 30, 30, 220), 6.0f);
        dl->AddText(ImVec2(pos.x + pad * 2.0f, pos.y + 8.0f + pad),
                    IM_COL32(255, 235, 235, 255), diag.c_str());
    }

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
        DrawLightGizmo();
        DrawColliderGizmo();
        if (m_ShowColliders) DrawAllColliders(); // overlay de física debug
        DrawNavDebug();
    } else {
        KZ_CORE_TRACE("EditorLayer::OnImGuiRender — gizmo pulado (Play)");
        DrawNavDebug(); // grade + caminhos da IA sempre visíveis no Play
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

    // Janela de configurações (Arquivo > Configurações).
    // Configurações unificadas no painel Project Settings (menu Janelas).
    // A janela antiga "Configurações" foi descontinuada (redundante).

    // Overlay de progresso do carregamento assíncrono de cena. A janela é
    // desenhada por último (fica por cima de tudo) e o loop de eventos segue
    // vivo — com projeto grande o usuário vê o progresso e pode fechar o
    // editor, em vez de ficar com a janela congelada.
    if (m_SceneLoading) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(420.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.96f));
        if (ImGui::Begin("##loading_overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted("Carregando cena...");
            ImGui::TextColored(ImVec4(0.7f, 0.75f, 0.85f, 1.0f), "%s", m_PendingScenePath.c_str());
            ImGui::Spacing();
            char pct[16];
            snprintf(pct, sizeof(pct), "%.0f%%", m_PendingLoadProgress * 100.0f);
            ImGui::ProgressBar(m_PendingLoadProgress, ImVec2(-1.0f, 0.0f), pct);
            ImGui::TextDisabled("A janela continua respondendo — você pode fechar a qualquer momento.");
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // Overlay de compilação do C# (Play) — dotnet build roda em segundo
    // plano; aqui só o aviso + spinner, a janela nunca trava.
    if (m_PlayBuildActive && !m_PlayBuildDone) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(360.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.08f, 0.10f, 0.14f, 0.96f));
        if (ImGui::Begin("##compile_overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::TextUnformatted("Compilando assembly do jogo...");
            ImGui::Spacing();
            ImGui::ProgressBar(-1.0f, ImVec2(-1.0f, 0.0f), "dotnet build (1ª vez pode demorar)");
            ImGui::TextDisabled("A janela continua respondendo.");
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    // Erro persistente do build C# (Play) — antes só ia pro console e
    // "sumia rápido"; agora fica na tela até o usuário fechar ou um novo
    // Play tentar compilar de novo.
    if (!m_PlayBuildActive && !m_PlayBuildError.empty()) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(ImVec2(520.0f, 0.0f));
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.16f, 0.06f, 0.08f, 0.97f));
        if (ImGui::Begin("##build_error_overlay", nullptr,
                         ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoDocking | ImGuiWindowFlags_NoResize |
                         ImGuiWindowFlags_NoSavedSettings)) {
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.45f, 0.4f, 1.0f));
            ImGui::TextUnformatted("Falha ao compilar o jogo (C#)");
            ImGui::PopStyleColor();
            ImGui::Spacing();
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.85f, 0.8f, 1.0f));
            std::string display = m_PlayBuildError;
            // Dicas amigáveis (v0.37.0): se o código usa API antiga/errada,
            // o erro mostra a correção provável.
            std::string hints;
            std::error_code sec;
            if (Project::GetActive()) {
                std::filesystem::path sourceDir = std::filesystem::path(Project::GetActive()->GetProjectDirectory()) / "Source";
                for (auto& f : std::filesystem::directory_iterator(sourceDir, sec)) {
                    if (f.path().extension() != ".cs") continue;
                    std::ifstream in(f.path().string());
                    std::string txt((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
                    if (txt.find("OnStart") != std::string::npos) hints += "OnStart nao existe — o metodo e OnCreate()\n";
                    if (txt.find("KeyCode") != std::string::npos) hints += "KeyCode nao existe — use Key (ex: Key.A)\n";
                    if (txt.find("Entity.Move(") != std::string::npos) hints += "Entity.Move nao existe — use Entity.Translate(...)\n";
                    if (txt.find("KizuriScript") != std::string::npos) hints += "KizuriScript nao existe — a classe base e Script\n";
                }
            }
            if (!hints.empty())
                display += "\n\n---- TALVEZ VOCE QUISESSE DIZER ----\n" + hints;
            ImGui::TextWrapped("%s", display.c_str());
            ImGui::PopStyleColor();
            ImGui::Spacing();
            if (ImGui::Button("Fechar")) m_PlayBuildError.clear();
            ImGui::SameLine();
            ImGui::TextDisabled("O Play funciona sem scripts C#; o erro é só do assembly do jogo.");
        }
        ImGui::End();
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();
    }

    ImGui::End();
    KZ_CORE_TRACE("EditorLayer::OnImGuiRender — fim");
}

// ---------------------------------------------------------------------------
// Atualização automática — fluxo: check (thread) -> modal Sim/Não (com
// "não perguntar novamente" persistido) -> download -> instala -> relaunch.
// ---------------------------------------------------------------------------
void EditorLayer::StartUpdateCheck() {
    {
        std::lock_guard lock(m_UpdateMutex);
        if (m_UpdateBusy) return;
        m_UpdateBusy = true;
        m_UpdateState = 1;
        m_UpdateError.clear();
    }
    if (m_UpdateThread.joinable()) m_UpdateThread.join();

    m_UpdateThread = std::thread([this]() {
        std::string err;
        auto info = kizuri::Updater::CheckForUpdate(err);
        {
            std::lock_guard lock(m_UpdateMutex);
            m_UpdateError = std::move(err);
            m_UpdateBusy = false;
            if (info.Valid && kizuri::Updater::GetSkipVersion() != info.Version) {
                m_UpdateVersion = info.Version;
                m_UpdateUrl = info.DownloadUrl;
                m_UpdateState = 2; // nova versão disponível — pergunta
            } else if (!err.empty() && !kizuri::Updater::GetApiUrl().empty()) {
                // API configurada mas falhou (rede/HTTP) — mostra o motivo
                // (silencioso se nunca foi configurada).
                m_UpdateState = 6;
            } else {
                m_UpdateState = 0;
            }
        }
    });
}

void EditorLayer::DrawUpdateModals() {
    int state = m_UpdateState;

    // BeginPopupModal precisa de um OpenPopup antes (a transição pro estado
    // acontece na thread do check).
    if (state == 2 && !m_UpdatePopupOpened) {
        m_UpdatePopupOpened = true;
        ImGui::OpenPopup("Nova versão disponível");
    }
    if (state == 6 && !m_UpdateErrPopupOpened) {
        m_UpdateErrPopupOpened = true;
        ImGui::OpenPopup("Falha na atualização");
    }

    if (state == 1) {
        // Verificando em segundo plano: sem janela/aviso na tela (o testador
        // reclamou de "coisas em cima sem pedir"). O resultado aparece
        // somente via modal quando há atualização (ou erro).
        return;
    }

    if (state == 2) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Nova versão disponível", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("A versão %s da Kizuri Engine está disponível.", m_UpdateVersion.c_str());
            ImGui::TextWrapped("Você está na v%s.", KIZURI_VERSION);
            ImGui::Spacing();
            ImGui::Checkbox("Não perguntar novamente", &m_UpdateSkipAsk);
            ImGui::Spacing();
            if (ImGui::Button("Atualizar agora", ImVec2(140.0f, 0.0f))) {
                if (m_UpdateSkipAsk) kizuri::Updater::SetSkipVersion(m_UpdateVersion);
                m_UpdateSkipAsk = false;
                // Baixa na thread; progresso aparece no modal seguinte.
                {
                    std::lock_guard lock(m_UpdateMutex);
                    m_UpdateState = 3;
                    m_UpdateBusy = true;
                }
                if (m_UpdateThread.joinable()) m_UpdateThread.join();
                std::string url = m_UpdateUrl;
                m_UpdateThread = std::thread([this, url]() {
                    std::string err;
                    std::string zip = "kizuri_update.zip";
                    bool ok = kizuri::Updater::Download(url, zip, err);
                    {
                        std::lock_guard lock(m_UpdateMutex);
                        m_UpdateError = std::move(err);
                        m_UpdateBusy = false;
                        m_UpdateState = ok ? 4 : 6; // 4 instala
                        m_UpdateZip = zip;
                    }
                });
                ImGui::CloseCurrentPopup();
            }
            ImGui::SameLine();
            if (ImGui::Button("Depois", ImVec2(100.0f, 0.0f))) {
                if (m_UpdateSkipAsk) kizuri::Updater::SetSkipVersion(m_UpdateVersion);
                m_UpdateSkipAsk = false;
                m_UpdateState = 0;
                m_UpdatePopupOpened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
        return;
    }

    if (state >= 3 && state <= 5) {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        if (ImGui::Begin("##update_work", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            if (state == 3)
                ImGui::Text("Baixando a nova versão...");
            else if (state == 4) {
                ImGui::Text("Instalando a nova versão...");
                // Inicia a instalação SO uma vez (quando a thread do
                // download terminou).
                std::lock_guard lock(m_UpdateMutex);
                if (!m_UpdateBusy && !m_UpdateInstallStarted) {
                    m_UpdateInstallStarted = true;
                    m_UpdateBusy = true;
                    m_UpdateState = 4;
                    std::string zip = m_UpdateZip;
                    if (m_UpdateThread.joinable()) m_UpdateThread.join();
                    m_UpdateThread = std::thread([this, zip]() {
                        std::string err;
                        bool ok = kizuri::Updater::Install(zip, err);
                        {
                            std::lock_guard lock(m_UpdateMutex);
                            m_UpdateError = std::move(err);
                            m_UpdateBusy = false;
                            m_UpdateState = ok ? 5 : 6;
                        }
                    });
                }
            } else {
                ImGui::Text("Reiniciando editor...");
                std::string err;
                kizuri::Updater::Relaunch(err);
                kizuri::Application::Get().Close();
            }
            ImGui::End();
        }
        return;
    }

    if (state == 6) {
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Falha na atualização", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("Não foi possível atualizar: %s", m_UpdateError.empty() ? "erro desconhecido" : m_UpdateError.c_str());
            ImGui::Spacing();
            if (ImGui::Button("OK", ImVec2(100.0f, 0.0f))) {
                m_UpdateState = 0;
                m_UpdateErrPopupOpened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

// ---------------------------------------------------------------------------
// Export Android LOCAL — a engine compila o APK sozinha (thread de fundo;
// resultado aparece num popup e as etapas vão pro console).
// ---------------------------------------------------------------------------
void EditorLayer::StartAndroidExport() {
    if (m_AndroidRunning) return;
    {
        std::lock_guard lock(m_AndroidMutex);
        m_AndroidRunning = true;
        m_AndroidDone = false;
        m_AndroidResult.clear();
    }
    if (m_AndroidThread.joinable()) m_AndroidThread.join();

    // Captura os parâmetros antes da thread.
    std::string csproj, engineRoot;
    GetGameBuildInfo(csproj, engineRoot);
    std::string outputDir = m_ExportDirBuffer;
    std::string gameName = m_ExportGameName;
    std::string projectDir;
    if (Project::GetActive())
        projectDir = Project::GetActive()->GetProjectDirectory();
    auto tools = m_AndroidTools;

    m_AndroidThread = std::thread([this, csproj, engineRoot, outputDir, gameName, projectDir, tools]() {
        // Staging do conteúdo do jogo (cena + assets), sem lixo de build.
        namespace fs = std::filesystem;
        std::string stageErr;
        std::string stage = (fs::path(outputDir) / "android_build" / "stage_game").string();
        {
            std::error_code ec;
            fs::create_directories(stage, ec);
            if (!projectDir.empty() && fs::is_directory(projectDir, ec)) {
                for (auto& e : fs::recursive_directory_iterator(projectDir, ec)) {
                    if (e.is_directory(ec)) continue;
                    std::string rel = fs::relative(e.path(), projectDir).generic_string();
                    if (rel.rfind("bin/", 0) == 0 || rel.rfind("obj/", 0) == 0 ||
                        rel.rfind("Export/", 0) == 0 || rel.rfind(".git/", 0) == 0)
                        continue;
                    fs::path dst = fs::path(stage) / rel;
                    if (dst.has_parent_path()) fs::create_directories(dst.parent_path(), ec);
                    fs::copy_file(e.path(), dst, fs::copy_options::overwrite_existing, ec);
                }
            }
            // A cena atual vira a inicial se estiver fora da pasta do projeto.
            if (!m_ScenePath.empty()) {
                fs::path startScene = fs::path(stage) / "Start.kzscene";
                if (!fs::exists(startScene, ec))
                    fs::copy_file(m_ScenePath, startScene, fs::copy_options::overwrite_existing, ec);
            }
        }

        auto logCb = [this](const std::string& m) { KZ_CORE_INFO("{0}", m); };
        std::string apkPath, err;
        bool ok = kizuri::AndroidExporter::Export(tools, engineRoot, csproj, stage,
                                                  gameName.empty() ? "KizuriGame" : gameName,
                                                  outputDir, apkPath, err, logCb);
        {
            std::lock_guard lock(m_AndroidMutex);
            m_AndroidRunning = false;
            m_AndroidDone = true;
            m_AndroidResult = ok ? ("APK gerado: " + apkPath) : ("FALHOU: " + err);
        }
    });
}

void EditorLayer::DrawAndroidExportModals() {
    bool running, done;
    std::string result;
    {
        std::lock_guard lock(m_AndroidMutex);
        running = m_AndroidRunning;
        done = m_AndroidDone;
        result = m_AndroidResult;
    }

    if (running) {
        ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowBgAlpha(0.9f);
        if (ImGui::Begin("##android_export", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoMove)) {
            ImGui::Text("Exportando Android...");
            ImGui::TextDisabled("Etapas no console (CMake/NDK, dotnet publish, aapt2, assinatura).");
            ImGui::End();
        }
        return;
    }
    if (done) {
        if (!m_AndroidErrPopupOpened) {
            m_AndroidErrPopupOpened = true;
            ImGui::OpenPopup("Exportação Android");
        }
        bool failed = result.rfind("FALHOU", 0) == 0;
        ImVec2 center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
        if (ImGui::BeginPopupModal("Exportação Android", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
            ImGui::TextWrapped("%s", result.c_str());
            ImGui::Spacing();
            if (ImGui::Button(failed ? "OK" : "Abrir pasta", ImVec2(120.0f, 0.0f))) {
                if (!failed) {
                    std::string cmd;
#if defined(_WIN32)
                    cmd = "explorer /select,\"" + std::filesystem::path(result.substr(10)).string() + "\"";
#else
                    cmd = "xdg-open \"" + std::filesystem::path(result.substr(10)).parent_path().string() + "\"";
#endif
                    if (std::system(cmd.c_str()) != 0) { /* ignora: abrir pasta é best-effort */ }
                }
                m_AndroidDone = false;
                m_AndroidErrPopupOpened = false;
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }
}

// Seletor de template de script (v0.37.0): pergunta qual script pronto criar.
void EditorLayer::DrawScriptTemplateModal() {
    if (m_RequestScriptTemplate) {
        m_RequestScriptTemplate = false;
        ImGui::OpenPopup("Criar Script C#");
    }
    ImVec2 center = ImGui::GetMainViewport()->GetCenter();
    ImGui::SetNextWindowPos(center, ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    if (ImGui::BeginPopupModal("Criar Script C#", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::TextWrapped("Escolha o template (código pronto, comentado em português):");
        ImGui::Spacing();
        const char* names[] = { "Vazio", "PlayerController (3D, WASD + pulo)", "Movement2D (setas + física)", "Coletável (colisão + som)" };
        for (int i = 0; i < 4; ++i) {
            if (ImGui::Button(names[i], ImVec2(320.0f, 0.0f))) {
                CreateNewCSharpScript(m_ScriptTemplateDir, i);
                ImGui::CloseCurrentPopup();
            }
            if (i < 3) ImGui::Spacing();
        }
        ImGui::Spacing();
        ImGui::TextDisabled("O script vai pra pasta atual do Content Browser / Source.");
        ImGui::EndPopup();
    }
}
