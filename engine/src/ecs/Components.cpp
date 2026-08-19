#include "kizuri/ecs/Components.hpp"
#include "kizuri/scripting/NativeScript.hpp"
#include "kizuri/scripting/ScriptEngine.hpp"
#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/project/Project.hpp"
#include "kizuri/core/Log.hpp"
#include <box2d/box2d.h>

namespace kizuri {

bool AnimatorStateMachineComponent::SetState(const std::string& name, float defaultBlend) {
    for (int i = 0; i < (int)States.size(); ++i) {
        if (States[i].Name != name) continue;
        if (i == CurrentState) return true;

        float blend = defaultBlend;
        for (const auto& tr : Transitions) {
            if (tr.To == i && (tr.From == -1 || tr.From == CurrentState)) { blend = tr.BlendTime; break; }
        }
        m_TransitionFrom = CurrentState;
        m_TransitionTime = 0.0f;
        m_TransitionDuration = glm::max(blend, 0.01f);
        CurrentState = i;
        return true;
    }
    return false;
}

bool AnimatorStateMachineComponent::IsInState(const std::string& name) const {
    return CurrentState >= 0 && CurrentState < (int)States.size() && States[CurrentState].Name == name;
}

void FoliageComponent::Regenerate() {
    Instances.clear();
    if (Count == 0) return;
    Instances.reserve(Count);
    uint32_t state = Seed * 747796405u + 2891336453u;
    auto rnd = [&state]() {
        state ^= state << 13; state ^= state >> 17; state ^= state << 5;
        return state * 1.0f / 4294967296.0f;
    };
    const float halfX = AreaSize.x * 0.5f, halfZ = AreaSize.y * 0.5f;
    const float avoidR = 1.5f;
    for (uint32_t i = 0; i < Count; ++i) {
        float x = 0.0f, z = 0.0f;
        for (int attempt = 0; attempt < 8; ++attempt) {
            x = (rnd() * 2.0f - 1.0f) * halfX;
            z = (rnd() * 2.0f - 1.0f) * halfZ;
            if (!AvoidCenter || std::sqrt(x * x + z * z) >= avoidR) break;
        }
        float scale = ScaleMin + rnd() * (ScaleMax - ScaleMin);
        float yaw = rnd() * 6.2831853f;
        glm::mat4 world = glm::translate(glm::mat4(1.0f), { x, 0.0f, z }) *
                          glm::rotate(glm::mat4(1.0f), yaw, { 0.0f, 1.0f, 0.0f }) *
                          glm::scale(glm::mat4(1.0f), { scale, scale * HeightScale, scale });
        Instances.push_back(world);
    }
}

void AudioSourceComponent::Play() {
    if (ClipPath.empty()) return;

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
    if (!RuntimeBody) return;
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

void Rigidbody2DComponent::ApplyForce(const glm::vec2& force, bool wake) {
    if (!RuntimeBody) return;
    static_cast<b2Body*>(RuntimeBody)->ApplyForceToCenter({ force.x, force.y }, wake);
}

float Rigidbody2DComponent::GetAngularVelocity() const {
    if (!RuntimeBody) return 0.0f;
    return static_cast<b2Body*>(RuntimeBody)->GetAngularVelocity();
}

void Rigidbody2DComponent::SetAngularVelocity(float w) {
    if (!RuntimeBody) return;
    static_cast<b2Body*>(RuntimeBody)->SetAngularVelocity(w);
}

void Rigidbody2DComponent::SetFixedRotation(bool fixed) {
    FixedRotation = fixed;
    if (!RuntimeBody) return;
    static_cast<b2Body*>(RuntimeBody)->SetFixedRotation(fixed);
}

void NativeScriptComponent::BindByName(const std::string& className) {
    ClassName = className;

    InstantiateScript = [className]() -> NativeScript* {
        NativeScript* instance = ScriptEngine::GetRegistry().Create(className);
        if (!instance)
            KZ_CORE_ERROR("O script '{0}' não foi encontrado no módulo de jogo carregado.", className);
        return instance;
    };
    DestroyScript = [](NativeScriptComponent* nsc) { nsc->DestroyInstance(); };
}

void NativeScriptComponent::DestroyInstance() {

    NativeScript* ptr = Instance;
    Instance = nullptr;
    delete ptr;
}

}
