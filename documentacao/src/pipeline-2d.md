---
title: Pipeline 2D
group: Renderização
order: 2
---

# Pipeline 2D

O renderer 2D é um pipeline **dedicado em GLSL 330**, desenhado depois do 3D
e antes da UI:

```
3D → 2D → UI
```

## O que ele desenha

| Elemento | Componente | Página |
|----------|------------|--------|
| Quad com textura / cor | `SpriteRenderer` | [Sprites](sprites-2d.html) |
| Círculo (SDF, disco/anel) | `CircleRenderer` | [Sprites](sprites-2d.html) |
| Texto com fonte embutida | `TextComponent` | [Sprites](sprites-2d.html) |
| Animação por frames | `SpriteAnimation` | [Animação 2D](animacao-2d.html) |
| Grade de tiles | `Tilemap` | [Tilemap](tilemap.html) |
| Grid de referência | — | desenhado no viewport |

## Ordenação (SortingLayer)

Camadas menores desenham **atrás**. Sirva-se de valores negativos para fundos
e positivos para a frente:

```
-5  fundo
 0  mundo
 5  HUD
```

## Modo de jogo (2D)

Uma cena é "2D" quando tem **câmera primária ortográfica**. No Play, a
ordem de passes é:

1. **3D** (se houver câmera de perspectiva primária) — fundo 2.5D;
2. **2D** — sprites/texto/tilemap na frente;
3. **UI** — canvas por cima de tudo.

## Câmera 2D

A câmera ortográfica define o que aparece: `OrthoSize` é a **meia-altura** em
unidades de mundo. A largura visível acompanha a razão de aspecto da janela.

- Ver [Câmera](camera.html).

## Dicas

- Sprites com **tiling** (ex.: chão repetindo textura) — ajuste `TilingFactor`;
- **FlipX/FlipY** inverte o sprite no espaço local (vira o personagem);
- Texto usa a **fonte embutida JetBrains Mono** — não precisa de arquivo de
  fonte externo.
