#pragma once
#include <string>

namespace kizuri {

// Preset de qualidade gráfica — a engine aplica todos os valores por cima
// do que o hardware suporta (ex.: MSAA >1 com GL3.3 mínimo).
enum class QualityPreset { Ultra = 0, High, Medium, Low, Custom };

// Todas as configurações gráficas da engine. Vive num único struct para
// poder ser salvo/carregado em settings.json (editor) e aplicado em runtime
// sem reiniciar nada — cada passe do pipeline lê o valor atual no início
// do frame. Tudo aqui roda em OpenGL 3.3 core como mínimo (MSAA, SSAO,
// bloom, CSM e PBR forward são todos 3.3-compatíveis).
struct GraphicsSettings {
    QualityPreset Preset = QualityPreset::Ultra;

    // Resolução interna (fração da resolução do destino). <1 = renderiza
    // mais barato e dá upscale no passe de composição; >1 = supersampling.
    float RenderScale = 1.0f;

    // Amostras de MSAA do framebuffer HDR: 0/1 = desligado, 2/4/8 = ligado.
    int MSAA = 4;

    // Resolução de cada cascata do shadow map (CSM).
    int ShadowMapSize = 2048;

    // Raio do PCF do shadow map (0 = amostra única, 1..3 = vizinhança).
    int ShadowPCFRadius = 2;

    // PCSS (sombras suaves): quanto maior, mais larga a penumbra (0..1).
    float ShadowSoftness = 0.5f;

    // Resolução do shadow map da luz pontual (depth cubemap).
    int PointShadowMapSize = 512;

    bool BloomEnabled = true;
    float BloomThreshold = 1.2f;
    float BloomIntensity = 0.45f;

    // Oclusão de ambiente em espaço de tela (hemisfério amostrado no depth).
    bool SSAOEnabled = true;
    int SSAOSamples = 32;
    float SSAORadius = 0.5f;

    // Exposição multiplicada antes do tonemap ACES (equivalente ao "EV").
    float Exposure = 1.0f;

    // Névoa exponencial por distância (aplicada no shader de mesh).
    bool FogEnabled = false;
    float FogDensity = 0.012f;
    float FogColor[3] = { 0.55f, 0.6f, 0.66f };

    // Pós-cinema (composite, pós-tonemap — não afeta o IBL): vinheta,
    // aberração cromática e grão de filme animado.
    float Vignette = 0.2f;
    float ChromaticAberration = 0.0015f;
    float FilmGrain = 0.012f;

    // Qualidade do bloom (iterações do blur ping-pong; mais = glow mais largo).
    int BloomIterations = 4;

    // Tonemapping: 0 = ACES (padrão, cinematográfico), 1 = Reinhard (suave),
    // 2 = Filmic (Alchemy, contraste alto). Aplicado no composite.
    int ToneMapping = 0;

    bool VSync = true;

    // Ajusta os padrões pelo HARDWARE (versão GLSL detectada): GL 3.3 fica
    // conservador, 4.0+ mais agressivo, 4.3+ e 4.5+ ainda mais. Chamado no
    // Init da engine antes de carregar settings.json (que sobrescreve).
    void TuneToHardware();

    void ApplyPreset(QualityPreset preset);
    void Clamp();
};

// Persistência (nlohmann::json, arquivo simples chave->valor).
bool SaveGraphicsSettings(const std::string& path, const GraphicsSettings& settings);
bool LoadGraphicsSettings(const std::string& path, GraphicsSettings& out);

} // namespace kizuri
