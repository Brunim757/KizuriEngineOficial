---
title: Pipeline 3D
group: Renderização
order: 1
---

# Renderer 3D

O renderer 3D é **fisicamente realista** (PBR) com pós-processamento em HDR:
luzes, céu, sombras, brilho e atmosfera.

## Qualidade automática

O motor **ajusta a qualidade sozinho conforme a sua placa de vídeo**:

- Máquinas mais fracas → efeitos mais leves, mantendo a fluidez;
- Máquinas mais fortes → tudo no máximo (sombras suaves, mais resolução).

Você pode ajustar tudo à mão em **Configurações → Gráficos** — veja
[Qualidade gráfica](qualidade.html).

## Recursos

- **PBR (físico)** — metais, plásticos, tecidos respondem à luz de forma
  realista;
- **Céu e luz ambiente (IBL)** — o ambiente ilumina os objetos;
- **Luzes dinâmicas** — até **16 por cena** (sol, pontos de luz, spots);
- **Sombras** — em cascata (acompanham a câmera), com suavização;
- **Personagens animados** — esqueletos com animações (skinning);
- **Partículas** — fogo, faíscas, magia, fumaça;
- **Bloom** — objetos brilhantes "glowam" (luz neon, fogo);
- **Tonemapping** — acabamento de filme (ACES);
- **SSAO** — contato e profundidade nos cantos;
- **Névoa** — atmosfera por distância;
- **Pós-cinema** — vinheta, aberração cromática e grão de filme.

## Céu

| Fonte | Quando |
|-------|--------|
| **HDRI** do Content Pack | preferido, se presente |
| **Céu padrão do editor** | sempre disponível |
| **Céu procedural** | fallback automático |

## Ordem de renderização da cena

```
1. Mundo 3D
2. Mundo 2D   (sprites/texto/tilemaps)
3. Interface  (menus e HUD)
```

Cada etapa só roda se a cena tem a câmera correspondente — é isso que permite
jogos 100% 2D, 100% 3D ou **2.5D** (fundo 3D com gameplay 2D na frente).
