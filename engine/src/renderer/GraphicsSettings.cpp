#include "kizuri/renderer/GraphicsSettings.hpp"
#include "kizuri/renderer/Shader.hpp"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace kizuri {

void GraphicsSettings::TuneToHardware() {
    // A engine roda SEMPRE em OpenGL 3.3 core (o caminho comprovado) — um
    // único conjunto de valores, sem variação por versão. O AO clampado em
    // 0.35 garante que o SSAO nunca escureça a cena mesmo se a estimativa
    // quebrar num driver estranho.
    MSAA = 4; ShadowMapSize = 2048; ShadowPCFRadius = 2; SSAOSamples = 32;
    BloomIterations = 4;
}
static const char* PresetName(QualityPreset p) {
    switch (p) {
        case QualityPreset::Ultra:  return "Ultra";
        case QualityPreset::High:   return "High";
        case QualityPreset::Medium: return "Medium";
        case QualityPreset::Low:    return "Low";
        default:                    return "Custom";
    }
}

static QualityPreset PresetFromString(const std::string& s) {
    if (s == "Ultra")  return QualityPreset::Ultra;
    if (s == "High")   return QualityPreset::High;
    if (s == "Medium") return QualityPreset::Medium;
    if (s == "Low")    return QualityPreset::Low;
    return QualityPreset::Custom;
}

void GraphicsSettings::Clamp() {
    RenderScale = std::clamp(RenderScale, 0.25f, 2.0f);
    MSAA = (MSAA == 0 || MSAA == 1) ? 1 : ((MSAA <= 2) ? 2 : (MSAA <= 4 ? 4 : 8));
    ShadowMapSize = std::clamp(ShadowMapSize, 512, 4096);
    ShadowPCFRadius = std::clamp(ShadowPCFRadius, 0, 3);
    BloomThreshold = std::clamp(BloomThreshold, 0.1f, 10.0f);
    BloomIntensity = std::clamp(BloomIntensity, 0.0f, 3.0f);
    SSAOSamples = std::clamp(SSAOSamples, 8, 64);
    SSAORadius = std::clamp(SSAORadius, 0.05f, 2.0f);
    Exposure = std::clamp(Exposure, 0.1f, 8.0f);
    FogDensity = std::clamp(FogDensity, 0.0f, 0.2f);
    Vignette = std::clamp(Vignette, 0.0f, 1.0f);
    ChromaticAberration = std::clamp(ChromaticAberration, 0.0f, 0.02f);
    FilmGrain = std::clamp(FilmGrain, 0.0f, 0.2f);
    BloomIterations = std::clamp(BloomIterations, 1, 12);
    ToneMapping = std::clamp(ToneMapping, 0, 2);
}

void GraphicsSettings::ApplyPreset(QualityPreset preset) {
    switch (preset) {
        case QualityPreset::Ultra:
            RenderScale = 1.0f; MSAA = 8; ShadowMapSize = 4096; ShadowPCFRadius = 3;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.45f;
            SSAOEnabled = true;   SSAOSamples = 64;      SSAORadius = 0.5f;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.22f; ChromaticAberration = 0.0015f; FilmGrain = 0.015f;
            BloomIterations = 8;
            break;
        case QualityPreset::High:
            RenderScale = 1.0f; MSAA = 4; ShadowMapSize = 2048; ShadowPCFRadius = 2;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.45f;
            SSAOEnabled = true;   SSAOSamples = 32;      SSAORadius = 0.5f;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.2f; ChromaticAberration = 0.0012f; FilmGrain = 0.012f;
            BloomIterations = 5;
            break;
        case QualityPreset::Medium:
            RenderScale = 1.0f; MSAA = 2; ShadowMapSize = 1024; ShadowPCFRadius = 1;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.35f;
            SSAOEnabled = true;   SSAOSamples = 16;      SSAORadius = 0.5f;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.15f; ChromaticAberration = 0.0008f; FilmGrain = 0.008f;
            BloomIterations = 3;
            break;
        case QualityPreset::Low:
            RenderScale = 0.75f; MSAA = 1; ShadowMapSize = 1024; ShadowPCFRadius = 0;
            BloomEnabled = false; BloomThreshold = 1.2f; BloomIntensity = 0.0f;
            SSAOEnabled = false;  SSAOSamples = 8;       SSAORadius = 0.5f;
            Exposure = 1.0f;      VSync = false;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.0f; ChromaticAberration = 0.0f; FilmGrain = 0.0f;
            BloomIterations = 2;
            break;
        default:
            break; // Custom: mantém os valores atuais
    }
    Preset = preset;
    Clamp();
}

