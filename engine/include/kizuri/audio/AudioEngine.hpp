#pragma once
#include "kizuri/Core.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace kizuri {

using SoundHandle = uint32_t;
constexpr SoundHandle kInvalidSound = 0;

// AudioEngine encapsula miniaudio (mixagem, streaming, spatial audio 3D).
// API pensada para ser chamada tanto por C++ quanto futuramente pela KZScript.
class AudioEngine {
public:
    static void Init();
    static void Shutdown();

    static SoundHandle LoadSound(const std::string& name, const std::string& path, bool stream = false);
    // group: 0=SFX, 1=Música, 2=UI — o volume pedido é multiplicado pelo
    // volume do grupo (Audio mixer).
    static void Play(SoundHandle handle, bool loop = false, float volume = 1.0f, int group = 0);
    static void PlayOneShot(const std::string& path, float volume = 1.0f, int group = 0);
    // One-shot POSICIONAL (3D): toca na posição do mundo, atenuado pela
    // distância ao listener. O pool interno desinicializa quando o som acaba.
    static void PlayOneShotAt(const std::string& path, float volume, const glm::vec3& position, int group = 0);
    static void Stop(SoundHandle handle);
    static void StopAll(); // pra sair do Play: para e libera tudo, senão vazava (e continuava tocando!) entre sessões
    static void SetVolume(SoundHandle handle, float volume);

    static void SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
    static void SetSoundPosition3D(SoundHandle handle, const glm::vec3& position);
    static void SetSoundAttenuation(SoundHandle handle, float minDistance, float maxDistance);
    static bool IsSoundPlaying(SoundHandle handle);

    static void SetMasterVolume(float volume);

    // Audio mixer: volumes por grupo (0=SFX, 1=Música, 2=UI). Aplicados como
    // multiplicadores em Play/PlayOneShot.
    static void SetGroupVolume(int group, float volume);
    static float GetGroupVolume(int group);
};

} // namespace kizuri
