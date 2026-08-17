#pragma once
#include <Kizuri.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <string>
#include <filesystem>
#include <vector>
#include <unordered_map>
#include "AndroidExporter.hpp"
#include <unordered_set>
#include <memory>
#include <thread>
#include <atomic>

class EditorPanel; // base dos painéis (Profiler, Game View, Material Editor...)
struct EditorContext;

// EditorLayer implementa o esqueleto do Kizuri Editor: dockspace ImGui,
// painel de hierarquia de entidades e inspetor de componentes básico.
// É a base para evoluir num editor visual completo (viewport renderizado
// para uma textura de framebuffer, gizmos, etc).
class EditorLayer : public kizuri::Layer {
public:
    EditorLayer();
    ~EditorLayer() override; // definido no .cpp (m_Panels usa tipos incompletos)

    void OnAttach() override;
    void OnDetach() override;
    void OnUpdate(kizuri::Timestep ts) override;
    void OnImGuiRender() override;
    void OnEvent(kizuri::Event& e) override;

private:
    void DrawTitlebar();
    void DrawResizeBorders();
    void DrawDockspace();
    void DrawSceneHierarchy();
    void DrawInspector();
    // Desenha a grade de navegação + caminhos dos NavAgent no viewport
    // (debug do pilar IA). Visível no Play e em edição com o overlay ligado.
    void DrawNavDebug();
    // Abre o Content Browser na pasta do arquivo informado (botão
    // "Gerenciador" dos campos de arquivo do Inspetor).
    void RevealFileInContentBrowser(const std::string& filePath);
    // Posição de mundo sob o mouse do viewport (drop de arquivos do sistema).
    glm::vec3 MouseDropWorldPos() const;
    void DrawEntityNode(kizuri::Entity entity, kizuri::Entity& outEntityToDelete, bool editable);
    void DrawAddComponentButton();
    void DrawSceneFileModals();
    void DrawProjectModals();
    void DrawHub();
    void DrawLoadingScreen();
    void LoadRecentProjects();
    void SaveRecentProjects();
    void RememberProject(const kizuri::Ref<kizuri::Project>& project);
    void OnProjectOpened(const kizuri::Ref<kizuri::Project>& project);
    void OpenRecentProject(const std::string& path);
    void DrawGameModuleModal();
    void DrawConsole();
    void DrawContentBrowser();
    void UpdateEditorCamera(kizuri::Timestep ts);
    void UpdateEditor2DCamera(kizuri::Timestep ts);
    void DrawViewportToolbar();
    void DrawGizmo();
    void DrawCameraGizmo(); // pirâmide de frustum + seta de direção, só quando uma Camera tá selecionada
    void DrawLightGizmo();  // marcador de luz (ponto/spot/direcional) no viewport
    void DrawColliderGizmo(); // wireframe dos colisores 2D/3D da entidade selecionada
    void DrawAllColliders();  // overlay de física debug: todos os colliders da cena
    void Reparent(kizuri::Entity child, kizuri::Entity newParent);
    // Multi-seleção: consulta/resolve o conjunto de UUIDs selecionados.
    bool IsEntityMultiSelected(kizuri::Entity entity) const;
    std::vector<kizuri::Entity> GetMultiSelection() const;
    void ClearMultiSelection();

    // Configurações da engine (Arquivo > Configurações): seções em sidebar —
    // Gráficos, Geral e Editor. Não é mais só a aba padrão do ImGui.
    void DrawSettings();
    void DrawSettingsGraphics();
    void DrawSettingsGeneral();
    void DrawSettingsEditor();
    void LoadGraphicsSettingsFromDisk();
    void SaveGraphicsSettingsToDisk();

    // Miniatura real de arquivo de imagem pro Content Browser (cache por
    // caminho absoluto — recarrega só na primeira vez).
    kizuri::Ref<kizuri::Texture2D> GetThumbnail(const std::string& path);

    void NewScene();
    void SaveScene();
    void SaveSceneAs();
    void OpenScene(const std::string& path);

    // Cena de demonstração 3D: monta um showcase com o Content Pack (se
    // presente) — Fox esquelético animado, DamagedHelmet PBR, primitivas,
    // HDRI de céu, fog — pra mostrar a engine trabalhando junta.
    void CreateDemoScene3D();
    // Cena de demonstração 2D: sprites, física Box2D, círculos, texto e UI.
    void CreateDemoScene2D();
    // Cena de demonstração IA (v0.34): NavGrid + obstáculos + inimigos com
    // EnemyAIComponent (patrulha/persegue/ataca) — fuja com WASD.
    void CreateDemoSceneAI();
    void CreateDemoSceneGame();
    // Cena de demonstração REDE (v0.34): 2 instâncias (host + cliente) na
    // porta 26000 — WASD no host, o cliente vê o cubo se mover.
    void CreateDemoSceneNet();