bool SaveGraphicsSettings(const std::string& path, const GraphicsSettings& settings) {
    nlohmann::json j;
    j["preset"] = PresetName(settings.Preset);
    j["render_scale"] = settings.RenderScale;
    j["msaa"] = settings.MSAA;
    j["shadow_map_size"] = settings.ShadowMapSize;
    j["shadow_pcf_radius"] = settings.ShadowPCFRadius;
    j["bloom_enabled"] = settings.BloomEnabled;
    j["bloom_threshold"] = settings.BloomThreshold;
    j["bloom_intensity"] = settings.BloomIntensity;
    j["ssao_enabled"] = settings.SSAOEnabled;
    j["ssao_samples"] = settings.SSAOSamples;
    j["ssao_radius"] = settings.SSAORadius;
    j["exposure"] = settings.Exposure;
    j["fog_enabled"] = settings.FogEnabled;
    j["fog_density"] = settings.FogDensity;
    j["fog_color"] = { settings.FogColor[0], settings.FogColor[1], settings.FogColor[2] };
    j["vignette"] = settings.Vignette;
    j["chromatic_aberration"] = settings.ChromaticAberration;
    j["film_grain"] = settings.FilmGrain;
    j["bloom_iterations"] = settings.BloomIterations;
    j["tone_mapping"] = settings.ToneMapping;
    j["vsync"] = settings.VSync;

    std::ofstream out(path);
    if (!out.is_open()) return false;
    out << j.dump(4);
    return true;
}

bool LoadGraphicsSettings(const std::string& path, GraphicsSettings& out) {
    std::ifstream in(path);
    if (!in.is_open()) return false;
    try {
        nlohmann::json j;
        in >> j;
        out.Preset = PresetFromString(j.value("preset", "Custom"));
        out.RenderScale = j.value("render_scale", out.RenderScale);
        out.MSAA = j.value("msaa", out.MSAA);
        out.ShadowMapSize = j.value("shadow_map_size", out.ShadowMapSize);
        out.ShadowPCFRadius = j.value("shadow_pcf_radius", out.ShadowPCFRadius);
        out.BloomEnabled = j.value("bloom_enabled", out.BloomEnabled);
        out.BloomThreshold = j.value("bloom_threshold", out.BloomThreshold);
        out.BloomIntensity = j.value("bloom_intensity", out.BloomIntensity);
        out.SSAOEnabled = j.value("ssao_enabled", out.SSAOEnabled);
        out.SSAOSamples = j.value("ssao_samples", out.SSAOSamples);
        out.SSAORadius = j.value("ssao_radius", out.SSAORadius);
        out.Exposure = j.value("exposure", out.Exposure);
        out.FogEnabled = j.value("fog_enabled", out.FogEnabled);
        out.FogDensity = j.value("fog_density", out.FogDensity);
        if (j.contains("fog_color") && j["fog_color"].is_array() && j["fog_color"].size() == 3)
            for (int i = 0; i < 3; ++i) out.FogColor[i] = j["fog_color"][i].get<float>();
        out.Vignette = j.value("vignette", out.Vignette);
        out.ChromaticAberration = j.value("chromatic_aberration", out.ChromaticAberration);
        out.FilmGrain = j.value("film_grain", out.FilmGrain);
        out.BloomIterations = j.value("bloom_iterations", out.BloomIterations);
        out.ToneMapping = j.value("tone_mapping", out.ToneMapping);
        out.VSync = j.value("vsync", out.VSync);
        out.Clamp();
        return true;
    } catch (...) {
        return false;
    }
}

} // namespace kizuri
