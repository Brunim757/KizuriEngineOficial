#pragma once
#include "kizuri/Core.hpp"
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>

namespace kizuri {

using SoundHandle = uint32_t;
constexpr SoundHandle kInvalidSound = 0;



class AudioEngine {
public:
    static void Init();
    static void Shutdown();

    static SoundHandle LoadSound(const std::string& name, const std::string& path, bool stream = false);
    
    
    static void Play(SoundHandle handle, bool loop = false, float volume = 1.0f, int group = 0);
    static void PlayOneShot(const std::string& path, float volume = 1.0f, int group = 0);
    
    
    static void PlayOneShotAt(const std::string& path, float volume, const glm::vec3& position, int group = 0);
    static void Stop(SoundHandle handle);
    static void StopAll(); 
    static void SetVolume(SoundHandle handle, float volume);

    static void SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up);
    static void SetSoundPosition3D(SoundHandle handle, const glm::vec3& position);
    static void SetSoundAttenuation(SoundHandle handle, float minDistance, float maxDistance);
    static bool IsSoundPlaying(SoundHandle handle);

    static void SetMasterVolume(float volume);

    
    
    static void SetGroupVolume(int group, float volume);
    static float GetGroupVolume(int group);

    
    
    
    
    static void SetGlobalReverb(float wet, float roomSize, float damp);

    
    
    static void SetSoundReverb(SoundHandle handle, bool enabled, float wet = 1.0f);
    static bool IsSoundReverbing(SoundHandle handle);
};

} 
