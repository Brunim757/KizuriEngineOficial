#include "kizuri/core/ImGuiLayer.hpp"
#include "kizuri/core/Application.hpp"
#include "kizuri/core/Log.hpp"
#include <imgui.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <fstream>
#include <EmbeddedFontRegular.hpp>
#include <EmbeddedFontBold.hpp>

namespace kizuri {

ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") {}

void ImGuiLayer::OnAttach() {
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;

    // Sem imgui.ini: o layout de painéis é sempre o padrão embutido no
    // código (ver EditorLayer::DrawDockspace), não algo que persiste entre
    // sessões num arquivo solto ao lado do executável. Isso também evita
    // que um layout salvo de uma versão antiga do editor (com painéis que
    // não existem mais) deixe a UI num estado inconsistente.
    io.IniFilename = nullptr;

    LoadFonts();
    SetDarkThemeKizuri();

    auto* window = static_cast<GLFWwindow*>(Application::Get().GetWindow().GetNativeWindow());
    ImGui_ImplGlfw_InitForOpenGL(window, true);

    // A diretiva GLSL passada aqui PRECISA bater com o contexto que a
    // Window realmente conseguiu criar (Window::Init tenta 4.5 -> 4.1 ->
    // 3.3 core, na ordem, e fica com a primeira que a GPU/driver aceitar).
    // Antes disso era fixo em "#version 450 core", ou seja: numa máquina
    // que só conseguiu 3.3 core (comum em iGPU antiga, área de trabalho
    // remota, ou camadas de tradução tipo Zink/Vortek/Winlator — exatamente
    // o cenário do log que motivou essa correção), o backend do ImGui
    // tentava compilar um shader GLSL 450 sobre um contexto 3.3. Alguns
    // drivers tolerantes (Mesa/zink) deixam passar; drivers rígidos
    // (NVIDIA/AMD/Intel oficiais) rejeitam — falha de compilação de shader
    // silenciosa na melhor hipótese, comportamento indefinido na pior.
    // As shaders do ImGui em si não usam nenhum recurso exclusivo de GLSL
    // 4.x, então é seguro baixar a diretiva pra casar com o contexto real.
    const auto& win = Application::Get().GetWindow();
    const char* glslVersion = "#version 330 core";
    if (win.GetGLVersionMajor() > 4 || (win.GetGLVersionMajor() == 4 && win.GetGLVersionMinor() >= 5))
        glslVersion = "#version 450 core";
    else if (win.GetGLVersionMajor() == 4)
        glslVersion = "#version 410 core";
    KZ_CORE_INFO("ImGuiLayer: inicializando o backend OpenGL3 com '{0}' (contexto real: {1}.{2})",
                 glslVersion, win.GetGLVersionMajor(), win.GetGLVersionMinor());
    ImGui_ImplOpenGL3_Init(glslVersion);
}

// Fontes JetBrains Mono (SIL OFL) — dão à Kizuri Engine uma identidade de
// "ferramenta de dev" própria, em vez da fonte padrão do ImGui (Proggy
// Clean), que é a mesma usada por qualquer app genérico feito com ImGui.
// Os .ttf em engine/resources/fonts/ são convertidos em arrays de bytes C++
// em tempo de build (ver engine/CMakeLists.txt + cmake/EmbedResource.cmake)
// e compilados direto no executável — não há mais nenhuma pasta "assets"
// pra distribuir junto do binário nem caminho relativo pra resolver em
// runtime.
void ImGuiLayer::LoadFonts() {
    ImGuiIO& io = ImGui::GetIO();

    static const ImWchar ranges[] = { 0x0020, 0x00FF, 0 }; // Latin básico + acentos (pt-BR)

    ImFontConfig config;
    config.OversampleH = 2;
    config.OversampleV = 2;
    // Os dados ficam em arrays `static`/`inline const` com duração de todo
    // o programa, então o ImGui não precisa (e não deve) copiá-los nem
    // tentar liberá-los quando o atlas de fontes for destruído.
    config.FontDataOwnedByAtlas = false;

    m_FontRegular = io.Fonts->AddFontFromMemoryTTF(
        (void*)kizuri::embedded::kFontRegularTTF, (int)kizuri::embedded::kFontRegularTTF_size,
        16.0f, &config, ranges);
    m_FontBold = io.Fonts->AddFontFromMemoryTTF(
        (void*)kizuri::embedded::kFontBoldTTF, (int)kizuri::embedded::kFontBoldTTF_size,
        16.0f, &config, ranges);
    m_FontTitlebar = io.Fonts->AddFontFromMemoryTTF(
        (void*)kizuri::embedded::kFontBoldTTF, (int)kizuri::embedded::kFontBoldTTF_size,
        20.0f, &config, ranges);

    if (!m_FontRegular || !m_FontBold || !m_FontTitlebar) {
        KZ_CORE_WARN("Não foi possível carregar as fontes JetBrains Mono integradas; usando a fonte padrão do ImGui.");
        if (!m_FontRegular) m_FontRegular = io.Fonts->AddFontDefault();
        if (!m_FontBold) m_FontBold = m_FontRegular;
        if (!m_FontTitlebar) m_FontTitlebar = m_FontRegular;
    }

    io.FontDefault = m_FontRegular;
}

// Roda sempre dentro do binário da engine (é aqui que a função existe de
// verdade — em build SHARED, o executável só enxerga o import/thunk pra
// isso, nunca uma cópia inline própria), então ImGui::GetCurrentContext()
// aqui sempre lê o GImGui que ImGuiLayer::OnAttach() de fato inicializou.
ImGuiContext* ImGuiLayer::GetContext() {
    return ImGui::GetCurrentContext();
}

ImFont* ImGuiLayer::GetFont(KizuriFont font) const {
    switch (font) {
        case KizuriFont::Bold:     return m_FontBold;
        case KizuriFont::Titlebar: return m_FontTitlebar;
        default:                   return m_FontRegular;
    }
}

void ImGuiLayer::OnDetach() {
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

void ImGuiLayer::OnEvent(Event& e) {
    if (m_BlockEvents) {
        ImGuiIO& io = ImGui::GetIO();
        e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
        e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
    }
}

void ImGuiLayer::Begin() {
    KZ_CORE_TRACE("ImGuiLayer::Begin — NewFrame (OpenGL3)");
    ImGui_ImplOpenGL3_NewFrame();
    KZ_CORE_TRACE("ImGuiLayer::Begin — NewFrame (GLFW)");
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    KZ_CORE_TRACE("ImGuiLayer::Begin — fim");
}

void ImGuiLayer::End() {
    KZ_CORE_TRACE("ImGuiLayer::End — início");
    ImGuiIO& io = ImGui::GetIO();
    Application& app = Application::Get();
    io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

    KZ_CORE_TRACE("ImGuiLayer::End — ImGui::Render (monta draw data)");
    ImGui::Render();
    KZ_CORE_TRACE("ImGuiLayer::End — RenderDrawData (submete pro OpenGL)");
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    KZ_CORE_TRACE("ImGuiLayer::End — RenderDrawData ok");

    if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
        KZ_CORE_TRACE("ImGuiLayer::End — atualizando viewports de plataforma (janelas destacadas)");
        GLFWwindow* backup = glfwGetCurrentContext();
        ImGui::UpdatePlatformWindows();
        ImGui::RenderPlatformWindowsDefault();
        glfwMakeContextCurrent(backup);
        KZ_CORE_TRACE("ImGuiLayer::End — viewports de plataforma ok");
    }
    KZ_CORE_TRACE("ImGuiLayer::End — fim");
}

// Paleta escura com destaque em "vermelho torii" — identidade visual da Kizuri Engine.
void ImGuiLayer::SetDarkThemeKizuri() {
    ImGuiStyle& style = ImGui::GetStyle();

    // Proporções mais retas/técnicas (quase sem arredondamento) combinam
    // com a fonte monoespaçada e reforçam a sensação de "ferramenta",
    // em vez do arredondamento suave padrão do ImGui que aparece em
    // qualquer app feito com a lib sem nenhum ajuste.
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;
    style.WindowBorderSize = 1.0f;
    style.FrameBorderSize = 0.0f;
    style.WindowPadding = ImVec2(8.0f, 8.0f);
    style.FramePadding = ImVec2(6.0f, 4.0f);
    style.ItemSpacing = ImVec2(8.0f, 5.0f);
    style.IndentSpacing = 16.0f;
    style.ScrollbarSize = 12.0f;
    style.GrabMinSize = 10.0f;

    auto& colors = style.Colors;
    colors[ImGuiCol_WindowBg] = ImVec4{ 0.08f, 0.085f, 0.09f, 1.0f };
    colors[ImGuiCol_Border]   = ImVec4{ 0.16f, 0.16f, 0.18f, 1.0f };

    colors[ImGuiCol_Header]        = ImVec4{ 0.75f, 0.15f, 0.18f, 1.0f };
    colors[ImGuiCol_HeaderHovered] = ImVec4{ 0.85f, 0.20f, 0.22f, 1.0f };
    colors[ImGuiCol_HeaderActive]  = ImVec4{ 0.65f, 0.12f, 0.15f, 1.0f };

    colors[ImGuiCol_Button]        = ImVec4{ 0.18f, 0.18f, 0.20f, 1.0f };
    colors[ImGuiCol_ButtonHovered] = ImVec4{ 0.75f, 0.15f, 0.18f, 1.0f };
    colors[ImGuiCol_ButtonActive]  = ImVec4{ 0.65f, 0.12f, 0.15f, 1.0f };

    colors[ImGuiCol_FrameBg]        = ImVec4{ 0.15f, 0.15f, 0.17f, 1.0f };
    colors[ImGuiCol_FrameBgHovered] = ImVec4{ 0.22f, 0.13f, 0.14f, 1.0f };
    colors[ImGuiCol_FrameBgActive]  = ImVec4{ 0.28f, 0.12f, 0.13f, 1.0f };

    colors[ImGuiCol_Tab]                = ImVec4{ 0.13f, 0.13f, 0.15f, 1.0f };
    colors[ImGuiCol_TabHovered]         = ImVec4{ 0.75f, 0.15f, 0.18f, 1.0f };
    colors[ImGuiCol_TabActive]          = ImVec4{ 0.45f, 0.13f, 0.15f, 1.0f };
    colors[ImGuiCol_TabUnfocused]       = ImVec4{ 0.11f, 0.11f, 0.13f, 1.0f };
    colors[ImGuiCol_TabUnfocusedActive] = ImVec4{ 0.20f, 0.11f, 0.12f, 1.0f };

    colors[ImGuiCol_TitleBg]          = ImVec4{ 0.08f, 0.08f, 0.09f, 1.0f };
    colors[ImGuiCol_TitleBgActive]    = ImVec4{ 0.45f, 0.13f, 0.15f, 1.0f };
    colors[ImGuiCol_TitleBgCollapsed] = ImVec4{ 0.08f, 0.08f, 0.09f, 1.0f };
}

} // namespace kizuri