    // Cria uma entidade a partir de um arquivo de asset (soltado do Content
    // Browser no viewport): .obj -> MeshRenderer, imagem -> SpriteRenderer.
    kizuri::Entity CreateEntityFromAsset(const std::string& path, const glm::vec3& worldPos);

    // Ao selecionar uma entidade, troca o modo do viewport pro que faz
    // sentido pra ela: 3D (MeshRenderer/Light/Câmera 3D) ou 2D
    // (Sprite/Círculo/Texto/Tilemap/Animação). Híbrido sem esses componentes
    // não mexe em nada. Só em modo edição.
    void AutoSwitchViewportMode();

    void CreateDefaultSceneContent();

    // Play = m_ActiveScene aponta pra uma CÓPIA (ver Scene::Copy) que roda OnUpdateRuntime de
    // verdade — física, scripts, partículas, áudio. m_EditorScene guarda a cena "mestra"
    // intocada; Stop descarta a cópia e restaura o ponteiro, sem precisar desfazer nada.
    enum class SceneState { Edit = 0, Play = 1 };
    SceneState m_SceneState = SceneState::Edit;
    kizuri::Ref<kizuri::Scene> m_EditorScene;
    void OnScenePlay();
    void OnSceneStop();

    kizuri::Ref<kizuri::Scene> m_ActiveScene;
    kizuri::Ref<kizuri::Framebuffer> m_Framebuffer;
    kizuri::Entity m_SelectedEntity;
    // Multi-seleção (Ctrl+clique na Hierarquia). m_SelectedEntity é a última
    // da seleção; as demais ficam aqui (excluir/duplicar operam no conjunto).
    std::unordered_set<kizuri::UUID> m_MultiSelection;

    // Carregamento ASSÍNCRONO de cena: projetos grandes não podem travar o
    // editor (janela congelada que não fecha). OpenScene() prepara o
    // SceneSerializer e cada OnUpdate processa um lote por tempo; quando
    // termina, a cena nova substitui m_ActiveScene. Enquanto carrega, um
    // overlay de progresso é desenhado e o loop de eventos continua vivo.
    bool m_SceneLoading = false;
    float m_PendingLoadProgress = 0.0f;
    std::string m_PendingScenePath;
    kizuri::Ref<kizuri::Scene> m_PendingScene;
    std::unique_ptr<kizuri::SceneSerializer> m_PendingLoader;

    // Compilação do C# em SEGUNDO PLANO no Play (estilo Unity): dotnet build
    // pode levar segundos e baixar pacotes na 1ª vez — síncrono travava o
    // editor. A thread compila; o OnUpdate consome o resultado e entra no
    // Play; um overlay de "Compilando..." aparece no meio.
    void StartPlayInternal();
    std::thread m_PlayBuildThread;
    std::atomic<bool> m_PlayBuildDone{ false };
    std::atomic<bool> m_PlayBuildCancelled{ false };
    bool m_PlayBuildActive = false;
    bool m_PlayBuildOk = false;
    std::string m_PlayBuildDll, m_PlayBuildError;

    // Caminho do .kzscene atualmente aberto. Vazio enquanto a cena nunca
    // tiver sido salva — nesse estado "Salvar Cena" se comporta como
    // "Salvar Como" (mostra o modal pra escolher o caminho).
    std::string m_ScenePath;

    // Estado dos modais de "Salvar Como" / "Abrir Cena". Não há um seletor
    // de arquivos nativo integrado ainda (ver docs/roadmap), então por ora
    // o caminho é digitado num campo de texto simples.
    bool m_RequestOpenSaveAsPopup = false;
    bool m_RequestOpenLoadPopup = false;
    char m_ScenePathBuffer[256] = "cena.kzscene";

