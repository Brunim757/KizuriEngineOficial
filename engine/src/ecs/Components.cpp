#include "kizuri/ecs/Components.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/project/Project.hpp"
#include "kizuri/core/Log.hpp"
#include <box2d/box2d.h>

namespace kizuri {

void AudioSourceComponent::Play() {
    if (ClipPath.empty()) return;
    // Carrega sob demanda (primeira vez) e já configura a atenuação.
    if (Handle == kInvalidSound) {
        Handle = AudioEngine::LoadSound(Project::ResolvePath(ClipPath), ClipPath, false);
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

void Rigidbody2DComponent::ApplyLinearImpulse(const glm::vec2& impulse, bool wake) {
    if (!RuntimeBody) return; // corpo ainda não criado (só existe durante o Play)
    static_cast<b2Body*>(RuntimeBody)->ApplyLinearImpulseToCenter({ impulse.x, impulse.y }, wake);
}

void Rigidbody2DComponent::SetLinearVelocity(const glm::vec2& velocity) {
    if (!RuntimeBody) return;
    static_cast<b2Body*>(RuntimeBody)->SetLinearVelocity({ velocity.x, velocity.y });
}

glm::vec2 Rigidbody2DComponent::GetLinearVelocity() const {
    if (!RuntimeBody) return { 0.0f, 0.0f };
    const auto& v = static_cast<b2Body*>(RuntimeBody)->GetLinearVelocity();
    return { v.x, v.y };
}

void Rigidbody2DComponent::SetTransform(const glm::vec2& position, float angleRadians) {
    if (!RuntimeBody) return;
    static_cast<b2Body*>(RuntimeBody)->SetTransform({ position.x, position.y }, angleRadians);
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
            KZ_CORE_ERROR("O script '{0}' não foi encontrado no módulo de jogo carregado.", className);
        return instance;
    };
    DestroyScript = [](NativeScriptComponent* nsc) { delete nsc->Instance; nsc->Instance = nullptr; };
}

} // namespace kizuri
