#pragma once
#include "kizuri/core/Layer.hpp"

struct ImFont;
struct ImGuiContext;

namespace kizuri {

enum class KizuriFont {
    Regular,  // Texto padrão de toda a UI
    Bold,     // Cabeçalhos de painel, texto de destaque
    Titlebar  // Wordmark "KIZURI" na barra de título customizada
};

class ImGuiLayer : public Layer {
public:
    ImGuiLayer();
    ~ImGuiLayer() override = default;

    void OnAttach() override;
    void OnDetach() override;
    void OnEvent(Event& e) override;

    void Begin();
    void End();

    void SetBlockEvents(bool block) { m_BlockEvents = block; }
    void SetDarkThemeKizuri();

    // Retorna a fonte pedida, ou a fonte padrão do ImGui se o arquivo
    // .ttf correspondente não pôde ser carregado (ver LoadFonts()).
    ImFont* GetFont(KizuriFont font) const;

    // Retorna o ImGuiContext* criado dentro da KizuriEngine (ver OnAttach).
    // Necessário porque, em build SHARED, qualquer executável que linka a
    // engine dinamicamente e chama ImGui:: diretamente (hoje só o
    // KizuriEditor) acaba compilando sua PRÓPRIA cópia de imgui.cpp (fonte
    // estática puxada via ImGuizmo -> imgui). Isso faz o executável ter sua
    // própria variável global GImGui, separada da que a engine usa
    // internamente — então ImGui::Begin/Text/etc chamado do lado do
    // executável opera sobre um contexto nulo (nunca inicializado) e crasha
    // na hora, sem exceção, sem log ("abre e fecha"). Isso NUNCA aparece em
    // build STATIC porque aí só existe UM binário, logo uma única cópia de
    // GImGui pro processo inteiro.
    //
    // A correção é o executável chamar
    // ImGui::SetCurrentContext(ImGuiLayer::GetContext()) uma única vez,
    // ANTES de qualquer outra chamada ImGui:: sua (ver EditorLayer::OnAttach),
    // fazendo sua cópia local de GImGui apontar pro mesmo ImGuiContext que
    // vive dentro da KizuriEngine.dll/.so.
    static ImGuiContext* GetContext();

private:
    void LoadFonts();

    bool m_BlockEvents = true;
    ImFont* m_FontRegular = nullptr;
    ImFont* m_FontBold = nullptr;
    ImFont* m_FontTitlebar = nullptr;
};

} // namespace kizuri
