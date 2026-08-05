---
title: Animação de sprite 2D
group: Componentes
order: 3
---

# Animação de sprite 2D

Anima por **frames** uma folha de sprites (sprite sheet) — os quadros são
recortados da folha por UV.

## SpriteAnimationComponent

| Campo | Descrição |
|-------|-----------|
| **SheetPath** | Caminho da folha (`.png`/`.jpg`) |
| **FramesPerRow** | Quantos quadros por linha na folha |
| **TotalFrames** | Total de quadros |
| **FPS** | Velocidade da animação |
| **Loop** | Repete ao chegar ao fim |
| **SortingLayer** | Ordenação 2D |
| **Playing** | Toca/pausa em runtime |

## Como montar uma folha

Organize os quadros em uma grade regular na imagem. Exemplo: uma folha de
**4 colunas × 2 linhas** com um personagem de 8 quadros:

```
FramesPerRow = 4
TotalFrames  = 8
FPS          = 12
```

O renderer calcula as UVs pela posição do quadro atual na grade (row-major:
quadro 0 = topo-esquerda, quadro 3 = topo-direita, quadro 4 = linha 2…).

## No editor

No Inspetor, o componente mostra a **preview da animação** no viewport — você
vê o frame atual em tempo real enquanto ajusta FPS e total de frames.

::: warn
**FramesPerRow e TotalFrames precisam bater com a imagem.** Se a folha tem
4×2 = 8 quadros, informe `FramesPerRow = 4` e `TotalFrames = 8`. Valores
errados recortam UVs erradas.
:::

## Em C#

A API C# ainda não expõe a criação da animação em runtime — monte a folha no
editor. O renderer também respeita o `SetSortingLayer` para ordenar a
animação junto com o resto da cena 2D.
