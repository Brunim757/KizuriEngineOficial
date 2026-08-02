#include <Kizuri.hpp>
#include <kizuri/core/EntryPoint.hpp>
#include <imgui.h>

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
        m_Scene = CreateRef<Scene>("Jogo");

        // Scripts C++ do jogo (opcional — o jogo pode ser só cena).
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

        m_Scene->OnRuntimeStart();
    }

    void OnDetach() override {
        m_Scene->OnRuntimeStop();
        ScriptEngine::UnloadModule();
    }

    void OnUpdate(Timestep ts) override {
        RenderCommand::SetClearColor({ 0.08f, 0.09f, 0.11f, 1.0f });
        RenderCommand::Clear();
        m_Scene->OnUpdateRuntime(ts);
    }

    void OnImGuiRender() override {
        // Sem nenhuma UI de debug por padrão — o jogo final é só o render.
        // (A janela "Debug" do Sandbox não existe aqui; se um dia quiser um
        // overlay de FPS, é só adicionar.)
    }

    void OnEvent(Event& e) override {
        EventDispatcher dispatcher(e);
        dispatcher.Dispatch<KeyPressedEvent>([this](KeyPressedEvent& ev) {
            if (ev.GetKeyCode() == Key::Escape) Application::Get().Close();
            return false;
        });
    }

private:
    Ref<Scene> m_Scene;
    std::string m_ScenePath;
    std::string m_ModulePath;
};

class GameApp : public Application {
public:
    GameApp() : Application(MakeSpec()) {
        const auto& args = kizuri::GetCommandLineArgs();
        std::string scenePath = "Start.kzscene";  // padrão: cena inicial na pasta atual
        std::string modulePath;

        if (args.size() > 1) scenePath = args[1];
        if (args.size() > 2) modulePath = args[2];

        PushLayer(new GameLayer(scenePath, modulePath));
    }

    static ApplicationSpec MakeSpec() {
        ApplicationSpec spec;
        spec.Name = "Kizuri Game";
        spec.Width = 1600;
        spec.Height = 900;
        spec.VSync = true;
        return spec;
    }
};

kizuri::Application* kizuri::CreateApplication() {
    return new GameApp();
}
