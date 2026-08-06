---
title: Câmera
group: Componentes
order: 2
---

# Câmera

A **câmera** define o que o jogador vê. Um jogo pode ter várias, mas a que
tiver **Principal** marcado é a usada pelo **Game View** no Play.

## Tipos

- **2D (Ortográfica)** — sem perspectiva, ideal para sprites e UI
- **3D (Perspectiva)** — profundidade real, FOV ajustável

## Propriedades

- **Primary** — se é a câmera principal (usada no Play)
- **FOV** (3D) — campo de visão (45° é o padrão)
- **Near / Far** — planos de recorte (o que fica entre eles é renderizado)
- **OrthoSize** (2D) — quanto do mundo aparece na tela

## Dicas

- **Câmera do editor** é independente das câmeras da cena — você pode voar
  pelo viewport sem mexer no jogo.
- Uma cena **2.5D** usa uma câmera perspectiva (fundo 3D) + uma câmera
  ortográfica **Principal** para a camada 2D — a engine compõe 3D → 2D → UI
  automaticamente.
- No código, veja a [API de câmera](entity.html) (FOV, clipes e posição).

::: nota
Sem câmera principal, o **Play** não renderiza nada no 3D. Se a tela ficar
vazia, adicione uma câmera e marque **Primary**.
:::
