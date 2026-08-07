#include <Kizuri.hpp>
#include <kizuri/core/EntryPoint.hpp>
#include <kizuri/project/Project.hpp>
#include <nlohmann/json.hpp>
#include <imgui.h>
#include <filesystem>
#include <fstream>

using namespace kizuri;

// KizuriGame — o executável final de jogo. Diferente do editor, não tem
// nenhuma UI de edição: carrega a cena .kzscene inicial, carrega o
// GameModule (scripts C++) se existir, e roda o loop de runtime puro
// (física + scripts + render). É o que se entrega pro jogador.
class GameLayer : public Layer {
public:
    GameLayer(const std::string& scenePath, const std::string& modulePath)
        : Layer("GameLayer"), m_ScenePath(scenePath), m_ModulePath(modulePath) {}

    void OnAttach() override {
        auto& window = Application::Get().GetWindow();
        m_ViewportWidth = window.GetWidth();
        m_ViewportHeight = window.GetHeight();

        m_Scene = CreateRef<Scene>("Jogo");

        if (!m_ModulePath.empty()) {
            if (ScriptEngine::LoadModule(m_ModulePath))
                KZ_CORE_INFO("GameModule carregado: {0}", m_ModulePath);
            else
                KZ_CORE_WARN("GameModule não carregado ({0}): {1}", m_ModulePath, ScriptEngine::GetLastError());
        }

        if (SceneSerializer(m_Scene).Deserialize(m_ScenePath)) {
            KZ_CORE_INFO("Cena inicial carregada: {0}", m_ScenePath);
        } else {
            KZ_CORE_ERROR("Não foi possível carregar a cena inicial: {0}", m_ScenePath);
        }

        m_Scene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
        m_Scene->OnRuntimeStart();
    }

    void OnDetach() override {
        if (m_Scene) m_Scene->OnRuntimeStop();
        ScriptEngine::UnloadModule();
    }

    void OnUpdate(Timestep ts) override {
        RenderCommand::SetViewport(0, 0, m_ViewportWidth, m_ViewportHeight);
        RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.11f, 1.0f });
        RenderCommand::Clear();

        // Mouse em NDC de tela cheia (pro hit-test dos UIButton).
        auto [mx, my] = Input::GetMousePosition();
        glm::vec2 ndc{ 0.0f, 0.0f };
        if (m_ViewportWidth > 0 && m_ViewportHeight > 0) {
            ndc = { (mx / (float)m_ViewportWidth) * 2.0f - 1.0f,
                    1.0f - (my / (float)m_ViewportHeight) * 2.0f };
        }
        m_Scene->SetUIMouseNDC(ndc, Input::IsMouseButtonPressed(Mouse::Left));

        m_Scene->OnUpdateRuntime(ts);

        std::string nextScene;
        if (m_Scene->PollPendingLoad(nextScene)) {
            m_Scene->OnRuntimeStop();
            AudioEngine::StopAll();
            auto loaded = CreateRef<Scene>("Jogo");
            if (SceneSerializer(loaded).Deserialize(Project::ResolvePath(nextScene))) {
                loaded->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
                m_Scene = loaded;
                m_Scene->OnRuntimeStart();
                KZ_CORE_INFO("Cena trocada para: {0}", nextScene);
            } else {
                KZ_CORE_ERROR("Falha ao carregar cena: {0}", nextScene);
                Application::Get().Close();
            }
        }
    }

    void OnImGuiRender() override {}

    void OnEvent(Event& e) override {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
            if (ev.GetKeyCode() == Key::Escape) Application::Get().Close();
            return false;
        });
        dispatcher.Dispatch<WindowResizeEvent>([this](WindowResizeEvent& ev) {
            m_ViewportWidth = ev.GetWidth();
            m_ViewportHeight = ev.GetHeight();
            if (m_Scene) m_Scene->OnViewportResize(m_ViewportWidth, m_ViewportHeight);
            return false;
        });
    }

private:
    Ref<Scene> m_Scene;
    std::string m_ScenePath;
    std::string m_ModulePath;
    uint32_t m_ViewportWidth = 1600;
    uint32_t m_ViewportHeight = 900;
};

class GameApp : public Application {
public:
    GameApp() : Application(MakeSpec()) {
        const auto& args = kizuri::GetCommandLineArgs();
        std::string scenePath = "Start.kzscene";
        std::string modulePath;

        // Se existir .kzproj no CWD com StartScenePath, usa como padrão.
        if (auto project = TryLoadProjectFromCwd()) {
            if (!project->GetConfig().StartScenePath.empty())
                scenePath = Project::ResolvePath(project->GetConfig().StartScenePath);
        }

        if (args.size() > 1) scenePath = args[1];
        if (args.size() > 2) modulePath = args[2];

        PushLayer(new GameLayer(scenePath, modulePath));
    }

    static Ref<Project> TryLoadProjectFromCwd() {
        namespace fs = std::filesystem;
        std::error_code ec;
        for (auto& entry : fs::directory_iterator(fs::current_path(), ec)) {
            if (!entry.is_regular_file()) continue;
            if (entry.path().extension() == ".kzproj")
                return Project::Load(entry.path().string());
        }
        return nullptr;
    }

    static ApplicationSpec MakeSpec() {
        ApplicationSpec spec;
        spec.Name = "Kizuri Game";
        spec.Width = 1600;
        spec.Height = 900;
        spec.VSync = true;

        // Build settings (game.json) do export: nome + resolução da janela.
        namespace fs = std::filesystem;
        std::error_code ec;
        std::ifstream f("game.json");
        if (f.is_open()) {
            nlohmann::json j;
            try { f >> j; } catch (...) { return spec; }
            if (j.contains("name") && j["name"].is_string()) spec.Name = j["name"].get<std::string>();
            if (j.contains("width") && j["width"].is_number_integer()) spec.Width = j["width"].get<int>();
            if (j.contains("height") && j["height"].is_number_integer()) spec.Height = j["height"].get<int>();
        }
        return spec;
    }
};

kizuri::Application* kizuri::CreateApplication() {
    return new GameApp();
}
