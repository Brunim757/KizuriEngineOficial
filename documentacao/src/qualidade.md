---
title: Qualidade gráfica
group: Renderização
order: 3
---

# Qualidade gráfica

Todas as configurações gráficas vivem em um único struct
(`GraphicsSettings`) e podem ser ajustadas em **Configurações → Gráficos**
(<kbd>Ctrl+,</kbd>), aplicadas **em tempo real**, sem reiniciar.

## Presets

| Preset | Uso |
|--------|-----|
| **Ultra** | Máximo — para PC com GL 4.0+ |
| **High** | Balanceado |
| **Medium** | Consumo moderado |
| **Low** | Máquina fraca / integrada |
| **Custom** | Você ajustou manualmente (mostrado como Custom) |

## Referência das opções

| Opção | Faixa | Descrição |
|-------|-------|-----------|
| **Resolução interna** | 0.25 – 2.0 | Fração do alvo; <1 = mais barato + upscale; >1 = supersampling |
| **MSAA** | 0/1/2/4/8 | Amostras do framebuffer HDR |
| **Shadow map (CSM)** | 512–4096 | Resolução de cada cascata |
| **PCF (raio)** | 0–3 | Vizinhança do PCF (3.3 e 4.0+) |
| **PCSS (suavidade)** | 0–1 | Largura da penumbra (4.0+) |
| **Shadow map pontual** | 256–2048 | Cubemap da luz pontual (4.0+) |
| **Bloom** | on/off | Bright-pass + blur |
| **Limiar do bloom** | 0.1–10 | Brilho mínimo que "glowa" |
| **Intensidade do bloom** | 0–3 | Força do brilho |
| **Iterações do bloom** | 1–12 | Glow mais largo |
| **Tonemapping** | ACES/Reinhard/Filmic | Mapeamento HDR→LDR |
| **SSAO** | on/off | Oclusão de ambiente |
| **Amostras SSAO** | 8–64 | Qualidade da oclusão |
| **Raio SSAO** | 0.05–2 | Alcance da oclusão |
| **Exposição** | 0.1–8 | Multiplicador pré-tonemap (EV) |
| **VSync** | on/off | Sincronização vertical |
| **Névoa** | on/off | Fog exponencial |
| **Densidade da névoa** | 0–0.2 | Espessura |
| **Vinheta / Aberração / Grão** | — | Pós-cinema (composite) |

## TuneToHardware()

No início, a engine ajusta os padrões pelo **hardware**:

- GL 3.3 → conservador (MSAA 4x, SSAO com teto seguro);
- GL 4.0+ → mais agressivo (MSAA 8x, PCSS ligado);
- GL 4.3+/4.5+ → ainda mais (sombras e SSAO maiores).

Depois, o `settings.json` (ao lado do executável) **sobrescreve** os padrões
com as escolhas do usuário.

::: info
**Tudo roda em 3.3 core como mínimo.** MSAA, SSAO, bloom, CSM e PBR forward
são todos compatíveis com GL 3.3. As exceções (PCSS, sombra de ponto) são
tratadas pelo caminho version-gated — veja [Pipeline 3D](pipeline-3d.html).
:::

## Dicas de performance

- **Resolução interna 0.5–0.75** é o maior ganho em máquinas fracas;
- **SSAO** e **bloom** são os pós que mais custam — desligue no Low;
- **Shadow map 2048** é o equilíbrio; 4096 é para close-ups;
- **PCSS + PCF** aumentam o custo da sombra — reduza se houver quedas.
