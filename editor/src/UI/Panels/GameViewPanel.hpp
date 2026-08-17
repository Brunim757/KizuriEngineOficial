#pragma once
#include "EditorPanel.hpp"

// Aba "Game View": mostra o JOGO num framebuffer próprio, separado do
// viewport de edição — e agora também no EDIT: se a cena tem uma câmera
// primária, o painel renderiza a câmera do jogo ao vivo (preview) mesmo
// sem Play. O "Focar câmera" virou um mini-editor da câmera (FOV, Near,
// Far, OrthoSize, Tipo) que edita a câmera primária DIRETO na cena ativa:
// no Edit as mudanças persistem (salvar cena) e no Play mexem na cópia
// em execução — preview sem ter que dar Play toda hora.
class GameViewPanel : public EditorPanel {
public:
    explicit GameViewPanel(const EditorContext& ctx) : m_Ctx(ctx) {}
    const char* GetTitle() const override { return "Game View"; }

    void OnUpdate(kizuri::Timestep ts) override;
    void OnImGuiRender() override;

private:
    const EditorContext& m_Ctx;
    kizuri::Ref<kizuri::Framebuffer> m_Framebuffer;
    bool m_Focused = false;
    bool m_Hovered = false;
};