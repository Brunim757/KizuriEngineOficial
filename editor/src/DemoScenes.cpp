#include "EditorLayer.hpp"
#include <kizuri/ecs/Components.hpp>
#include <kizuri/ecs/Entity.hpp>
#include <kizuri/project/Project.hpp>
#include <kizuri/net/NetworkFacade.hpp>
#include <kizuri/renderer/TextRenderer.hpp>
#include <kizuri/core/Log.hpp>
#include <glm/gtc/random.hpp>
#include <cstdio>
#include <string>
#include <filesystem>
#include <kizuri/core/EmbeddedContent.hpp>
#include <kizuri/renderer/Renderer3D.hpp>

static std::string FindContentFile(const std::string& name) {
    namespace fs = std::filesystem;
    auto tryPath = [&](const std::string& a) {
        if (fs::exists(a)) return a;
        return std::string();
    };
    if (auto r = tryPath(name); !r.empty()) return r;
    if (auto r = tryPath("content/" + name); !r.empty()) return r;
    if (auto r = tryPath("../content/" + name); !r.empty()) return r;
    return {};
}



static std::string PickDemoAsset(const char* primary, const char* fallback) {
    if (!primary || !primary[0]) return {};
    auto p = FindContentFile(primary);
    if (!p.empty()) return p;
    return fallback ? fallback : "";
}

using namespace kizuri;
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

    Renderer3D::SetEnvironmentHDRIPath("");
    std::string hdri = FindContentFile("skies/qwantani_puresky_1k.hdr");
    if (!hdri.empty()) {
        Renderer3D::SetEnvironmentHDRIPath(hdri);
        KZ_CORE_INFO("Demo 3D: céu HDRI do content pack carregado ({0}).", hdri);
    } else if (kizuri::HasEmbeddedResource(std::string("skies/sky_gradient.hdr"))) {
        Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");
        KZ_CORE_INFO("Demo 3D: céu EMBUTIDO (kzres://skies/sky_gradient.hdr).");
    }

    Entity embeddedCube = m_ActiveScene->CreateEntity("Cubo Embutido (kzres)");
    auto& ecm = embeddedCube.AddComponent<MeshRendererComponent>();
    ecm.MeshSource = "kzres://models/Cube.glb";
    ecm.MeshAsset = Mesh::FromSource(ecm.MeshSource);
    ecm.MeshMaterial.Albedo = { 0.85f, 0.35f, 0.3f };
    ecm.MeshMaterial.Roughness = 0.4f;
    embeddedCube.GetComponent<TransformComponent>().Translation = { -2.6f, 0.5f, 1.6f };

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
    nm.MeshMaterial.EmissiveStrength = 6.0f;
    neon.GetComponent<TransformComponent>().Translation = { -2.6f, 0.8f, -1.5f };

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

    ScriptEngine::GetRegistry().Register<DemoPlayerMove>("DemoPlayerMove");
    ScriptEngine::GetRegistry().Register<DemoEnemyScript>("DemoEnemyScript");

    auto& scene = m_ActiveScene;

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

    Entity ground = scene->CreateEntity("Chão");
    auto& gm = ground.AddComponent<MeshRendererComponent>();
    gm.MeshSource = "builtin:plane";
    gm.MeshAsset = Mesh::FromSource(gm.MeshSource);
    gm.MeshMaterial.Albedo = { 0.20f, 0.22f, 0.26f };
    gm.MeshMaterial.Roughness = 0.9f;
    ground.GetComponent<TransformComponent>().Scale = { 30.0f, 1.0f, 30.0f };

    Entity navGrid = scene->CreateEntity("NavGrade");
    auto& ng = navGrid.AddComponent<NavGridComponent>();
    ng.Origin = { -25.0f, 0.0f, -25.0f };
    ng.Width = 50;
    ng.Depth = 50;
    ng.CellSize = 1.0f;

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
        ob.AddComponent<NavObstacleComponent>();
    }

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

        kizuri::net::Event ev;
        while (kizuri::Network::PollEvent(ev)) {
            if (ev.Type == kizuri::net::EventType::Connect)
                KZ_CORE_INFO("Rede: jogador {0} conectou!", ev.Peer);
            else if (ev.Type == kizuri::net::EventType::Data && ev.Data.size() >= kizuri::net::kNetTransformSize) {
                kizuri::net::NetTransform t = kizuri::net::ReadNetTransform(ev.Data.data());
                if (!m_Host) {
                    tc.Translation = { t.X, t.Y, t.Z };
                    tc.Rotation.y = t.Yaw;
                }
            }
        }

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

    Entity player = scene->CreateEntity("Jogador");
    auto& pm = player.AddComponent<MeshRendererComponent>();
    pm.MeshSource = "builtin:cube";
    pm.MeshAsset = Mesh::FromSource(pm.MeshSource);
    pm.MeshMaterial.Albedo = { 0.25f, 0.6f, 0.9f };
    pm.MeshMaterial.Roughness = 0.4f;
    player.GetComponent<TransformComponent>().Translation = { 0.0f, 0.5f, 6.0f };
    player.AddComponent<NativeScriptComponent>().BindByName("DemoPlayerMove");

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