    // Modais de "Novo Projeto" / "Abrir Projeto" — mesma lógica dos de
    // cena acima (sem seletor de arquivo nativo ainda, caminho digitado).
    // O editor funciona normalmente sem projeto nenhum aberto (a cena
    // solta de hoje continua existindo) — o projeto só passa a importar
    // quando o Content Browser e a resolução de asset por caminho relativo
    // entrarem (ver docs/NOTAS_INTERNAS.md).
    bool m_RequestOpenNewProjectPopup = false;
    bool m_RequestOpenLoadProjectPopup = false;
    char m_NewProjectDirBuffer[256] = "MeuJogo";
    char m_NewProjectNameBuffer[128] = "MeuJogo";
    int m_NewProjectModeIndex = 2; // 0 = 2D, 1 = 3D, 2 = Vazio
    char m_OpenProjectPathBuffer[256] = "";

    // Fluxo de entrada estilo hub (Unity): o editor abre numa tela de
    // seleção de projeto (recentes + novo/abrir) e só entra no editor
    // depois de um projeto aberto — com uma telinha de "Carregando
    // projeto" no meio. m_RecentProjects persiste em KizuriRecents.json.
    enum class EditorState { Hub = 0, Loading, Editor };
    EditorState m_EditorState = EditorState::Hub;
    struct RecentProject { std::string Name; std::string Path; std::string Mode; };
    std::vector<RecentProject> m_RecentProjects;
    float m_LoadingElapsed = 0.0f;
    std::string m_LoadingProjectName;
    static constexpr float kHubLoadingMinSeconds = 0.8f;

    bool m_RequestOpenGameModulePopup = false;
    char m_GameModulePathBuffer[256] = "";

    bool m_RequestOpenExportPopup = false;
    char m_ExportDirBuffer[512] = "export";
    bool m_ExportSelfContained = true;
    int m_ExportPlatform = 0; // 0=Windows, 1=Linux, 2=Android (CI)

    // ---- Atualização automática (menu Ajuda > Verificar Atualizações) ----
    void StartUpdateCheck();
    void DrawUpdateModals();
    std::thread m_UpdateThread;
    std::mutex m_UpdateMutex;
    bool m_UpdateBusy = false;
    int m_UpdateState = 0;  // 0 idle, 1 verificando, 2 nova versão, 3 baixando,
                            // 4 instalando, 5 relaunch, 6 erro
    std::string m_UpdateVersion;
    std::string m_UpdateUrl;
    std::string m_UpdateError;
    bool m_UpdateSkipAsk = false;
    bool m_UpdateStartupCheckDone = false;
    bool m_UpdateCheckOnStartup = true;
    char m_UpdateApiUrlBuf[512] = {};
    std::string m_UpdateApiUrlBufInput;

    // ---- Export Android local (engine compila o APK: SDK/NDK/dotnet) ----
    void StartAndroidExport();
    void DrawAndroidExportModals();
    std::thread m_AndroidThread;
    std::mutex m_AndroidMutex;
    bool m_AndroidRunning = false;
    kizuri::AndroidExporter::Tools m_AndroidTools;
    bool m_AndroidToolsChecked = false;
    bool m_AndroidDone = false;      // resultado pronto (ver strings)
    std::string m_AndroidResult;     // "APK: <path>" ou erro
    std::string m_AndroidMissing;
    bool m_AndroidErrPopupOpened = false;
    std::string m_UpdateZip;        // caminho do zip baixado
    bool m_UpdateInstallStarted = false;
    bool m_UpdatePopupOpened = false;   // edge p/ OpenPopup do modal
    bool m_UpdateErrPopupOpened = false; // embute o runtime .NET via dotnet publish
    // Build settings (export): nome/versão/resolução da janela do jogo.
    char m_ExportGameName[128] = "MeuJogo";
    char m_ExportVersion[32] = "1.0";
    int m_ExportWidth = 1280;
    int m_ExportHeight = 720;
    bool m_AutoCompileOnPlay = true;
    // Play com a CÂMERA DO JOGO no viewport (o que o jogador vê) — WASD mexe
    // o personagem na tela. Desliga pra voar com a câmera do editor.
    bool m_PlayUsesGameCamera = true;   // compila o assembly C# antes do Play (estilo Unity)
    bool m_ShowStats = true;           // overlay de estatísticas no viewport (FPS/draw calls/tris)
    bool m_ShowTextDiag = false;       // janela de diagnóstico do texto (atlas + estado GL)
    float m_FpsSmoothed = 0.0f;        // FPS médio suavizado (Profiler do viewport)
    bool m_ShowColliders = false;      // overlay de física: desenha TODOS os colliders da cena (debug)
    // Escultura de terreno (pilar AAA v0.34): pincel no viewport 3D.
    bool m_TerrainSculpting = false;
    float m_TerrainBrushRadius = 2.0f;
    float m_TerrainBrushStrength = 0.4f;
    int m_ThumbBudget = 0;             // thumbnails novos permitidos neste frame (anti-trava)

