---
title: Project Settings
group: Editor
order: 5
---

# Project Settings

O painel **Project Settings** concentra as configurações do projeto, com
seções na lateral.

## Gráficos

Comece por um **preset** (Ultra, High, Medium, Low) e ajuste o que quiser:

- **Resolução interna** (render scale) e **MSAA**
- **Sombras**: tamanho do shadow map (CSM), PCF e **PCSS** (penumbra)
- **Iluminação**: SSAO, SSR (reflexos), **SSGI** (iluminação global)
- **Anti-aliasing**: **TAA** e **FXAA**
- **Atmosfera**: God rays, **nuvens volumétricas**, **lens flare**
- **Pós**: bloom, bloom anamórfico, **DOF**, **motion blur**, exposição,
  tonemapping, saturação/contraste, vinheta e névoa
- **HDRI do céu** — vazio = gradiente procedural; `.hdr` = ambiente;
  "Céu atmosférico Rayleigh/Mie" liga o céu físico (raymarch, opcional)

::: dica
Tudo é aplicado **ao vivo** e salvo em `settings.json` — não precisa reiniciar.
:::

## Geral

- **Janela**: largura/altura, maximizar/minimizar, VSync
- **Informações do OpenGL**: fabricante, renderer e versão (útil para
  reportar problemas)

## Editor

Preferências de uso:

- **Velocidade da câmera livre** do viewport
- **Sensibilidade do mouse**
- **Snap** de translação e de rotação dos gizmos
- Compilar C# automaticamente antes do **Play**
- Desenhar colisores (overlay de física)

## Sobre

Versão da engine e um resumo das capacidades.

::: nota
A janela **Configurações** (menu Exibir) mostra a versão completa dos mesmos
ajustes. O Project Settings é a versão dockável e rápida.
:::
