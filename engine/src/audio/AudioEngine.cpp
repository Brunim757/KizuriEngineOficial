#include "kizuri/audio/AudioEngine.hpp"
#include "kizuri/core/Log.hpp"

#define MINIAUDIO_IMPLEMENTATION
#include <miniaudio.h>

#include <unordered_map>
#include <vector>
#include <memory>

namespace kizuri {

struct LoadedSound {
    ma_sound Sound;

    ~LoadedSound() { ma_sound_uninit(&Sound); }
};

static ma_engine s_Engine;
static bool s_Initialized = false;
static std::unordered_map<SoundHandle, std::unique_ptr<LoadedSound>> s_Sounds;
static std::unordered_map<std::string, SoundHandle> s_NameToHandle;
static SoundHandle s_NextHandle = 1;

static float s_GroupVolumes[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

struct ReverbChannelState {
    std::vector<float> CombBuf[4];
    std::vector<float> AllpassBuf[2];
    size_t CombPos[4] = {};
    size_t AllpassPos[2] = {};
    float LowpassState = 0.0f;

    void Init(uint32_t sampleRate) {

        const float combMs[4] = { 29.7f, 37.1f, 41.1f, 43.7f };
        const float apMs[2] = { 5.0f, 1.7f };
        for (int i = 0; i < 4; ++i) CombBuf[i].assign((size_t)(combMs[i] * sampleRate / 1000.0f), 0.0f);
        for (int i = 0; i < 2; ++i) AllpassBuf[i].assign((size_t)(apMs[i] * sampleRate / 1000.0f), 0.0f);
    }

    float Process(float input, float feedback, float damp) {
        float acc = 0.0f;
        for (int i = 0; i < 4; ++i) {
            size_t len = CombBuf[i].size();
            if (len == 0) continue;
            float delayed = CombBuf[i][CombPos[i]];

            LowpassState = delayed * (1.0f - damp) + LowpassState * damp;
            CombBuf[i][CombPos[i]] = input + LowpassState * feedback;
            CombPos[i] = (CombPos[i] + 1) % len;
            acc += delayed;
        }
        acc *= 0.25f;
        for (int i = 0; i < 2; ++i) {
            size_t len = AllpassBuf[i].size();
            if (len == 0) continue;
            float bufOut = AllpassBuf[i][AllpassPos[i]];
            float out = -input + bufOut;
            AllpassBuf[i][AllpassPos[i]] = input + bufOut * 0.5f;
            AllpassPos[i] = (AllpassPos[i] + 1) % len;
            input = out;
        }
        return acc;
    }
};

struct ReverbNode {
    ma_node_base base;
    ma_node* pNode = nullptr;
    std::vector<ReverbChannelState> Channels;
    float Wet = 0.0f;
    float Room = 0.5f;
    float Damp = 0.3f;
    uint32_t ChannelsCount = 2;
};

static void ReverbProcess(ma_node* pNode, const float** ppFramesIn, ma_uint32* pFrameCountIn,
                          float** ppFramesOut, ma_uint32* pFrameCountOut) {
    ReverbNode* reverb = pNode ? (ReverbNode*)pNode : nullptr;
    if (!reverb || !ppFramesIn || !ppFramesOut || !pFrameCountIn || !pFrameCountOut) return;
    ma_uint32 frames = (*pFrameCountIn < *pFrameCountOut) ? *pFrameCountIn : *pFrameCountOut;
    const float* in = ppFramesIn[0];
    float* out = ppFramesOut[0];
    uint32_t chs = reverb->ChannelsCount;
    for (ma_uint32 f = 0; f < frames; ++f) {
        for (uint32_t c = 0; c < chs; ++c) {
            float x = in[f * chs + c];
            float rev = 0.0f;
            if (c < reverb->Channels.size())
                rev = reverb->Channels[c].Process(x, 0.7f + reverb->Room * 0.28f, reverb->Damp);
            out[f * chs + c] = x + rev * reverb->Wet;
        }
    }
    *pFrameCountOut = frames;
}

static ma_node_vtable s_ReverbVTable = {
    ReverbProcess,
    NULL,
    1,
    1,
    MA_NODE_FLAG_CONTINUOUS_PROCESSING | MA_NODE_FLAG_ALLOW_NULL_INPUT,
};

static ReverbNode* s_Reverb = nullptr;
static bool s_ReverbNodeReady = false;
static std::unordered_map<SoundHandle, float> s_ReverbWet;

static void EnsureReverbNode() {
    if (s_ReverbNodeReady || !s_Initialized) return;
    ma_uint32 chs = ma_engine_get_channels(&s_Engine);
    if (chs == 0) chs = 2;

    auto* data = new (std::nothrow) ReverbNode();
    if (!data) return;
    data->ChannelsCount = chs;
    data->Channels.resize(chs < 2 ? chs : 2);
    for (auto& ch : data->Channels) ch.Init(ma_engine_get_sample_rate(&s_Engine));

    ma_uint32 inChs[1] = { chs };
    ma_uint32 outChs[1] = { chs };
    ma_node_config cfg = ma_node_config_init();
    cfg.vtable = &s_ReverbVTable;
    cfg.pInputChannels = inChs;
    cfg.pOutputChannels = outChs;

    ma_node_graph* graph = ma_engine_get_node_graph(&s_Engine);

    ma_node* node = (ma_node*)data;
    if (ma_node_init(graph, &cfg, nullptr, node) != MA_SUCCESS) {
        delete data;
        return;
    }
    data->pNode = node;

    ma_node_attach_output_bus(node, 0, ma_node_graph_get_endpoint(graph), 0);
    s_Reverb = data;
    s_ReverbNodeReady = true;
    KZ_CORE_INFO("Áudio: nó de reverb inserido no grafo ({0} canais).", chs);
}

static void RouteSoundThroughReverb(ma_sound* sound, bool enabled, float wet) {
    if (!sound) return;
    if (s_ReverbNodeReady && enabled && s_Reverb && s_Reverb->pNode) {
        ma_node_detach_output_bus((ma_node*)sound, 0);
        ma_node_attach_output_bus((ma_node*)sound, 0, s_Reverb->pNode, 0);
    } else {

        ma_node_detach_output_bus((ma_node*)sound, 0);
        ma_node_attach_output_bus((ma_node*)sound, 0, ma_node_graph_get_endpoint(ma_engine_get_node_graph(&s_Engine)), 0);
    }
    (void)wet;
}

struct OneShotSound {
    ma_sound Sound;
    bool InUse = false;
};
static std::vector<OneShotSound> s_OneShots;

void AudioEngine::Init() {
    ma_result result = ma_engine_init(nullptr, &s_Engine);
    if (result != MA_SUCCESS) {
        KZ_CORE_ERROR("Falha ao inicializar o motor de áudio (miniaudio).");
        return;
    }
    s_Initialized = true;
    KZ_CORE_INFO("Motor de áudio inicializado (miniaudio).");
}

void AudioEngine::Shutdown() {
    if (!s_Initialized) return;
    if (s_ReverbNodeReady && s_Reverb && s_Reverb->pNode) {
        ma_node_uninit(s_Reverb->pNode, NULL);
        delete s_Reverb;
        s_Reverb = nullptr;
        s_ReverbNodeReady = false;
        s_ReverbWet.clear();
    }
    s_Sounds.clear();
    for (auto& os : s_OneShots)
        if (os.InUse) { ma_sound_uninit(&os.Sound); os.InUse = false; }
    s_OneShots.clear();
    ma_engine_uninit(&s_Engine);
    s_Initialized = false;
}

SoundHandle AudioEngine::LoadSound(const std::string& name, const std::string& path, bool stream) {
    if (!s_Initialized) return kInvalidSound;

    auto loaded = std::make_unique<LoadedSound>();
    ma_uint32 flags = stream ? MA_SOUND_FLAG_STREAM : 0;
    ma_result result = ma_sound_init_from_file(&s_Engine, path.c_str(), flags, nullptr, nullptr, &loaded->Sound);
    if (result != MA_SUCCESS) {
        KZ_CORE_ERROR("Falha ao carregar o áudio '{0}' em '{1}'.", name, path);
        return kInvalidSound;
    }

    SoundHandle handle = s_NextHandle++;
    s_Sounds[handle] = std::move(loaded);
    s_NameToHandle[name] = handle;
    KZ_CORE_INFO("Áudio carregado: {0} ({1}).", name, path);
    return handle;
}

void AudioEngine::Play(SoundHandle handle, bool loop, float volume, int group) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_set_looping(&it->second->Sound, loop ? MA_TRUE : MA_FALSE);
    ma_sound_set_volume(&it->second->Sound, volume * GetGroupVolume(group));
    ma_sound_start(&it->second->Sound);
}

void AudioEngine::PlayOneShot(const std::string& path, float volume, int group) {
    if (!s_Initialized) return;
    ma_engine_play_sound(&s_Engine, path.c_str(), nullptr);
    (void)volume; (void)group;
}

void AudioEngine::PlayOneShotAt(const std::string& path, float volume, const glm::vec3& position, int group) {
    if (!s_Initialized) return;

    OneShotSound* slot = nullptr;
    for (auto& s : s_OneShots) {
        if (!s.InUse) {
            slot = &s;
            ma_sound_uninit(&slot->Sound);
            break;
        }
    }
    if (!slot) {
        s_OneShots.emplace_back();
        slot = &s_OneShots.back();
    }

    if (ma_sound_init_from_file(&s_Engine, path.c_str(), 0, nullptr, nullptr, &slot->Sound) != MA_SUCCESS) {
        slot->InUse = false;
        return;
    }
    slot->InUse = true;
    ma_sound_set_position(&slot->Sound, position.x, position.y, position.z);
    ma_sound_set_volume(&slot->Sound, volume * GetGroupVolume(group));
    ma_sound_set_end_callback(&slot->Sound, [](void* pUserData, ma_sound*) {
        static_cast<OneShotSound*>(pUserData)->InUse = false;
    }, slot);
    ma_sound_start(&slot->Sound);
}

void AudioEngine::Stop(SoundHandle handle) {
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    ma_sound_stop(&it->second->Sound);
}

void AudioEngine::StopAll() {
    if (!s_Initialized) return;

    for (auto& os : s_OneShots) {
        if (os.InUse) { ma_sound_stop(&os.Sound); os.InUse = false; }
    }

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

void AudioEngine::SetGroupVolume(int group, float volume) {
    if (group < 0 || group > 3) return;
    s_GroupVolumes[group] = volume < 0.0f ? 0.0f : volume;
}
float AudioEngine::GetGroupVolume(int group) {
    if (group < 0 || group > 3) return 1.0f;
    return s_GroupVolumes[group];
}

void AudioEngine::SetGlobalReverb(float wet, float roomSize, float damp) {
    if (!s_Initialized) return;
    wet = glm::clamp(wet, 0.0f, 1.0f);
    if (wet <= 0.001f) {

        for (auto& [handle, w] : s_ReverbWet) {
            auto it = s_Sounds.find(handle);
            if (it != s_Sounds.end()) RouteSoundThroughReverb(&it->second->Sound, false, 0.0f);
        }
        s_ReverbWet.clear();
        if (s_Reverb) s_Reverb->Wet = 0.0f;
        return;
    }
    EnsureReverbNode();
    if (!s_Reverb) return;
    s_Reverb->Wet = wet;
    s_Reverb->Room = glm::clamp(roomSize, 0.0f, 1.0f);
    s_Reverb->Damp = glm::clamp(damp, 0.0f, 1.0f);
}

void AudioEngine::SetSoundReverb(SoundHandle handle, bool enabled, float wet) {
    if (!s_Initialized) return;
    auto it = s_Sounds.find(handle);
    if (it == s_Sounds.end()) return;
    if (!enabled || wet <= 0.001f) {
        s_ReverbWet.erase(handle);
        RouteSoundThroughReverb(&it->second->Sound, false, 0.0f);
        return;
    }
    EnsureReverbNode();
    if (!s_Reverb) return;
    s_ReverbWet[handle] = wet;
    RouteSoundThroughReverb(&it->second->Sound, true, wet);
}

bool AudioEngine::IsSoundReverbing(SoundHandle handle) {
    return s_ReverbWet.find(handle) != s_ReverbWet.end();
}

}