void EditorLayer::CreateDemoScene2D() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<Scene>("Demonstração 2D");
    m_ScenePath.clear();
    m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode2D;

    m_Editor2DZoom = 10.0f;
    m_Editor2DCamPos = { 0.0f, 0.0f };
    m_Editor2DFirstMouseLook = true;

    Entity camera = m_ActiveScene->CreateEntity("Câmera 2D");
    auto& cc = camera.AddComponent<CameraComponent>();
    cc.Type = CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true;
    cc.OrthoSize = 10.0f;

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
    btext.FontSize = 14.0f;
    btext.Color = { 1.0f, 1.0f, 1.0f, 1.0f };
    button.SetParent(canvas);

    KZ_CORE_INFO("Cena de demonstração 2D criada.");
}


// ===== Helper: textura procedural =====

static Ref<kizuri::Texture2D> MakeBallSpriteSheet(int frames, int framePx) {
    int W = frames * framePx, H = framePx;
    std::vector<uint8_t> px(W * H * 4);
    float r = (float)(framePx/2 - 4);
    for (int f = 0; f < frames; ++f) {
        float squash = (f < frames/2) ? 1.0f + 0.2f*((float)f/(frames/2)) : 1.0f - 0.2f*((float)(f-frames/2)/(frames/2));
        float cy = (float)framePx*0.5f + 2.0f*(1.0f-squash);
        for (int py = 0; py < framePx; ++py) for (int pi = 0; pi < framePx; ++pi) {
            float dx = (float)pi - (float)framePx*0.5f;
            float dy = (float)py  - cy;
            float rx = r * squash;
            if (dx*dx/(rx*rx) + (squash>1.0f?dy*dy/(r/squash*r/squash):dy*dy/(r*squash*squash)) <= 1.0f) {
                int i = (py*W + f*framePx + pi)*4;
                px[i] = 220; px[i+1] = 60; px[i+2] = 30; px[i+3] = 255;
            }
        }
    }
    auto tex = kizuri::Texture2D::Create(W, H);
    tex->SetData(px.data(), (uint32_t)px.size());
    return tex;
}

static Ref<kizuri::Texture2D> MakeTileAtlas4x4() {
    constexpr int T = 4, S = 16;
    constexpr int W = T*S, H = T*S;
    std::vector<uint8_t> px(W*H*4);
    uint32_t c[] = {0x3a8a4c, 0x4e7c3a, 0x8b6f47, 0x6a7a5e,
                    0x2d7a3c, 0xb87333, 0xcda07d, 0x7fb38a,
                    0xd4a574, 0xf0c882, 0xe8d5b0, 0xf8f4ee,
                    0x3b6e2c, 0x228b22, 0xa0785a, 0x6b5b4f};
    for (int cy = 0; cy < T; ++cy) for (int cx = 0; cx < T; ++cx)
        for (int py = 0; py < S; ++py) for (int pi = 0; pi < S; ++pi) {
            int i = ((cy*S+py)*W + (cx*S+pi))*4;
            uint32_t v = c[cy*T+cx];
            px[i]=v>>16; px[i+1]=(v>>8)&0xff; px[i+2]=v&0xff; px[i+3]=255;
        }
    auto tex = kizuri::Texture2D::Create(W,H);
    tex->SetData(px.data(),(uint32_t)px.size());
    return tex;
}
static Ref<kizuri::Texture2D> MakeDecalTexture() {
    constexpr int S = 64;
    std::vector<uint8_t> px(S*S*4);
    float cx = (float)S * 0.5f, cy = (float)S * 0.5f, r = (float)(S/2 - 2);
    for (int y = 0; y < S; ++y) for (int x = 0; x < S; ++x) {
        float dx = (float)x - cx, dy = (float)y - cy;
        int i = (y*S + x) * 4;
        float d = sqrtf(dx*dx + dy*dy);
        float a = (d < r) ? 1.0f - 0.3f * (d / r) : 0.0f;
        px[i] = 255; px[i+1] = 50; px[i+2] = 50; px[i+3] = (uint8_t)(a * 255);
    }
    auto tex = kizuri::Texture2D::Create(S, S);
    tex->SetData(px.data(), (uint32_t)px.size());
    return tex;
}


// ===== DemoTilemap (2D) =====
void EditorLayer::CreateDemoTilemap() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Tilemap");
    m_ScenePath.clear(); m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode2D;
    m_Editor2DZoom = 20.0f; m_Editor2DCamPos = {0,0}; m_Editor2DFirstMouseLook = true;

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true; cc.OrthoSize = 16.0f;

    auto atlas = MakeTileAtlas4x4();
    Entity tileEntity = m_ActiveScene->CreateEntity("Tilemap");
    auto& tm = tileEntity.AddComponent<kizuri::TilemapComponent>();
    tm.AtlasTexture = atlas;
    tm.AtlasColumns = 4; tm.AtlasRows = 4;
    tm.TileSize = {1.0f,1.0f};
    tm.MapWidth = 24; tm.MapHeight = 14;
    tm.Tiles.resize(24*14, 0);
    for (int x = 0; x < 24; ++x) { tm.Tiles[x + 13*24] = 1; tm.Tiles[x + 12*24] = 2; }
    for (int i = 0; i < 4; ++i) { int bx = 4+i*5; for (int x=bx;x<bx+3&&x<24;++x) tm.Tiles[x+8*24]=3; }
    tm.SolidTileValues = {1, 2, 3};
    tileEntity.GetComponent<kizuri::TransformComponent>().Translation = {-12.0, -7.0, 0.0};

    Entity player = m_ActiveScene->CreateEntity("Player");
    auto& sr = player.AddComponent<kizuri::SpriteRendererComponent>();
    sr.Color = {0.25f,0.6f,0.9f,1.0f}; sr.SortingLayer = 5;
    player.AddComponent<kizuri::Rigidbody2DComponent>();
    player.GetComponent<kizuri::Rigidbody2DComponent>().Type = kizuri::Rigidbody2DComponent::BodyType::Dynamic;
    player.AddComponent<kizuri::BoxCollider2DComponent>().Size = {0.45f,0.9f};
    player.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 10.0f, 0.0f};

    Entity text = m_ActiveScene->CreateEntity("Hint");
    auto& tc = text.AddComponent<kizuri::TextComponent>();
    tc.Text = "Tilemap + Box2D Physics"; tc.FontSize = 28; tc.SortingLayer = 10;
    text.GetComponent<kizuri::TransformComponent>().Translation = {-6.5, 13.5, 0.0};
}

