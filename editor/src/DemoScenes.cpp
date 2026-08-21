#include "EditorLayer.hpp"
#include <kizuri/ecs/Components.hpp>
#include <kizuri/ecs/Entity.hpp>
#include <kizuri/project/Project.hpp>
#include <kizuri/net/NetworkFacade.hpp>
#include <kizuri/renderer/TextRenderer.hpp>
#include <kizuri/core/Log.hpp>
#include <glm/gtc/random.hpp>
#include <cstdio>

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
    } else if (HasEmbeddedResource("skies/sky_gradient.hdr")) {
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

