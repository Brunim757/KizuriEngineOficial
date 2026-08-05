---
title: Tilemap
group: Componentes
order: 4
---

# Tilemap

Grade de tiles apontando para um **atlas de textura** — a base de níveis de
platformer.

## TilemapComponent

| Campo | Descrição |
|-------|-----------|
| **AtlasPath** | Textura do atlas |
| **AtlasColumns / AtlasRows** | Tiles por linha/coluna no atlas |
| **MapWidth / MapHeight** | Dimensões da grade em tiles |
| **TileSize** | Tamanho de cada tile em unidades de mundo |
| **Tiles** | Índices row-major; `0` = vazio, `N` = tile N-1 do atlas |
| **SolidTileValues** | Valores de tile que geram **collider estático Box2D** no Play |
| **SortingLayer** | Ordenação 2D |

## Como funciona a grade

A grade é uma lista de índices (row-major). O índice na lista do tile `(linha,
coluna)` é `linha * MapWidth + coluna`. Cada valor:

- `0` → célula vazia (não desenha);
- `N` (1-based) → desenha o tile `N-1` da grade do atlas.

Exemplo de mapa 4×4:

```text
1 1 1 1
1 0 0 1
1 0 0 1
1 1 1 1
```

Vira a lista `[1,1,1,1, 1,0,0,1, 1,0,0,1, 1,1,1,1]` — uma caixa de paredes com
buraco no meio.

## Colisão sólida

Se `SolidTileValues` contém o valor `1`, todos os tiles `1` do mapa ganham um
**collider estático Box2D** durante o Play. Resultado: chão e paredes de
platformer sem adicionar colliders um a um.

## No editor

- Use **AtlasColumns/AtlasRows** para descrever o atlas;
- Use o campo **Tiles** para desenhar a grade (valores separados por vírgula);
- Marque os valores sólidos em **SolidTileValues**.

::: info
O tilemap renderiza no **Renderer 2D**, respeitando `SortingLayer` como os
demais elementos 2D. A colisão sólida só existe durante o **Play**.
:::