    // Preferências do editor (configuráveis em Configurações > Editor).
    float m_EditorCamFlySpeed = 4.0f;      // velocidade da câmera livre (Shift = x3)
    float m_EditorCamSensitivity = 0.12f;  // sensibilidade do mouse em graus/pixel
    float m_GizmoSnapTranslation = 0.5f;   // snapping de translação (Ctrl)
    float m_GizmoSnapRotation = 15.0f;     // snapping de rotação em graus (Ctrl)

    // Acha <Projeto>/Source/*.csproj (jogo) e a raiz do checkout da engine
    // (subindo da pasta bin/ até o marcador managed/Kizuri.Scripting). Usado
    // pelo Play (compilar) e pelo export (publish). Preenche vazio se não achar.
    void GetGameBuildInfo(std::string& outCsproj, std::string& outEngineRoot);

    bool m_RequestOpenSavePrefabPopup = false;
    char m_PrefabPathBuffer[512] = "Assets/entidade.kzprefab";
    kizuri::UUID m_PrefabEntityUUID;

    void DrawExportModal();
    void DrawSavePrefabModal();
    void ExportGame(const std::string& outputDir);
    // Console: filtro por severidade + busca por texto. m_ConsoleAutoScroll
    // gruda a rolagem no final enquanto está true, e se solta assim que a
    // pessoa rola manualmente pra cima (comportamento padrão de console).
    bool m_ConsoleShowTrace = true;
    bool m_ConsoleShowInfo = true;
    bool m_ConsoleShowWarn = true;
    bool m_ConsoleShowError = true;
    bool m_ConsoleAutoScroll = true;
    char m_ConsoleSearchBuffer[128] = "";

    // Content Browser: navega a pasta de assets do projeto ativo. Fica
    // vazio (sem raiz) enquanto nenhum projeto estiver aberto — o painel
    // mostra uma mensagem nesse caso em vez de tentar listar algo.
    std::filesystem::path m_ContentBrowserRoot;
    std::filesystem::path m_ContentBrowserCurrentDir;
    std::unordered_map<std::string, kizuri::Ref<kizuri::Texture2D>> m_ThumbCache;
    // Botão "Gerenciador" do Inspetor: revela o arquivo do campo no Content
    // Browser (navega pra pasta dele + abre o painel se estiver colapsado).
    std::string m_ContentBrowserRevealPath;
    bool m_ContentBrowserRevealRequested = false;

    // Busca de entidade na Hierarquia (filtra por nome; lista plana).
    char m_HierarchySearchBuffer[64] = "";

    // Renomear arquivo no Content Browser (menu de contexto).
    bool m_RequestRenamePopup = false;
    std::filesystem::path m_RenameTarget;
    char m_RenameBuffer[256] = "";

    // Janela de configurações (Arquivo > Configurações).
    bool m_ShowSettings = false;
    kizuri::GraphicsSettings m_GraphicsSettings;
    char m_EnvironmentHDRIPathBuffer[512] = "";
    int m_SettingsSection = 0; // 0 = Gráficos, 1 = Geral, 2 = Editor
    bool m_ViewportMaximized = false; // botão fullscreen do viewport (esconde os painéis laterais)

    glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;

    // Estado do frame anterior pra detectar borda de subida (tecla que
    // acabou de ser pressionada agora, não "está segurada"). Ver
    // OnImGuiRender: o atalho de undo/redo usa kizuri::Input (GLFW direto)
    // em vez de ImGui::IsKeyPressed(ImGuiKey, repeat=false) — essa segunda
    // forma foi isolada como o ponto exato de um crash silencioso nessa
    // função em determinados ambientes (ver KizuriEngine.log), então
    // trocamos pro caminho de Input que já está comprovadamente estável
    // (é o mesmo usado pela câmera livre do viewport) e replicamos a
    // detecção de borda manualmente aqui.
    bool m_PrevZKeyDown = false;
    bool m_PrevYKeyDown = false;
    bool m_PrevF5KeyDown = false;
    bool m_PrevDKeyDown = false;
    bool m_PrevF11KeyDown = false;

    // Alternância 2D/3D do viewport (botão na toolbar — ver
    // DrawViewportToolbar). Troca só o COMPORTAMENTO DO EDITOR (qual
    // câmera navega, qual grid aparece, se o passe 3D roda) — nunca uma
    // restrição da cena em si, que continua híbrida por baixo o tempo
    // todo (ver docs/NOTAS_INTERNAS.md).
    enum class ViewportMode { Mode2D, Mode3D };
    ViewportMode m_ViewportMode = ViewportMode::Mode3D;

