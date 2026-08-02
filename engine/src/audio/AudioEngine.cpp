#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/core/Log.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <unordered_map>
#include <memory>

namespace kizuri {

struct LoadedSound {
    ma_sound Sound;
    // Sem isso, ma_sound_init_from_file deixava o som "vivo" no grafo de mixagem interno do
    // miniaudio pra sempre — só liberar a memória em C++ (via unique_ptr) não desconectava
    // ele de lá, um use-after-free em potencial na próxima vez que o engine processasse áudio.
    ~LoadedSound() { ma_sound_uninit(&Sound); }
};

static ma_engine s_Engine;
static bool s_Initialized = false;
static std::unordered_map<SoundHandle, std::unique_ptr<LoadedSound>> s_Sounds;
static std::unordered_map<std::string, SoundHandle> s_NameToHandle;
static SoundHandle s_NextHandle = 1;

void AudioEngine::Init() {
    ma_result result = ma_engine_init(nullptr, &s_Engine);
    if (result != MA_SUCCESS) {
        KZ_CORE_ERROR("Falha ao inicializar o AudioEngine (miniaudio)!");
        return;
    }
    s_Initialized = true;
    KZ_CORE_INFO("AudioEngine inicializado (miniaudio)");
}

void AudioEngine::Shutdown() {
    if (!s_Initialized) return;
    s_Sounds.clear();
    ma_engine_uninit(&s_Engine);
    s_Initialized = false;
}

SoundHandle AudioEngine::LoadSound(const std::string& name, const std::string& path, bool stream) {
    if (!s_Initialized) return kInvalidSound;

    auto loaded = std::make_unique<LoadedSound>();
    ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : 0;
    ma_result result = ma_sound_init_from_file(&s_Engine, path.c_str(), flags, nullptr, nullptr, &loaded->Sound);
    if (result != MA_SUCCESS) {
        KZ_CORE_ERROR("Falha ao carregar som '{0}' em '{1}'", name, path);
        return kInvalidSound;
    }

    SoundHandle handle = s_NextHandle++;
    s_Sounds[handle] = std::move(loaded);
    s_NameToHandle[name] = handle;
    KZ_CORE_INFO("Som carregado: {0} ({1})", name, path);
    return handle;
}

void AudioEngine::Play(SoundHandle handle, bool loop, float volume) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_set_looping(&it->second->Sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&it->second->Sound, volume);
    ma_sound_start(&it->second->Sound);
}

void AudioEngine::PlayOneShot(const std::string& path, float volume) {
    if (!s_Initialized) return;
    ma_engine_play_sound(&s_Engine, path.c_str(), nullptr);
    (void)volume; // miniaudio permite volume por grupo; simplificado aqui
}

void AudioEngine::Stop(SoundHandle handle) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_stop(&it->second->Sound);
}

void AudioEngine::StopAll() {
    // Limpar o mapa destrói cada LoadedSound, e o destrutor dele agora chama ma_sound_uninit
    // (ver correção acima) — é assim que os sons realmente param e a memória nativa é liberada.
    s_Sounds.clear();
}

void AudioEngine::SetVolume(SoundHandle handle, float volume) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_set_volume(&it->second->Sound, volume);
}

void AudioEngine::SetListenerPosition(const glm::vec3& position, const glm::vec3& forward, const glm::vec3& up) {
    if (!s_Initialized) return;
    ma_engine_listener_set_position(&s_Engine, 0, position.x, position.y, position.z);
    ma_engine_listener_set_direction(&s_Engine, 0, forward.x, forward.y, forward.z);
    ma_engine_listener_set_world_up(&s_Engine, 0, up.x, up.y, up.z);
}

void AudioEngine::SetSoundPosition3D(SoundHandle handle, const glm::vec3& position) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_set_position(&it->second->Sound, position.x, position.y, position.z);
    ma_sound_set_spatialization_enabled(&it->second->Sound, MA_TRUE);
}

void AudioEngine::SetSoundAttenuation(SoundHandle handle, float minDistance, float maxDistance) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_set_min_distance(&it->second->Sound, minDistance);
    ma_sound_set_max_distance(&it->second->Sound, maxDistance);
}

bool AudioEngine::IsSoundPlaying(SoundHandle handle) {
    auto it = s_Sounds.find(handle);
    return it != s_Sounds.end() && ma_sound_is_playing(&it->second->Sound);
}

void AudioEngine::SetMasterVolume(float volume) {
    if (!s_Initialized) return;
    ma_engine_set_volume(&s_Engine, volume);
}

} // namespace kizuri