// ===== DemoSpriteAnim (2D) =====
void EditorLayer::CreateDemoSpriteAnim() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Sprite Animation");
    m_ScenePath.clear(); m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode2D;
    m_Editor2DZoom = 6.0f; m_Editor2DCamPos = {0,0}; m_Editor2DFirstMouseLook = true;

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true; cc.OrthoSize = 4.0f;

    auto sheet = MakeBallSpriteSheet(8,32);
    const char* labels[] = {"Loop 6 FPS","Loop 12 FPS","One-shot 24 FPS"};
    float fps[] = {6.0, 12.0, 24.0};
    bool loops[] = {true,true,false};
    for (int i = 0; i < 3; ++i) {
        Entity e = m_ActiveScene->CreateEntity(labels[i]);
        e.AddComponent<kizuri::TransformComponent>(); // ensure transform exists
        auto& an = e.AddComponent<kizuri::SpriteAnimationComponent>();
        an.SheetTexture = sheet; an.FramesPerRow = 8; an.TotalFrames = 8;
        an.FPS = fps[i]; an.Loop = loops[i]; an.SortingLayer = 5;
        e.GetComponent<kizuri::TransformComponent>().Translation = {(float)(i-1)*3.0f, 1.5f, 0.0f};

        Entity lbl = m_ActiveScene->CreateEntity("Label " + std::to_string(i));
        auto& tc = lbl.AddComponent<kizuri::TextComponent>();
        tc.Text = labels[i]; tc.FontSize = 22; tc.SortingLayer = 10;
        lbl.GetComponent<kizuri::TransformComponent>().Translation = {(float)(i-1)*3.0f - 1.2f, 0.2f, 0.0f};
    }
}

// ===== DemoFisica3D (3D) =====
void EditorLayer::CreateDemoFisica3D() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Fisica 3D");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cc.PerspectiveFOV = 55.0f;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 8.0f, 14.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-20.0f),0,0};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    auto& lc = sun.AddComponent<kizuri::LightComponent>();
    lc.Type = kizuri::LightType::Directional; lc.Color = {1.0, 1.0, 1.0}; lc.Intensity = 2.0f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(45.0f), glm::radians(30.0f), 0.0f};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& fm = floor.AddComponent<kizuri::MeshRendererComponent>();
    fm.MeshSource="builtin:plane"; fm.MeshAsset=kizuri::Mesh::FromSource(fm.MeshSource);
    fm.MeshMaterial.Albedo={0.3, 0.3, 0.3}; fm.MeshMaterial.Roughness=0.9f;
    floor.GetComponent<kizuri::TransformComponent>().Scale={20.0f, 1.0f, 20.0f};
    floor.AddComponent<kizuri::Rigidbody3DComponent>().Type=kizuri::Rigidbody3DComponent::BodyType::Static;
    floor.AddComponent<kizuri::BoxCollider3DComponent>().HalfExtents={10.0f, 0.1f, 10.0f};

    const char* shapes[] = {"builtin:cube","builtin:cube","builtin:cube","builtin:sphere","builtin:cube"};
    for (int i = 0; i < 5; ++i) {
        Entity e = m_ActiveScene->CreateEntity("Fallen " + std::to_string(i));
        auto& m = e.AddComponent<kizuri::MeshRendererComponent>();
        m.MeshSource = shapes[i]; m.MeshAsset = kizuri::Mesh::FromSource(m.MeshSource);
        m.MeshMaterial.Albedo = {0.2f+i*0.15f,0.4f,0.8f-i*0.1f};
        m.MeshMaterial.Roughness = 0.4f;
        auto& t = e.GetComponent<kizuri::TransformComponent>();
        t.Translation = {(float)i*1.2f - 2.4f, 8.0f + i*2.5f, 0};
        t.Scale = {0.8, 0.8, 0.8};
        e.AddComponent<kizuri::Rigidbody3DComponent>().Type = kizuri::Rigidbody3DComponent::BodyType::Dynamic;
        if (shapes[i]==std::string("builtin:sphere"))
            e.AddComponent<kizuri::SphereCollider3DComponent>().Radius=0.4f;
        else e.AddComponent<kizuri::BoxCollider3DComponent>().HalfExtents={0.4, 0.4, 0.4};
    }
}

