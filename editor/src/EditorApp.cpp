#include <Kizuri.hpp>
#include <kizuri/core/EntryPoint.hpp>
#include "EditorLayer.hpp"

class KizuriEditorApp : public kizuri::Application {
public:
    KizuriEditorApp() : Application(MakeSpec()) {
        PushLayer(new EditorLayer());
    }

    static kizuri::ApplicationSpec MakeSpec() {
        kizuri::ApplicationSpec spec;
        spec.Name = "Kizuri Editor";
        spec.Width = 1920;
        spec.Height = 1080;
        spec.VSync = true;
        spec.CustomTitlebar = true;
        return spec;
    }
};

kizuri::Application* kizuri::CreateApplication() {
    return new KizuriEditorApp();
}
