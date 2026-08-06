---
title: Transform
group: Componentes
order: 1
---

# Transform

Todo objeto da cena tem um **Transform** — onde ele está, como está
orientado e o tamanho dele. É o primeiro componente de qualquer entidade.

## Posição

Posição no mundo (`X`, `Y`, `Z`). No modo 2D, use `X`/`Y` (e `Z` para a
ordem de desenho, se precisar).

## Rotação

Ângulos em graus em cada eixo. A convenção é **euler**: `Y` (guinada) → `X`
(inclinação) → `Z` (rolagem). Um cubo "vira" mexendo no `Y`.

## Escala

Tamanho em cada eixo. Escala negativa inverte (espelha). No 2D, `X`/`Y`
controlam o tamanho do sprite.

## Hierarquia

Se uma entidade tem **pai**, o Transform dela é **relativo ao pai**: mover o
pai move todos os filhos. Use a Hierarquia para arrastar uma entidade sobre
outra.

::: dica
No viewport, use os **gizmos** (W/E/R) para editar posição/rotação/escala com
o mouse. No código, veja as [API de transform](entity.html).
:::