// ===== DemoLuzes (3D) =====
void EditorLayer::CreateDemoLuzes() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Luzes");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cc.PerspectiveFOV = 55.0f;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 5.0f, 12.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-15.0f),0,0};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& fm = floor.AddComponent<kizuri::MeshRendererComponent>();
    fm.MeshSource="builtin:plane"; fm.MeshAsset=kizuri::Mesh::FromSource(fm.MeshSource);
    fm.MeshMaterial.Albedo={0.05, 0.05, 0.06}; fm.MeshMaterial.Roughness=0.95f;
    floor.GetComponent<kizuri::TransformComponent>().Scale={30.0f, 1.0f, 30.0f};

    auto makeSphere=[&](const char* name, glm::vec3 pos, glm::vec3 color, glm::vec3 emissive, float intensity){
        Entity e = m_ActiveScene->CreateEntity(name);
        auto& m = e.AddComponent<kizuri::MeshRendererComponent>();
        m.MeshSource="builtin:sphere"; m.MeshAsset=kizuri::Mesh::FromSource(m.MeshSource);
        m.MeshMaterial.Albedo = color; m.MeshMaterial.Emissive = emissive;
        m.MeshMaterial.EmissiveStrength = intensity; m.MeshMaterial.Roughness = 0.3f;
        e.GetComponent<kizuri::TransformComponent>().Translation = pos;
        e.GetComponent<kizuri::TransformComponent>().Scale = {0.6, 0.6, 0.6};
    };
    makeSphere("Red Light",{-5.0f, 0.2, 0.0f},{1.0f, 0.0f, 0.0f},{1.0f, 0.0f, 0.0f},2);
    makeSphere("Green Light",{0.0f, 0.2, -4.0f},{0.0f, 1.0f, 0.0f},{0.0f, 1.0f, 0.0f},2);
    makeSphere("Blue Light",{5.0f, 0.2, 0.0f},{0.0f, 0.3, 1.0f},{0.0f, 0.3, 1.0f},2);

    auto makeLight=[&](const char* name, glm::vec3 pos, glm::vec3 dir, glm::vec3 color, float intensity, kizuri::LightType type, float range, bool shadow){
        Entity e = m_ActiveScene->CreateEntity(name);
        auto& lc = e.AddComponent<kizuri::LightComponent>();
        lc.Type=type; lc.Color=color; lc.Intensity=intensity; lc.Range=range; lc.CastsShadow=shadow;
        e.GetComponent<kizuri::TransformComponent>().Translation = pos;
        e.GetComponent<kizuri::TransformComponent>().Rotation = dir;
    };
    makeLight("Spot Left",{-4.0f, 6.0f, -2.0f},{glm::radians(60.0f),0,0},{1.0f, 1.0f, 0.8},4,kizuri::LightType::Spot,15,true);
    makeLight("Point Fill",{3.0f, 4.0f, -6.0f},{0.0f, 0.0f, 0.0f},{0.2, 0.3, 0.8},1.5f,kizuri::LightType::Point,10,false);
    makeLight("Spot Right",{4.0f, 6.0f, -2.0f},{glm::radians(60.0f),glm::radians(180.0f),0},{1.0f, 0.9, 0.6},3,kizuri::LightType::Spot,12,false);

    for (int i = 0; i < 3; ++i) {
        Entity e = m_ActiveScene->CreateEntity("Column " + std::to_string(i));
        auto& m = e.AddComponent<kizuri::MeshRendererComponent>();
        m.MeshSource="builtin:cylinder"; m.MeshAsset=kizuri::Mesh::FromSource(m.MeshSource);
        m.MeshMaterial.Albedo={0.15, 0.15, 0.18}; m.MeshMaterial.Roughness=0.6f;
        e.GetComponent<kizuri::TransformComponent>().Translation={(float)(i-1)*4.0f,0.75f,-2.0f};
        e.GetComponent<kizuri::TransformComponent>().Scale={0.4, 1.5, 0.4};
    }
}

// ===== DemoParticulas (3D) =====
void EditorLayer::CreateDemoParticulas() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Particulas");
    m_ScenePath.clear(); m_SelectedEntity = {};

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 3.0f, 8.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-10.0f),0,0};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 1.2f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f), glm::radians(30.0f), 0.0f};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& fm = floor.AddComponent<kizuri::MeshRendererComponent>();
    fm.MeshSource="builtin:plane"; fm.MeshAsset=kizuri::Mesh::FromSource(fm.MeshSource);
    fm.MeshMaterial.Albedo={0.15, 0.15, 0.18};
    floor.GetComponent<kizuri::TransformComponent>().Scale={20.0f, 1.0f, 20.0f};

    auto makeEmitter=[&](const char* name, glm::vec3 pos, glm::vec3 velMin, glm::vec3 velMax,
                          glm::vec3 grav, glm::vec4 sc, glm::vec4 ec, float rate, float ltMin, float ltMax,
                          float szMin, float szMax, bool add, glm::vec3 color)
    {
        Entity e = m_ActiveScene->CreateEntity(name);
        auto& m = e.AddComponent<kizuri::MeshRendererComponent>();
        m.MeshSource="builtin:cube"; m.MeshAsset=kizuri::Mesh::FromSource(m.MeshSource);
        m.MeshMaterial.Albedo = color; m.MeshMaterial.Roughness=0.5f;
        m.MeshMaterial.Emissive = color; m.MeshMaterial.EmissiveStrength = 0.0f;
        auto& ps = e.AddComponent<kizuri::ParticleSystemComponent>();
        ps.Playing=true; ps.Additive=add; ps.EmissionRate=rate;
        ps.LifetimeMin=ltMin; ps.LifetimeMax=ltMax;
        ps.VelocityMin=velMin; ps.VelocityMax=velMax;
        ps.Gravity=grav; ps.StartColor=sc; ps.EndColor=ec;
        ps.StartSize=szMin; ps.EndSize=szMax; ps.MaxParticles=400;
        e.GetComponent<kizuri::TransformComponent>().Translation=pos;
    };

    makeEmitter("Fire",{-4.0f, 1.5, 0.0f},{-0.1, 2.0, -0.1},{0.1, 4.0, 0.1},{0.0f, 1.5, 0.0f},
        {1,0.7f,0.1f,1},{1,0.1f,0.0f,0.0f},40,0.5f,1.2f,0.08f,0.25f,true,{1.0f, 0.8, 0.3});
    makeEmitter("Smoke",{0.0f, 1.5, 0.0f},{-0.2, 0.8, -0.2},{0.2, 2.0, 0.2},{0.0f, 0.3, 0.0f},
        {0.6f,0.6f,0.6f,0.6f},{0.4f,0.4f,0.4f,0.0f},20,1.0f,2.0f,0.3f,0.8f,false,{0.5, 0.5, 0.5});
    makeEmitter("Sparks",{4.0f, 0.5, 0.0f},{-1.0, 5.0, -1.0},{1.0, 8.0, 1.0},{0.0f, -9.8, 0.0f},
        {1,1,0.3f,1},{1,0.6f,0.1f,0.0f},60,0.3f,0.8f,0.04f,0.1f,true,{1.0f, 0.9, 0.5});
}

