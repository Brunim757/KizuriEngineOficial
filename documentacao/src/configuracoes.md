---
title: Configurações
group: Editor
order: 5
---

# Configurações

Abra com <kbd>Ctrl+,</kbd> (ou menu **Exibir → Configurações**). A janela tem
três seções em uma sidebar.

## Gráficos

Todas as opções do pipeline — veja o detalhamento completo em
[Qualidade gráfica](qualidade.html). Resumo:

| Item | O que faz |
|------|-----------|
| **Qualidade** | Preset Ultra / High / Medium / Low (ou Custom) |
| **Resolução interna** | Fração do alvo; <1 renderiza mais barato (upscale) |
| **MSAA** | 0/1/2/4/8 amostras do framebuffer HDR |
| **Shadow map (CSM)** | 512–4096 por cascata |
| **PCF / PCSS** | Suavização de sombra (raio / largura da penumbra) |
| **Shadow map pontual** | Resolução do cubemap da luz pontual |
| **Bloom** | Ligar/desligar, limiar, intensidade, iterações |
| **Tonemapping** | ACES / Reinhard / Filmic |
| **SSAO** | Ligar/desligar, amostras, raio |
| **Exposição** | Multiplicador pré-tonemap (EV) |
| **VSync** | Sincronização vertical |
| **Névoa** | Fog exponencial (densidade) |

## Geral

Configurações do projeto e da janela.

## Editor

Preferências de uso do editor.

::: info
Os ajustes gráficos são aplicados **em tempo real**, sem reiniciar o editor —
mude o preset e veja o viewport reagir na hora.
:::

## Salvo onde?

As configurações gráficas são salvas em `settings.json` ao lado do executável.
O valor de `TuneToHardware()` define padrões pelo hardware (3.3 conservador,
4.0+ agressivo) e o arquivo sobrescreve.
