#pragma once
#include <Kizuri.hpp>
#include <imgui.h>
#include <ImGuizmo.h>
#include <glm/glm.hpp>
#include <string>
#include <filesystem>

// EditorLayer implementa o esqueleto do Kizuri Editor: dockspace ImGui,
// painel de hierarquia de entidades e inspetor de componentes básico.
// É a base para evoluir num editor visual completo (viewport renderizado
// para uma textura de framebuffer, gizmos, etc).
class EditorLayer : public kizuri::Layer {
public:
    EditorLayer();

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
    void DrawEntityNode(kizuri::Entity entity, kizuri::Entity& outEntityToDelete, bool editable);
    void DrawAddComponentButton();
    void DrawSceneFileModals();
    void DrawProjectModals();
    void DrawGameModuleModal();
    void DrawConsole();
    void DrawContentBrowser();
    void UpdateEditorCamera(kizuri::Timestep ts);
    void UpdateEditor2DCamera(kizuri::Timestep ts);
    void DrawViewportToolbar();
    void DrawGizmo();
    void DrawCameraGizmo(); // pirâmide de frustum + seta de direção, só quando uma Camera tá selecionada
    void Reparent(kizuri::Entity child, kizuri::Entity newParent);

    void NewScene();
    void SaveScene();
    void SaveSceneAs();
    void OpenScene(const std::string& path);

    // Cria uma entidade a partir de um arquivo de asset (soltado do Content
    // Browser no viewport): .obj -> MeshRenderer, imagem -> SpriteRenderer.
    kizuri::Entity CreateEntityFromAsset(const std::string& path, const glm::vec3& worldPos);

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

    bool m_RequestOpenGameModulePopup = false;
    char m_GameModulePathBuffer[256] = "";

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

    // Alternância 2D/3D do viewport (botão na toolbar — ver
    // DrawViewportToolbar). Troca só o COMPORTAMENTO DO EDITOR (qual
    // câmera navega, qual grid aparece, se o passe 3D roda) — nunca uma
    // restrição da cena em si, que continua híbrida por baixo o tempo
    // todo (ver docs/NOTAS_INTERNAS.md).
    enum class ViewportMode { Mode2D, Mode3D };
    ViewportMode m_ViewportMode = ViewportMode::Mode3D;

    // Câmera livre do editor ("fly camera"): navega o viewport 3D
    // independente de qualquer CameraComponent da cena. Segurar botão
    // direito do mouse sobre o viewport ativa olhar (mouse) + movimento
    // (WASD, Q/E sobe-desce, Shift acelera).
    kizuri::PerspectiveCamera m_EditorCamera{ 45.0f, 16.0f / 9.0f, 0.01f, 1000.0f };
    glm::vec3 m_EditorCamPos = { 0.0f, 3.0f, 8.0f };
    float m_EditorCamYaw = -90.0f;
    float m_EditorCamPitch = -10.0f;
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

    // Estado da barra de título customizada (ver DrawTitlebar/DrawResizeBorders).
    bool m_DraggingWindow = false;
    enum class ResizeEdge { None, Left, Right, Bottom, BottomLeft, BottomRight };
    ResizeEdge m_ResizingEdge = ResizeEdge::None;

    static constexpr float kTitlebarHeight = 38.0f;
    static constexpr float kMenubarHeight = 26.0f;
    static constexpr float kResizeBorder = 5.0f;
};