// ===== DemoTerreno (3D) =====
void EditorLayer::CreateDemoTerreno() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Terreno");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 12.0f, 18.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-30.0f),0,0};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 2.0f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f), glm::radians(30.0f), 0.0f};

    Entity terrain = m_ActiveScene->CreateEntity("Terrain");
    auto& tc = terrain.AddComponent<kizuri::TerrainComponent>();
    tc.Segments = 64; tc.Size = 80.0f; tc.HeightScale = 8.0f; tc.Seed = 42;
    tc.Regenerate();
    terrain.AddComponent<kizuri::MeshRendererComponent>().MeshSource = "builtin:plane";
    terrain.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.Albedo = {0.28, 0.45, 0.22};
    terrain.GetComponent<kizuri::MeshRendererComponent>().MeshMaterial.Roughness = 0.95f;
    terrain.GetComponent<kizuri::MeshRendererComponent>().MeshAsset = terrain.GetComponent<kizuri::TerrainComponent>().GeneratedMesh;

    Entity water = m_ActiveScene->CreateEntity("Water");
    auto& wm = water.AddComponent<kizuri::MeshRendererComponent>();
    wm.MeshSource="builtin:plane"; wm.MeshAsset=kizuri::Mesh::FromSource(wm.MeshSource);
    wm.MeshMaterial.Albedo={0.05, 0.15, 0.35}; wm.MeshMaterial.Metallic=0.9f; wm.MeshMaterial.Roughness=0.05f;
    water.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 1.2, 0.0f};
    water.GetComponent<kizuri::TransformComponent>().Scale = {50.0f, 1.0f, 50.0f};
}

