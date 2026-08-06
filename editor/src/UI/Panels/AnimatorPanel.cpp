#include "AnimatorPanel.hpp"
#include <imgui.h>
#include <algorithm>

void AnimatorPanel::OnImGuiRender() {
    if (!ImGui::Begin(GetTitle(), &m_Visible)) {
        ImGui::End();
        return;
    }
    if (!m_Ctx.ActiveScene || !m_Ctx.SelectedEntity) {
        ImGui::TextDisabled("Selecione uma entidade com Animator.");
        ImGui::End();
        return;
    }
    if (!m_Ctx.SelectedEntity.HasComponent<kizuri::AnimatorComponent>()) {
        ImGui::TextDisabled("A entidade selecionada não tem Animator.\n\nAdicione pelo Inspetor (Componente > Animador).");
        ImGui::End();
        return;
    }

    auto& anim = m_Ctx.SelectedEntity.GetComponent<kizuri::AnimatorComponent>();

    // Skin carregada sob demanda (path do .glb/.gltf).
    if (!anim.Skin && !anim.MeshPath.empty())
        anim.Skin = kizuri::SkinData::CreateFromGLTF(kizuri::Project::ResolvePath(anim.MeshPath));

    if (!anim.Skin) {
        ImGui::TextDisabled("Skin não carregada (path: %s)", anim.MeshPath.empty() ? "vazio" : anim.MeshPath.c_str());
        ImGui::End();
        return;
    }

    // Lista de clips.
    if (!anim.Skin->Clips.empty()) {
        m_SelectedClip = std::max(m_SelectedClip, 0);
        // Acha o índice do clip atual pra sincronizar a lista.
        int current = anim.Skin->GetClipIndex(anim.ClipName);
        if (current >= 0 && current != m_SelectedClip) m_SelectedClip = current;

        if (ImGui::BeginCombo("Clip", anim.ClipName.empty() ? "(pose de repouso)" : anim.ClipName.c_str())) {
            if (ImGui::Selectable("(pose de repouso)", anim.ClipName.empty()))
                { anim.ClipName.clear(); anim.Time = 0.0f; anim.Playing = false; }
            for (int i = 0; i < (int)anim.Skin->Clips.size(); ++i) {
                bool sel = (i == current);
                if (ImGui::Selectable(anim.Skin->Clips[i].Name.c_str(), sel)) {
                    anim.Play(anim.Skin->Clips[i].Name);
                    m_SelectedClip = i;
                }
            }
            ImGui::EndCombo();
        }
    } else {
        ImGui::TextDisabled("Sem clips de animação na skin.");
    }

    // Transport controls.
    bool playing = anim.Playing;
    if (ImGui::Button(playing ? "Pausar" : "Tocar"))
        anim.Playing = !anim.Playing;
    ImGui::SameLine();
    ImGui::Checkbox("Loop", &anim.Loop);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(100.0f);
    ImGui::DragFloat("Velocidade", &anim.Speed, 0.01f, -4.0f, 4.0f, "%.2fx");

    // Time scrubber.
    float dur = anim.Skin->GetClipDuration(anim.ClipName);
    if (dur > 0.0f) {
        float t = anim.Time;
        if (ImGui::SliderFloat("Tempo", &t, 0.0f, dur)) {
            anim.Time = t;
            anim.Playing = false; // scrub manual pausa
        }
        ImGui::TextDisabled("%.2f / %.2f s", anim.Time, dur);
    }
    ImGui::End();
}
