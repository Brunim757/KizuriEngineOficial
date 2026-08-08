---
title: Qualidade e presets
group: Gráficos
order: 1
---

# Qualidade e presets

A Kizuri Engine roda 100% em **OpenGL 3.3** e entrega gráficos modernos sem
precisar de placas especiais. Tudo é controlado por **presets** e ajustes em
**Project Settings → Gráficos**.

## Presets

| Preset | Para quem |
|---|---|
| **Ultra** | Máquinas boas: todas as features ligadas (reflexos, SSGI, nuvens...) |
| **High** | Equilíbrio entre visual e desempenho |
| **Medium** | Máquinas modestas (preset padrão) |
| **Low** | Qualquer coisa: quase tudo desligado, render mais barato |
| **Custom** | Quando você muda algo manualmente (o preset vira Custom) |

::: nota
**DOF** e **motion blur** não vêm ligados nos presets High/Ultra — o foco
fixo borra demais a câmera livre do editor. Ative pelas caixas em
Project Settings → Gráficos se quiser o visual cinematográfico.
:::

## O que cada grupo controla

- **Resolução interna** — renderiza numa resolução menor e amplia (mais FPS)
- **MSAA** — suaviza bordas (1x, 2x, 4x, 8x)
- **Sombras** — tamanho do shadow map e suavidade (PCSS)
- **Iluminação** — SSAO (oclusão) e SSGI (luz indireta)
- **Reflexos** — SSR (reflexos em espaço de tela) e espelhos
- **Anti-aliasing** — TAA (temporal) e FXAA
- **Atmosfera** — god rays, nuvens, lens flare
- **Pós-processamento** — bloom, DOF, motion blur, exposição, tonemapping, cor
- **Céu** — físico (Rayleigh/Mie) ou HDRI

## Aplicado ao vivo

Todos os ajustes são aplicados **em tempo real**, sem reiniciar o editor, e
ficam salvos em `settings.json` ao lado do projeto.

::: dica
Encontrou FPS baixo? Comece baixando o **preset** para Medium — o maior
impacto no desempenho vem de MSAA, reflexos e SSGI.
:::

Veja as páginas detalhadas: [Reflexos](reflexos.html), [Iluminação](iluminacao.html),
[Céu](ceu.html), [Sombras](sombras.html) e [Pós-processamento](pos-processamento.html).
