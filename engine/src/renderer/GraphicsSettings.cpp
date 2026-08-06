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
    ShadowSoftness = 0.6f;
    GodRaysEnabled = false; GodRaysIntensity = 0.6f;
    DOFEnabled = false; DOFFocusDistance = 10.0f; DOFFocusRange = 4.0f; DOFStrength = 1.0f;
    MotionBlurEnabled = false; MotionBlurIntensity = 0.5f;
    SSREnabled = true; SSRMaxSteps = 24; SSRThickness = 0.12f; SSRIntensity = 0.6f;
    SSRMarchDistance = 20.0f;
    TAAEnabled = true;
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
    ShadowSoftness = std::clamp(ShadowSoftness, 0.0f, 1.0f);
    BloomThreshold = std::clamp(BloomThreshold, 0.1f, 10.0f);
    BloomIntensity = std::clamp(BloomIntensity, 0.0f, 3.0f);
    SSAOSamples = std::clamp(SSAOSamples, 8, 64);
    SSAORadius = std::clamp(SSAORadius, 0.05f, 2.0f);
    SSRMaxSteps = std::clamp(SSRMaxSteps, 8, 48);
    SSRThickness = std::clamp(SSRThickness, 0.01f, 1.0f);
    SSRIntensity = std::clamp(SSRIntensity, 0.0f, 2.0f);
    SSRMarchDistance = std::clamp(SSRMarchDistance, 1.0f, 200.0f);
    GodRaysIntensity = std::clamp(GodRaysIntensity, 0.0f, 3.0f);
    DOFFocusDistance = std::clamp(DOFFocusDistance, 0.1f, 1000.0f);
    DOFFocusRange = std::clamp(DOFFocusRange, 0.1f, 200.0f);
    DOFStrength = std::clamp(DOFStrength, 0.0f, 5.0f);
    MotionBlurIntensity = std::clamp(MotionBlurIntensity, 0.0f, 2.0f);
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
            ShadowSoftness = 0.7f;
            GodRaysEnabled = true; GodRaysIntensity = 0.7f;
            DOFEnabled = true; DOFFocusDistance = 10.0f; DOFFocusRange = 5.0f; DOFStrength = 1.2f;
            MotionBlurEnabled = true; MotionBlurIntensity = 0.5f;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.45f;
            SSAOEnabled = true;   SSAOSamples = 64;      SSAORadius = 0.5f;
            SSREnabled = true;    SSRMaxSteps = 32;      SSRIntensity = 0.7f;
            SSRThickness = 0.12f; SSRMarchDistance = 28.0f;
            TAAEnabled = true;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.22f; ChromaticAberration = 0.0015f; FilmGrain = 0.015f;
            BloomIterations = 8;
            break;
        case QualityPreset::High:
            RenderScale = 1.0f; MSAA = 4; ShadowMapSize = 2048; ShadowPCFRadius = 2;
            ShadowSoftness = 0.6f;
            GodRaysEnabled = true; GodRaysIntensity = 0.6f;
            DOFEnabled = true; DOFFocusDistance = 10.0f; DOFFocusRange = 4.0f; DOFStrength = 1.0f;
            MotionBlurEnabled = true; MotionBlurIntensity = 0.4f;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.45f;
            SSAOEnabled = true;   SSAOSamples = 32;      SSAORadius = 0.5f;
            SSREnabled = true;    SSRMaxSteps = 24;      SSRIntensity = 0.6f;
            SSRThickness = 0.12f; SSRMarchDistance = 20.0f;
            TAAEnabled = true;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.2f; ChromaticAberration = 0.0012f; FilmGrain = 0.012f;
            BloomIterations = 5;
            break;
        case QualityPreset::Medium:
            RenderScale = 1.0f; MSAA = 2; ShadowMapSize = 1024; ShadowPCFRadius = 1;
            ShadowSoftness = 0.5f;
            GodRaysEnabled = false; GodRaysIntensity = 0.4f;
            DOFEnabled = false; DOFFocusDistance = 10.0f; DOFFocusRange = 4.0f; DOFStrength = 0.8f;
            MotionBlurEnabled = false; MotionBlurIntensity = 0.3f;
            BloomEnabled = true;  BloomThreshold = 1.2f; BloomIntensity = 0.35f;
            SSAOEnabled = true;   SSAOSamples = 16;      SSAORadius = 0.5f;
            SSREnabled = true;    SSRMaxSteps = 16;      SSRIntensity = 0.45f;
            SSRThickness = 0.14f; SSRMarchDistance = 16.0f;
            TAAEnabled = true;
            Exposure = 1.0f;      VSync = true;
            FogEnabled = false;   FogDensity = 0.012f;
            FogColor[0] = 0.55f;  FogColor[1] = 0.6f;  FogColor[2] = 0.66f;
            Vignette = 0.15f; ChromaticAberration = 0.0008f; FilmGrain = 0.008f;
            BloomIterations = 3;
            break;
        case QualityPreset::Low:
            RenderScale = 0.75f; MSAA = 1; ShadowMapSize = 1024; ShadowPCFRadius = 0;
            ShadowSoftness = 0.0f;
            GodRaysEnabled = false; GodRaysIntensity = 0.0f;
            DOFEnabled = false; DOFFocusDistance = 10.0f; DOFFocusRange = 4.0f; DOFStrength = 0.0f;
            MotionBlurEnabled = false; MotionBlurIntensity = 0.0f;
            BloomEnabled = false; BloomThreshold = 1.2f; BloomIntensity = 0.0f;
            SSAOEnabled = false;  SSAOSamples = 8;       SSAORadius = 0.5f;
            SSREnabled = false;   SSRMaxSteps = 8;       SSRIntensity = 0.0f;
            SSRThickness = 0.16f; SSRMarchDistance = 10.0f;
            TAAEnabled = false;
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
    j["shadow_softness"] = settings.ShadowSoftness;
    j["god_rays_enabled"] = settings.GodRaysEnabled;
    j["god_rays_intensity"] = settings.GodRaysIntensity;
    j["dof_enabled"] = settings.DOFEnabled;
    j["dof_focus_distance"] = settings.DOFFocusDistance;
    j["dof_focus_range"] = settings.DOFFocusRange;
    j["dof_strength"] = settings.DOFStrength;
    j["motion_blur_enabled"] = settings.MotionBlurEnabled;
    j["motion_blur_intensity"] = settings.MotionBlurIntensity;
    j["bloom_enabled"] = settings.BloomEnabled;
    j["bloom_threshold"] = settings.BloomThreshold;
    j["bloom_intensity"] = settings.BloomIntensity;
    j["ssao_enabled"] = settings.SSAOEnabled;
    j["ssao_samples"] = settings.SSAOSamples;
    j["ssao_radius"] = settings.SSAORadius;
    j["ssr_enabled"] = settings.SSREnabled;
    j["ssr_max_steps"] = settings.SSRMaxSteps;
    j["ssr_thickness"] = settings.SSRThickness;
    j["ssr_intensity"] = settings.SSRIntensity;
    j["ssr_march_distance"] = settings.SSRMarchDistance;
    j["taa_enabled"] = settings.TAAEnabled;
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
        out.ShadowSoftness = j.value("shadow_softness", out.ShadowSoftness);
        out.GodRaysEnabled = j.value("god_rays_enabled", out.GodRaysEnabled);
        out.GodRaysIntensity = j.value("god_rays_intensity", out.GodRaysIntensity);
        out.DOFEnabled = j.value("dof_enabled", out.DOFEnabled);
        out.DOFFocusDistance = j.value("dof_focus_distance", out.DOFFocusDistance);
        out.DOFFocusRange = j.value("dof_focus_range", out.DOFFocusRange);
        out.DOFStrength = j.value("dof_strength", out.DOFStrength);
        out.MotionBlurEnabled = j.value("motion_blur_enabled", out.MotionBlurEnabled);
        out.MotionBlurIntensity = j.value("motion_blur_intensity", out.MotionBlurIntensity);
        out.BloomEnabled = j.value("bloom_enabled", out.BloomEnabled);
        out.BloomThreshold = j.value("bloom_threshold", out.BloomThreshold);
        out.BloomIntensity = j.value("bloom_intensity", out.BloomIntensity);
        out.SSAOEnabled = j.value("ssao_enabled", out.SSAOEnabled);
        out.SSAOSamples = j.value("ssao_samples", out.SSAOSamples);
        out.SSAORadius = j.value("ssao_radius", out.SSAORadius);
        out.SSREnabled = j.value("ssr_enabled", out.SSREnabled);
        out.SSRMaxSteps = j.value("ssr_max_steps", out.SSRMaxSteps);
        out.SSRThickness = j.value("ssr_thickness", out.SSRThickness);
        out.SSRIntensity = j.value("ssr_intensity", out.SSRIntensity);
        out.SSRMarchDistance = j.value("ssr_march_distance", out.SSRMarchDistance);
        out.TAAEnabled = j.value("taa_enabled", out.TAAEnabled);
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
