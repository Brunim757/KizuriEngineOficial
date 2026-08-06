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

    // PCSS (Percentage-Closer Soft Shadows) — sombras suaves de verdade com
    // penumbra proporcional ao tamanho do bloqueador. Reimplementado com
    // loops de PASSOS FIXOS (teto constante no shader) — 100% seguro em GLSL
    // 330 core (o PCSS antigo usava loops dinâmicos e só rodava em 4.x).
    // 0..1 = largura da penumbra (0 = desligado, PCF simples).
    float ShadowSoftness = 0.6f;

    bool BloomEnabled = true;
    float BloomThreshold = 1.2f;
    float BloomIntensity = 0.45f;

    // Oclusão de ambiente em espaço de tela (hemisfério amostrado no depth).
    bool SSAOEnabled = true;
    int SSAOSamples = 32;
    float SSAORadius = 0.5f;

    // Reflexos em espaço de tela (SSR): marcha o raio refletido contra o depth
    // buffer num loop de PASSOS FIXOS (constante no shader) — 100% seguro em
    // GLSL 330 core. O SSR antigo usava loop de comprimento VARIÁVEL (só 4.x)
    // e quebrava em 3.3/Wine; com teto constante ele compila em qualquer driver.
    bool SSREnabled = true;
    int SSRMaxSteps = 24;        // passos máximos do raio (clampado no teto do shader)
    float SSRThickness = 0.12f;  // espessura do depth pro raio "acertar"
    float SSRIntensity = 0.6f;   // força da reflexão
    float SSRMarchDistance = 20.0f; // distância máxima da marcha (unidades de mundo)

    // Anti-aliasing temporal (TAA): jitter da câmera (sequência Halton) por
    // frame + mistura com o histórico anterior com clamp de vizinhança —
    // tira o aliasing/estrelinhas das silhuetas (100% 3.3, passe fullscreen).
    bool TAAEnabled = true;

    // God rays / luz volumétrica em espaço de tela: marcha radial do pixel até
    // a posição do sol na tela, acumulando a cor brilhante da cena (passos
    // FIXOS, constante no shader — GLSL 330-safe).
    bool GodRaysEnabled = false;
    float GodRaysIntensity = 0.6f;

    // Depth of field (bokeh, 1 passe): blur por círculo de confusão calculado
    // da distância ao plano focal. Passos FIXOS (teto constante no shader).
    bool DOFEnabled = false;
    float DOFFocusDistance = 10.0f; // distância do plano em foco (mundo)
    float DOFFocusRange = 4.0f;     // metade da faixa em foco (fora = desfoca)
    float DOFStrength = 1.0f;       // força do desfoque (raio do bokeh)

    // Motion blur por REPROJEÇÃO (sem velocity buffer): reconstrói o mundo do
    // depth e projeta com a VP do frame anterior — blur linear ao longo do
    // vetor de movimento. Passos FIXOS no shader.
    bool MotionBlurEnabled = false;
    float MotionBlurIntensity = 0.5f;

    // ---- v0.25 — mais do que o 3.3 aguenta --------------------------------

    // SSGI (iluminação global em espaço de tela): raios de hemisfério marchados
    // contra o depth (como o SSR) que coletam a cor indireta da cena. Loop de
    // passos FIXOS, meia resolução.
    bool SSGIEnabled = false;
    float SSGIIntensity = 0.4f;

    // Nuvens volumétricas no céu atmosférico (raymarch com fbm 3D, passos fixos).
    bool CloudsEnabled = false;

    // Lens flare em espaço de tela (ghosts a partir do brilho do bloom, na
    // direção do sol).
    bool LensFlareEnabled = false;
    float LensFlareIntensity = 0.6f;

    // FXAA (AA de pós-processamento) — alternativa/complemento ao TAA.
    bool FXAAEnabled = false;

    // Color grading no composite: saturação e contraste (pós-tonemap).
    float Saturation = 1.0f;
    float Contrast = 1.0f;

    // Bloom anamórfico: os raios horizontais do blur são alongados (estilo
    // cinema, 0 = circular normal).
    float BloomAnamorphic = 0.0f;

    // Névoa por altura: a névoa exponencial fica mais forte abaixo da altura
    // FogHeight e some acima (0 = desligado, aplicado só se FogEnabled).
    float FogHeight = 0.0f;
    float FogHeightFalloff = 0.0f;

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