// ===== DemoTimeline (3D) =====
void EditorLayer::CreateDemoTimeline() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Timeline");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 5.0f, 12.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-20.0f),0,0};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 1.8f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f),0,0};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& fm = floor.AddComponent<kizuri::MeshRendererComponent>();
    fm.MeshSource="builtin:plane"; fm.MeshAsset=kizuri::Mesh::FromSource(fm.MeshSource);
    fm.MeshMaterial.Albedo={0.2, 0.2, 0.22};
    floor.GetComponent<kizuri::TransformComponent>().Scale={30.0f, 1.0f, 30.0f};

    Entity platform = m_ActiveScene->CreateEntity("Plataforma");
    auto& pm = platform.AddComponent<kizuri::MeshRendererComponent>();
    pm.MeshSource="builtin:cube"; pm.MeshAsset=kizuri::Mesh::FromSource(pm.MeshSource);
    pm.MeshMaterial.Albedo={0.3, 0.6, 0.9};
    platform.GetComponent<kizuri::TransformComponent>().Translation = {-5.0f, 2.0f, 0.0f};
    platform.GetComponent<kizuri::TransformComponent>().Scale = {3.0f, 0.4, 3.0f};
    auto& tpl = platform.AddComponent<kizuri::TimelineComponent>();
    tpl.Keyframes = {
        {-5.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, {5.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {5.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 90.0f}, {1.0f, 1.0f, 1.0f}}, {-5.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 360.0f}, {1.0f, 1.0f, 1.0f}},
    };
    tpl.Loop = true; tpl.Speed = 0.5f;

    Entity torus = m_ActiveScene->CreateEntity("Torus");
    auto& ttm = torus.AddComponent<kizuri::MeshRendererComponent>();
    ttm.MeshSource="builtin:torus"; ttm.MeshAsset=kizuri::Mesh::FromSource(ttm.MeshSource);
    ttm.MeshMaterial.Albedo={0.9, 0.3, 0.3}; ttm.MeshMaterial.Roughness=0.3f;
    torus.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 3.0f, 0.0f};
    auto& ttl = torus.AddComponent<kizuri::TimelineComponent>();
    ttl.Keyframes = {
        {0.0f, {0.0f, 3.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, {0.0f, {0.0f, 3.0f, 0.0f}, {0.0f, 360.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    };
    ttl.Loop = true; ttl.Speed = 1.0f;
}

// ===== DemoLOD (3D) =====
void EditorLayer::CreateDemoLOD() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo LOD");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 2.0f, 3.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-10.0f),0.0f,0.0f};
    auto& camTL = cam.AddComponent<kizuri::TimelineComponent>();
    camTL.Keyframes = {
        {0.0f, {0.0f,2.0f,3.0f}, {glm::radians(10.0f),0.0f,0.0f}, {1.0f,1.0f,1.0f}},
        {4.0f, {12.0f,2.0f,8.0f}, {glm::radians(5.0f),glm::radians(-30.0f),0.0f}, {1.0f,1.0f,1.0f}},
        {8.0f, {0.0f,2.0f,18.0f}, {glm::radians(15.0f),glm::radians(-60.0f),0.0f}, {1.0f,1.0f,1.0f}},
        {12.0f, {0.0f,2.0f,3.0f}, {glm::radians(10.0f),0.0f,0.0f}, {1.0f,1.0f,1.0f}},
    };
    camTL.Loop = true; camTL.Speed = 0.5f;

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 1.5f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(45.0f), glm::radians(30.0f), 0.0f};

    for (int i = 0; i < 5; ++i) {
        Entity e = m_ActiveScene->CreateEntity("Sphere LOD " + std::to_string(i));
        auto& m = e.AddComponent<kizuri::MeshRendererComponent>();
        m.MeshSource="builtin:sphere"; m.MeshAsset=kizuri::Mesh::FromSource(m.MeshSource);
        m.MeshMaterial.Albedo = {0.3f+(float)i*0.12f,0.3f,0.9f-(float)i*0.12f};
        m.MeshMaterial.Roughness = 0.4f;
        e.GetComponent<kizuri::TransformComponent>().Translation = {(float)i*4.0f - 8.0f, 1.0f, -i*2.0f};
        auto& lod = e.AddComponent<kizuri::LODComponent>();
        for (int l = 0; l < 3; ++l) {
            auto& lv = lod.Levels.emplace_back();
            lv.MeshSource = "builtin:sphere";
            lv.Distance = 15.0f * (l+1);
            lv.MeshAsset = kizuri::Mesh::CreateLODMesh("builtin:sphere", l);
        }
    }

    Entity label = m_ActiveScene->CreateEntity("Label");
    auto& tc = label.AddComponent<kizuri::TextComponent>();
    tc.Text = "LOD: afaste para ver menos triangulos"; tc.FontSize = 26; tc.SortingLayer = 10;
    label.GetComponent<kizuri::TransformComponent>().Translation = {-8.0f, 6.0f, 0.0f};
}

// ===== DemoEfeitos (3D) =====
void EditorLayer::CreateDemoEfeitos() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Efeitos");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 4.0f, 10.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-15.0f),0,0};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 1.8f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f),0,0};

    Entity wall = m_ActiveScene->CreateEntity("Parede");
    auto& wm = wall.AddComponent<kizuri::MeshRendererComponent>();
    wm.MeshSource="builtin:cube"; wm.MeshAsset=kizuri::Mesh::FromSource(wm.MeshSource);
    wm.MeshMaterial.Albedo={0.4, 0.35, 0.3};
    wall.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 1.5, -2.0f};
    wall.GetComponent<kizuri::TransformComponent>().Scale = {12.0f, 3.0f, 0.3};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& ff = floor.AddComponent<kizuri::MeshRendererComponent>();
    ff.MeshSource="builtin:plane"; ff.MeshAsset=kizuri::Mesh::FromSource(ff.MeshSource);
    ff.MeshMaterial.Albedo={0.3, 0.3, 0.32};
    floor.GetComponent<kizuri::TransformComponent>().Scale = {20.0f, 1.0f, 20.0f};

    auto decalTex = MakeDecalTexture();
    Entity decal = m_ActiveScene->CreateEntity("Decal");
    decal.AddComponent<kizuri::DecalComponent>();
    decal.GetComponent<kizuri::DecalComponent>().Texture = decalTex;
    decal.GetComponent<kizuri::DecalComponent>().Color = {1,0,0,0.7f};
    decal.GetComponent<kizuri::DecalComponent>().SortingLayer = -1;
    decal.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 1.55, -1.7};
    decal.GetComponent<kizuri::TransformComponent>().Scale = {2.0f, 2.0f, 0.1};

    Entity foliage = m_ActiveScene->CreateEntity("Foliage");
    auto& fc = foliage.AddComponent<kizuri::FoliageComponent>();
    fc.MeshSource="builtin:cone"; fc.AreaSize={10,10}; fc.Count=60;
    fc.ScaleMin=0.5f; fc.ScaleMax=1.5f; fc.HeightScale=3.0f;
    fc.MeshAsset=kizuri::Mesh::FromSource(fc.MeshSource);
    fc.Color={0.15f,0.5f,0.2f,1.0f}; fc.Seed=7;
    foliage.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 0.0f, -5.0f};

    Entity label = m_ActiveScene->CreateEntity("Label");
    auto& tc = label.AddComponent<kizuri::TextComponent>();
    tc.Text = "Decal vermelho + foliage instanciada (60 arvores)"; tc.FontSize = 24; tc.SortingLayer = 10;
    label.GetComponent<kizuri::TransformComponent>().Translation = {-8.0f, 5.0f, 0.0f};
}

