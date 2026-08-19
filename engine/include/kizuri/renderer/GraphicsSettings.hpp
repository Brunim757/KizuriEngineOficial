#pragma once
#include <string>

namespace kizuri {



enum class QualityPreset { Ultra = 0, High, Medium, Low, Custom };






struct GraphicsSettings {
    QualityPreset Preset = QualityPreset::Ultra;

    
    
    float RenderScale = 1.0f;

    
    int MSAA = 4;

    
    int ShadowMapSize = 2048;

    
    int ShadowPCFRadius = 2;

    
    
    
    
    
    float ShadowSoftness = 0.6f;

    bool BloomEnabled = true;
    float BloomThreshold = 1.2f;
    float BloomIntensity = 0.45f;

    
    bool SSAOEnabled = true;
    int SSAOSamples = 32;
    float SSAORadius = 0.5f;

    
    
    
    
    bool SSREnabled = true;
    int SSRMaxSteps = 24;        
    float SSRThickness = 0.12f;  
    float SSRIntensity = 0.6f;   
    float SSRMarchDistance = 20.0f; 

    
    
    
    bool TAAEnabled = true;

    
    
    
    bool GodRaysEnabled = false;
    float GodRaysIntensity = 0.6f;

    
    
    bool DOFEnabled = false;
    float DOFFocusDistance = 10.0f; 
    float DOFFocusRange = 4.0f;     
    float DOFStrength = 1.0f;       

    
    
    
    bool MotionBlurEnabled = false;
    float MotionBlurIntensity = 0.5f;

    

    
    
    
    bool SSGIEnabled = false;
    float SSGIIntensity = 0.4f;

    
    bool CloudsEnabled = false;

    
    
    bool LensFlareEnabled = false;
    float LensFlareIntensity = 0.6f;

    
    bool FXAAEnabled = false;

    
    float Saturation = 1.0f;
    float Contrast = 1.0f;

    
    
    float BloomAnamorphic = 0.0f;

    
    
    float FogHeight = 0.0f;
    float FogHeightFalloff = 0.0f;

    
    
    
    bool AtmosphereSky = false;

    
    float Exposure = 1.0f;

    
    bool FogEnabled = false;
    float FogDensity = 0.012f;
    float FogColor[3] = { 0.55f, 0.6f, 0.66f };

    
    
    float Vignette = 0.2f;
    float ChromaticAberration = 0.0015f;
    float FilmGrain = 0.012f;

    
    int BloomIterations = 4;

    
    
    int ToneMapping = 0;

    bool VSync = true;

    
    
    
    void TuneToHardware();

    void ApplyPreset(QualityPreset preset);
    void Clamp();
};


bool SaveGraphicsSettings(const std::string& path, const GraphicsSettings& settings);
bool LoadGraphicsSettings(const std::string& path, GraphicsSettings& out);

} 
