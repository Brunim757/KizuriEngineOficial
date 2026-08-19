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

class EditorPanel; 
struct EditorContext;





class EditorLayer : public kizuri::Layer {
public:
    EditorLayer();
    ~EditorLayer() override; 

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
    
    
    void DrawNavDebug();
    
    
    void RevealFileInContentBrowser(const std::string& filePath);
    
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
    void DrawCamera2DPreview();
    void UpdateEditor2DCamera(kizuri::Timestep ts);
    void DrawViewportToolbar();
    void DrawGizmo();
    void DrawCameraGizmo(); 
    void DrawLightGizmo();  
    void DrawColliderGizmo(); 
    void DrawAllColliders();  
    void Reparent(kizuri::Entity child, kizuri::Entity newParent);
    
    bool IsEntityMultiSelected(kizuri::Entity entity) const;
    std::vector<kizuri::Entity> GetMultiSelection() const;
    void ClearMultiSelection();

    
    
    void DrawSettings();
    void DrawSettingsGraphics();
    void DrawSettingsGeneral();
    void DrawSettingsEditor();
    void LoadGraphicsSettingsFromDisk();
    void SaveGraphicsSettingsToDisk();

    
    
    kizuri::Ref<kizuri::Texture2D> GetThumbnail(const std::string& path);

    void NewScene();
    void SaveScene();
    void SaveSceneAs();
    void OpenScene(const std::string& path);

    
    
    
    void CreateDemoScene3D();
    
    void CreateDemoScene2D();
    
    
    void CreateDemoSceneAI();
    void CreateDemoSceneGame();
    
    
    void CreateDemoSceneNet();

    
    
    kizuri::Entity CreateEntityFromAsset(const std::string& path, const glm::vec3& worldPos);

    
    
    
    
    void AutoSwitchViewportMode();

    void CreateDefaultSceneContent();

    
    
    
    enum class SceneState { Edit = 0, Play = 1 };
    SceneState m_SceneState = SceneState::Edit;
    kizuri::Ref<kizuri::Scene> m_EditorScene;
    void OnScenePlay();
    void OnSceneStop();

    kizuri::Ref<kizuri::Scene> m_ActiveScene;
    kizuri::Ref<kizuri::Framebuffer> m_Framebuffer;
    kizuri::Entity m_SelectedEntity;
    
    
    std::unordered_set<kizuri::UUID> m_MultiSelection;

    
    
    
    
    
    bool m_SceneLoading = false;
    float m_PendingLoadProgress = 0.0f;
    std::string m_PendingScenePath;
    kizuri::Ref<kizuri::Scene> m_PendingScene;
    std::unique_ptr<kizuri::SceneSerializer> m_PendingLoader;

    
    
    
    
    void StartPlayInternal();
    std::thread m_PlayBuildThread;
    std::atomic<bool> m_PlayBuildDone{ false };
    std::atomic<bool> m_PlayBuildCancelled{ false };
    bool m_PlayBuildActive = false;
    bool m_PlayBuildOk = false;
    std::string m_PlayBuildDll, m_PlayBuildError;

    
    
    
    std::string m_ScenePath;

    
    
    
    bool m_RequestOpenSaveAsPopup = false;
    bool m_RequestOpenLoadPopup = false;
    char m_ScenePathBuffer[256] = "cena.kzscene";

    
    
    
    
    
    
    bool m_RequestOpenNewProjectPopup = false;
    bool m_RequestOpenLoadProjectPopup = false;
    char m_NewProjectDirBuffer[256] = "MeuJogo";
    char m_NewProjectNameBuffer[128] = "MeuJogo";
    int m_NewProjectModeIndex = 2; 
    char m_OpenProjectPathBuffer[256] = "";

    
    
    
    
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
    int m_ExportPlatform = 0; 

    
    void StartUpdateCheck();
    void DrawUpdateModals();
    std::thread m_UpdateThread;
    std::mutex m_UpdateMutex;
    bool m_UpdateBusy = false;
    int m_UpdateState = 0;  
                            
    std::string m_UpdateVersion;
    std::string m_UpdateUrl;
    std::string m_UpdateError;
    bool m_UpdateSkipAsk = false;
    bool m_UpdateStartupCheckDone = false;
    bool m_UpdateCheckOnStartup = true;
    char m_UpdateApiUrlBuf[512] = {};
    std::string m_UpdateApiUrlBufInput;

    
    void StartAndroidExport();
    void DrawAndroidExportModals();
    std::thread m_AndroidThread;
    std::mutex m_AndroidMutex;
    bool m_AndroidRunning = false;
    kizuri::AndroidExporter::Tools m_AndroidTools;
    bool m_AndroidToolsChecked = false;
    bool m_AndroidDone = false;      
    std::string m_AndroidResult;     
    std::string m_AndroidMissing;
    bool m_AndroidErrPopupOpened = false;

    
    void CompileAndRegisterGame();
    std::thread m_CompileRegThread;
    bool m_CompileRegBusy = false;
    std::string m_UpdateZip;        
    bool m_UpdateInstallStarted = false;
    bool m_UpdatePopupOpened = false;   
    bool m_UpdateErrPopupOpened = false; 
    