// ===== Demo2_5D (hybrid 3D + 2D + HUD) =====
void EditorLayer::CreateDemo25D() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo 2.5D");
    m_ScenePath.clear(); m_SelectedEntity = {};
    Renderer3D::SetEnvironmentHDRIPath("kzres://skies/sky_gradient.hdr");

    Entity cam = m_ActiveScene->CreateEntity("Camera 3D");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 4.0f, 10.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-15.0f),0,0};

    Entity ortho = m_ActiveScene->CreateEntity("Camera 2D (HUD)");
    auto& oc = ortho.AddComponent<kizuri::CameraComponent>();
    oc.Type = kizuri::CameraComponent::ProjectionType::Orthographic2D;
    oc.OrthoSize = 5.0f;

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    sun.AddComponent<kizuri::LightComponent>().Type = kizuri::LightType::Directional;
    sun.GetComponent<kizuri::LightComponent>().Intensity = 1.5f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f),0,0};

    Entity floor = m_ActiveScene->CreateEntity("Floor");
    auto& fm = floor.AddComponent<kizuri::MeshRendererComponent>();
    fm.MeshSource="builtin:plane"; fm.MeshAsset=kizuri::Mesh::FromSource(fm.MeshSource);
    fm.MeshMaterial.Albedo={0.18, 0.18, 0.2};
    floor.GetComponent<kizuri::TransformComponent>().Scale = {20.0f, 1.0f, 20.0f};

    Entity pillar1 = m_ActiveScene->CreateEntity("Pillar L");
    auto& p1 = pillar1.AddComponent<kizuri::MeshRendererComponent>();
    p1.MeshSource="builtin:cylinder"; p1.MeshAsset=kizuri::Mesh::FromSource(p1.MeshSource);
    p1.MeshMaterial.Albedo={0.5, 0.4, 0.3};
    pillar1.GetComponent<kizuri::TransformComponent>().Translation = {-4.0f, 1.5, 0.0f};
    pillar1.GetComponent<kizuri::TransformComponent>().Scale = {0.5, 3.0f, 0.5};

    Entity pillar2 = m_ActiveScene->CreateEntity("Pillar R");
    auto& p2 = pillar2.AddComponent<kizuri::MeshRendererComponent>();
    p2.MeshSource="builtin:cylinder"; p2.MeshAsset=kizuri::Mesh::FromSource(p2.MeshSource);
    p2.MeshMaterial.Albedo={0.5, 0.4, 0.3};
    pillar2.GetComponent<kizuri::TransformComponent>().Translation = {4.0f, 1.5, 0.0f};
    pillar2.GetComponent<kizuri::TransformComponent>().Scale = {0.5, 3.0f, 0.5};

    Entity wall = m_ActiveScene->CreateEntity("Back Wall");
    auto& bw = wall.AddComponent<kizuri::MeshRendererComponent>();
    bw.MeshSource="builtin:cube"; bw.MeshAsset=kizuri::Mesh::FromSource(bw.MeshSource);
    bw.MeshMaterial.Albedo={0.2, 0.18, 0.22};
    wall.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 1.5, -3.0f};
    wall.GetComponent<kizuri::TransformComponent>().Scale = {14.0f, 3.0f, 0.2};

    Entity sprite = m_ActiveScene->CreateEntity("2D Player");
    auto& sr = sprite.AddComponent<kizuri::SpriteRendererComponent>();
    sr.Color = {0.2f,0.7f,1.0f,1.0f}; sr.SortingLayer = 1;
    sprite.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 0.0f, 0.0f};

    Entity coin = m_ActiveScene->CreateEntity("2D Coin");
    auto& cr = coin.AddComponent<kizuri::CircleRendererComponent>();
    cr.Color = {1,0.85f,0.2f,1}; cr.Thickness = 1.0f; cr.SortingLayer = 2;
    coin.GetComponent<kizuri::TransformComponent>().Translation = {2.0f, 0.8, 0.0f};

    Entity hud = m_ActiveScene->CreateEntity("HUD");
    auto& tc = hud.AddComponent<kizuri::TextComponent>();
    tc.Text = "2.5D Demo — 3D world + 2D sprites + HUD"; tc.FontSize = 28; tc.SortingLayer = 100;
    hud.GetComponent<kizuri::TransformComponent>().Translation = {-4.0f, 3.0f, 0.0f};

    Entity uicanvas = m_ActiveScene->CreateEntity("HUD Canvas");
    uicanvas.AddComponent<kizuri::UICanvasComponent>().OrthoSize = 5.0f;
    Entity uirect = m_ActiveScene->CreateEntity("HUD Box");
    uirect.AddComponent<kizuri::UIRectComponent>().Position = {3.5f, -3.5f};
    uirect.GetComponent<kizuri::UIRectComponent>().Size = {2.5f, 1.2f};
    uirect.GetComponent<kizuri::UIRectComponent>().Color = {0.2f,0.5f,0.8f,0.85f};
    uirect.SetParent(uicanvas);
}

