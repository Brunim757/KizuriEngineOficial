#pragma once
#include "kizuri/core/Layer.hpp"

struct ImFont;
struct ImGuiContext;

namespace kizuri {

enum class KizuriFont {
    Regular,
    Bold,
    Titlebar
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

    ImFont* GetFont(KizuriFont font) const;

    static ImGuiContext* GetContext();

private:
    void LoadFonts();

    bool m_BlockEvents = true;
    ImFont* m_FontRegular = nullptr;
    ImFont* m_FontBold = nullptr;
    ImFont* m_FontTitlebar = nullptr;
};

}