    // Pintor de tilemap no viewport 2D: com uma entidade Tilemap selecionada,
    // botão esquerdo pinta o valor do pincel, botão direito apaga (0).
    // m_TilePaintBefore captura o estado antes do gesto pra empurrar um
    // EntityEditCommand (undo) quando o mouse solta.
    int m_TilemapBrushValue = 1;
    bool m_TilePainting = false;
    kizuri::EntitySnapshot m_TilePaintBefore;

    // Câmera livre do editor ("fly camera"): navega o viewport 3D
    // independente de qualquer CameraComponent da cena. Segurar botão
    // direito do mouse sobre o viewport ativa olhar (mouse) + movimento
    // (WASD, Q/E sobe-desce, Shift acelera).
    kizuri::PerspectiveCamera m_EditorCamera{ 45.0f, 16.0f / 9.0f, 0.01f, 1000.0f };
    glm::vec3 m_EditorCamPos = { 0.0f, 3.0f, 8.0f };
    float m_EditorCamYaw = -90.0f;
    float m_EditorCamPitch = -10.0f;
    // Órbita (Alt+arrastar): alvo ao redor do qual a câmera gira.
    glm::vec3 m_EditorOrbitTarget = { 0.0f, 0.0f, 0.0f };
    float m_EditorOrbitDist = -1.0f; // -1 = ainda sem alvo (calcula no 1º uso)
    glm::vec2 m_LastMousePos = { 0.0f, 0.0f };
    bool m_FirstMouseLook = true;

    // Câmera de edição 2D: pan (botão direito + arrastar) e zoom (scroll).
    // Independente de qualquer CameraComponent da cena, do mesmo jeito que
    // a câmera livre 3D acima — não precisa de uma entidade de câmera só
    // pra poder editar sprites.
    kizuri::OrthographicCamera m_Editor2DCamera{ -10.0f, 10.0f, -10.0f, 10.0f };
    glm::vec2 m_Editor2DCamPos = { 0.0f, 0.0f };
    float m_Editor2DZoom = 10.0f; // metade da altura visível, em unidades de mundo
    glm::vec2 m_Editor2DLastMousePos = { 0.0f, 0.0f };
    bool m_Editor2DFirstMouseLook = true;

    // Gizmo de transformação (ImGuizmo). W/E/R trocam entre
    // mover/rotacionar/escalar quando o viewport está com foco (e a
    // câmera livre não está ativa — botão direito solto). O retângulo é
    // salvo em coordenadas de tela porque é assim que ImGuizmo::SetRect
    // espera receber, e só sabemos isso depois de desenhar o painel.
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    glm::vec2 m_ViewportBounds[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    bool m_GizmoWasUsing = false;
    kizuri::EntitySnapshot m_GizmoEditBefore;

    // Undo/redo. m_InspectorEditEntity/m_InspectorEditBefore rastreiam uma
    // edição de propriedade em andamento no Inspetor (ImGui::IsAnyItemActive
    // sinaliza início/fim) — cobre DragFloat, ColorEdit, Checkbox, Combo e
    // os botões de Adicionar/Remover Componente com o mesmo mecanismo,
    // sem precisar de rastreamento por widget.
    kizuri::CommandHistory m_History;
    bool m_InspectorWasActive = false;
    kizuri::UUID m_InspectorEditEntity = kizuri::UUID::Invalid();
    kizuri::EntitySnapshot m_InspectorEditBefore;

    // Painéis dockáveis (Profiler, Game View, Material Editor, Animator,
    // Project Settings). Cada um é uma classe própria em UI/Panels — o
    // EditorLayer só os cria, preenche o contexto e chama render/update.
    std::vector<std::unique_ptr<EditorPanel>> m_Panels;
    std::unique_ptr<EditorContext> m_PanelContext;

    // Estado da barra de título customizada (ver DrawTitlebar/DrawResizeBorders).
    bool m_DraggingWindow = false;
    enum class ResizeEdge { None, Left, Right, Bottom, BottomLeft, BottomRight };
    ResizeEdge m_ResizingEdge = ResizeEdge::None;

    static constexpr float kTitlebarHeight = 38.0f;
    static constexpr float kMenubarHeight = 26.0f;
    static constexpr float kResizeBorder = 5.0f;
};