// ===== DemoCameraFollow2D (2D platformer-ish) =====
void EditorLayer::CreateDemoCameraFollow2D() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Camera Follow 2D");
    m_ScenePath.clear(); m_SelectedEntity = {};
    m_ViewportMode = ViewportMode::Mode2D;
    m_Editor2DZoom = 12.0f; m_Editor2DCamPos = {0,0}; m_Editor2DFirstMouseLook = true;

    Entity cam = m_ActiveScene->CreateEntity("Camera 2D");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Orthographic2D;
    cc.Primary = true; cc.OrthoSize = 8.0f;
    cam.AddComponent<kizuri::CameraFollowComponent>().TargetName = "Alvo";
    cam.GetComponent<kizuri::CameraFollowComponent>().Offset = {0.0f, 2.0f, -1.0f};

    Entity ground = m_ActiveScene->CreateEntity("Chao");
    auto& gr = ground.AddComponent<kizuri::SpriteRendererComponent>();
    gr.Color = {0.28f,0.32f,0.38f,1.0f};
    ground.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, -3.5, 0.0f};
    ground.GetComponent<kizuri::TransformComponent>().Scale = {30.0f, 0.5, 1.0f};

    Entity alvo = m_ActiveScene->CreateEntity("Alvo");
    auto& ar = alvo.AddComponent<kizuri::SpriteRendererComponent>();
    ar.Color = {0.25f,0.6f,0.9f,1.0f}; ar.SortingLayer = 5;
    alvo.GetComponent<kizuri::TransformComponent>().Translation = {-8.0f, 0.0f, 0.0f};

    auto& tl = alvo.AddComponent<kizuri::TimelineComponent>();
    tl.Keyframes = {
        {-8.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, {8.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
        {8.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}}, {-8.0f, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f}, {1.0f, 1.0f, 1.0f}},
    };
    tl.Loop = true; tl.Speed = 2.0f;

    for (int i = 0; i < 10; ++i) {
        Entity coin = m_ActiveScene->CreateEntity("Coin " + std::to_string(i));
        auto& cr = coin.AddComponent<kizuri::CircleRendererComponent>();
        cr.Color = {1,0.85f,0.2f,1}; cr.Thickness = 1.0f;
        coin.GetComponent<kizuri::TransformComponent>().Translation = {(float)i*3-12,0,0};
    }
}

// ===== DemoChunk (Chunked World) =====
void EditorLayer::CreateDemoChunk() {
    if (m_SceneState != SceneState::Edit) return;
    m_ActiveScene = CreateRef<kizuri::Scene>("Demo Chunked World");
    m_ScenePath.clear(); m_SelectedEntity = {};

    Entity cam = m_ActiveScene->CreateEntity("Camera");
    auto& cc = cam.AddComponent<kizuri::CameraComponent>();
    cc.Type = kizuri::CameraComponent::ProjectionType::Perspective3D;
    cc.PerspectiveFOV = 60.0f;
    cam.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 50.0f, 50.0f};
    cam.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(-45.0f), 0.0f, 0.0f};

    Entity sun = m_ActiveScene->CreateEntity("Sun");
    auto& lc = sun.AddComponent<kizuri::LightComponent>();
    lc.Type = kizuri::LightType::Directional;
    lc.Color = {1.0f, 1.0f, 0.95f};
    lc.Intensity = 2.0f;
    sun.GetComponent<kizuri::TransformComponent>().Rotation = {glm::radians(50.0f), glm::radians(30.0f), 0.0f};

    Entity player = m_ActiveScene->CreateEntity("Jogador");
    auto& pm = player.AddComponent<kizuri::MeshRendererComponent>();
    pm.MeshSource = "builtin:cube";
    pm.MeshAsset = kizuri::Mesh::FromSource(pm.MeshSource);
    pm.MeshMaterial.Albedo = {0.2f, 0.5f, 0.9f};
    pm.MeshMaterial.Roughness = 0.3f;
    player.GetComponent<kizuri::TransformComponent>().Translation = {0.0f, 1.5f, 0.0f};
    player.GetComponent<kizuri::TransformComponent>().Scale = {1.0f, 2.0f, 1.0f};

    Entity world = m_ActiveScene->CreateEntity("Mundo Chunked");
    world.AddComponent<kizuri::ChunkWorldComponent>();
    world.GetComponent<kizuri::ChunkWorldComponent>().TargetTag = "Jogador";
    world.GetComponent<kizuri::ChunkWorldComponent>().LoadRadius = 2;
    world.GetComponent<kizuri::ChunkWorldComponent>().ChunkSize = 32.0f;

    uint32_t colors[] = {0x3a8a4c, 0xb87333, 0x228b22, 0x8b6f47, 0x4e7c3a, 0x556b2f, 0xcd853f, 0x708090};
    for (int cz = -2; cz <= 2; ++cz) for (int cx = -2; cx <= 2; ++cx) {
        int ci = abs(cx) + abs(cz);
        Entity chunk = m_ActiveScene->CreateEntity("Chunk " + std::to_string(cx) + "_" + std::to_string(cz));
        auto& cm = chunk.AddComponent<kizuri::MeshRendererComponent>();
        cm.MeshSource = "builtin:plane";
        cm.MeshAsset = kizuri::Mesh::FromSource(cm.MeshSource);
        cm.MeshMaterial.Albedo = {
            ((colors[ci % 8] >> 16) & 0xff) / 255.0f,
            ((colors[ci % 8] >> 8) & 0xff) / 255.0f,
            (colors[ci % 8] & 0xff) / 255.0f };
        cm.MeshMaterial.Roughness = 0.9f;
        chunk.GetComponent<kizuri::TransformComponent>().Translation = {(float)cx * 32.0f, 0.0f, (float)cz * 32.0f};
        chunk.GetComponent<kizuri::TransformComponent>().Scale = {32.0f, 1.0f, 32.0f};
        chunk.AddComponent<kizuri::ChunkEntityComponent>().ChunkX = cx;
        chunk.GetComponent<kizuri::ChunkEntityComponent>().ChunkZ = cz;
    }

    Entity label = m_ActiveScene->CreateEntity("Label");
    auto& tc = label.AddComponent<kizuri::TextComponent>();
    tc.Text = "Chunked World: chunks no XZ plane, carregam/descarregam por distância";
    tc.FontSize = 24;
    tc.SortingLayer = 10;
    label.GetComponent<kizuri::TransformComponent>().Translation = {-20.0f, 25.0f, 0.0f};
}

