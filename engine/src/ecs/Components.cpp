#include "kizuri/ecs/Components.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/core/Log.hpp"

namespace kizuri {

void AudioSourceComponent::Play() {
    if (ClipPath.empty()) return;
    // Carrega sob demanda (primeira vez) e já configura a atenuação.
    if (Handle == kInvalidSound) {
        Handle = AudioEngine::LoadSound(ClipPath, ClipPath, false);
        if (Handle == kInvalidSound) return;
        AudioEngine::SetSoundAttenuation(Handle, MinDistance, MaxDistance);
    }
    AudioEngine::Play(Handle, Loop, Volume);
    HasStarted = true;
}

void AudioSourceComponent::Stop() {
    if (Handle != kInvalidSound) AudioEngine::Stop(Handle);
    HasStarted = false;
}

bool AudioSourceComponent::IsPlaying() const {
    return Handle != kInvalidSound && AudioEngine::IsSoundPlaying(Handle);
}

void NativeScriptComponent::BindByName(const std::string& className) {
    ClassName = className;

    // Captura só o nome (string), não um ponteiro de fábrica direto — se
    // o GameModule for recarregado entre o Bind e a instanciação de fato
    // (Play), essa busca por nome sempre pega a versão mais recente
    // registrada, em vez de um factory apontando pro módulo antigo.
    InstantiateScript = [className]() -> NativeScript* {
        NativeScript* instance = ScriptEngine::GetRegistry().Create(className);
        if (!instance)
            KZ_CORE_ERROR("Script '{0}' não encontrado no GameModule carregado.", className);
        return instance;
    };
    DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
}

} // namespace kizuri