    char m_ExportGameName[128] = "MeuJogo";
    char m_ExportVersion[32] = "1.0";
    int m_ExportWidth = 1280;
    int m_ExportHeight = 720;
    bool m_AutoCompileOnPlay = true;
    
    
    bool m_PlayUsesGameCamera = true;   
    bool m_ShowStats = true;           
    bool m_ShowTextDiag = false;       
    float m_FpsSmoothed = 0.0f;        
    bool m_ShowColliders = false;      
    
    bool m_TerrainSculpting = false;
    float m_TerrainBrushRadius = 2.0f;
    float m_TerrainBrushStrength = 0.4f;
    int m_ThumbBudget = 0;             

    
    float m_EditorCamFlySpeed = 4.0f;      
    float m_EditorCamSensitivity = 0.12f;  
    float m_GizmoSnapTranslation = 0.5f;   
    float m_GizmoSnapRotation = 15.0f;     

    
    
    
    void GetGameBuildInfo(std::string& outCsproj, std::string& outEngineRoot);

    bool m_RequestOpenSavePrefabPopup = false;
    char m_PrefabPathBuffer[512] = "Assets/entidade.kzprefab";
    kizuri::UUID m_PrefabEntityUUID;

    void DrawExportModal();
    void DrawSavePrefabModal();
    void ExportGame(const std::string& outputDir);
    
    
    
    bool m_ConsoleShowTrace = true;
    bool m_ConsoleShowInfo = true;
    bool m_ConsoleShowWarn = true;
    bool m_ConsoleShowError = true;
    bool m_ConsoleAutoScroll = true;
    char m_ConsoleSearchBuffer[128] = "";

    
    
    
    std::filesystem::path m_ContentBrowserRoot;
    std::filesystem::path m_ContentBrowserCurrentDir;
    std::unordered_map<std::string, kizuri::Ref<kizuri::Texture2D>> m_ThumbCache;
    
    
    std::string m_ContentBrowserRevealPath;
    bool m_ContentBrowserRevealRequested = false;

    
    char m_HierarchySearchBuffer[64] = "";

    
    bool m_RequestRenamePopup = false;
    std::filesystem::path m_RenameTarget;
    char m_RenameBuffer[256] = "";

    
    bool m_ShowSettings = false;
    kizuri::GraphicsSettings m_GraphicsSettings;
    char m_EnvironmentHDRIPathBuffer[512] = "";
    int m_SettingsSection = 0; 
    bool m_ViewportMaximized = false; 

    glm::vec2 m_ViewportSize = { 0.0f, 0.0f };
    bool m_ViewportFocused = false;
    bool m_ViewportHovered = false;

    
    
    
    
    
    
    
    
    
    bool m_PrevZKeyDown = false;
    bool m_PrevYKeyDown = false;
    bool m_PrevF5KeyDown = false;
    bool m_PrevDKeyDown = false;
    bool m_PrevF11KeyDown = false;

    
    
    
    
    
    enum class ViewportMode { Mode2D, Mode3D };
    ViewportMode m_ViewportMode = ViewportMode::Mode3D;

    
    
    
    
    int m_TilemapBrushValue = 1;
    bool m_TilePainting = false;
    kizuri::EntitySnapshot m_TilePaintBefore;

    
    
    
    
    kizuri::PerspectiveCamera m_EditorCamera{ 45.0f, 16.0f / 9.0f, 0.01f, 1000.0f };
    glm::vec3 m_EditorCamPos = { 0.0f, 3.0f, 8.0f };
    float m_EditorCamYaw = -90.0f;
    float m_EditorCamPitch = -10.0f;
    
    glm::vec3 m_EditorOrbitTarget = { 0.0f, 0.0f, 0.0f };
    float m_EditorOrbitDist = -1.0f; 
    glm::vec2 m_LastMousePos = { 0.0f, 0.0f };
    bool m_FirstMouseLook = true;

    
    
    
    
    kizuri::OrthographicCamera m_Editor2DCamera{ -10.0f, 10.0f, -10.0f, 10.0f };
    glm::vec2 m_Editor2DCamPos = { 0.0f, 0.0f };
    float m_Editor2DZoom = 10.0f; 
    glm::vec2 m_Editor2DLastMousePos = { 0.0f, 0.0f };
    bool m_Editor2DFirstMouseLook = true;

    
    
    
    
    
    ImGuizmo::OPERATION m_GizmoOperation = ImGuizmo::TRANSLATE;
    glm::vec2 m_ViewportBounds[2] = { { 0.0f, 0.0f }, { 0.0f, 0.0f } };
    bool m_GizmoWasUsing = false;
    kizuri::EntitySnapshot m_GizmoEditBefore;

    
    
    
    
    
    kizuri::CommandHistory m_History;
    bool m_InspectorWasActive = false;
    kizuri::UUID m_InspectorEditEntity = kizuri::UUID::Invalid();
    kizuri::EntitySnapshot m_InspectorEditBefore;

    
    
    
    std::vector<std::unique_ptr<EditorPanel>> m_Panels;
    std::unique_ptr<EditorContext> m_PanelContext;

    
    bool m_DraggingWindow = false;
    enum class ResizeEdge { None, Left, Right, Bottom, BottomLeft, BottomRight };
    ResizeEdge m_ResizingEdge = ResizeEdge::None;

    static constexpr float kTitlebarHeight = 38.0f;
    static constexpr float kMenubarHeight = 26.0f;
    static constexpr float kResizeBorder = 5.0f;
};
